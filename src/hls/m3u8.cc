#include "hls/m3u8.hh"

#include <boost/range/irange.hpp>
#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/thread.hh>
#include <seastar/util/log.hh>

#include "hls/log.hh"
#include "hls/m3u8.hh"
#include "util/util.hh"

namespace amadeus {
namespace hls {

using namespace seastar;

namespace fs = fs;

m3u8_writer::m3u8_writer(const sstring &directroy, bool save_timestamp, float delete_delay_seconds)
: _directory(directroy)
, _save_timestamp(save_timestamp)
, _delete_delay_seconds(delete_delay_seconds) {}

future<>
m3u8_writer::on_add_playitem(fragment_info_ptr item) {
    if (!item) return make_ready_future<>();

    _playlist.push_back(item);

    return make_file();
}

future<>
m3u8_writer::on_remove_playitem(fragment_info_ptr item) {
    if (!item) return make_ready_future<>();

    auto it = std::find(_playlist.begin(), _playlist.end(), item);
    if (it != _playlist.end()) _playlist.erase(it);

    remove_useless_headers();
    return make_file();
}

future<>
m3u8_writer::on_update_header(fragment_info_ptr header, metadata_ptr metadata) {
    if (header) {
        _playlist.push_back(header);
        remove_useless_headers();
    }

    return make_ready_future<>();
}

void
m3u8_writer::remove_useless_headers() {
    if (_playlist.size() <= 1) return;

    auto second = _playlist.begin() + 1;
    for (auto it = second; it != _playlist.end(); it++) {
        if ((*it)->is_header) continue;
        if (it != second) _playlist.erase(_playlist.begin(), it - 1);
        break;
    }
}

//用_playlist生成.m3u8文件
future<>
m3u8_writer::make_file(const sstring &end) {
    auto content = make_content(_playlist, end);
    return write_playlist_files(std::move(content));
}

//用content来原子的更新m3u8文件
future<>
m3u8_writer::write_playlist_files(sstring content) {
    return write_file(_filepath, content).then([this, content] {
        if (!_save_timestamp) return make_ready_future<>();

        using namespace std::chrono;
        int64_t current_ts = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

        if (_next_timstamp == -1) _next_timstamp = current_ts;

        return parallel_for_each(
                   boost::irange(_next_timstamp, current_ts + 1),
                   [this, content](auto ts) {
                       auto filepath = _directory / fs::path(to_sstring(ts) + ".m3u8");

                       return write_file(filepath, content).then([this, filepath] {
                           if (_delete_delay_seconds > 0) (void)remove_file_delay(filepath, _delete_delay_seconds);
                       });
                   })
            .then([this, current_ts] {
                _next_timstamp = current_ts + 1;
            });
    });
}

//原子的将content写入filepath，先写入到临时文件再替换原文件
future<>
m3u8_writer::write_file(fs::path filepath, sstring content) {
    auto tmp_file = filepath;
    tmp_file += ".bak";

    int fd = ::open(
        tmp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH | S_IWOTH);
    if (fd < 0) return make_exception_future<>(std::system_error(errno, std::system_category(), "open"));

    int st = ::ftruncate(fd, content.size());
    if (st >= 0) {
        st = ::write(fd, content.c_str(), content.size());
        if (st >= 0) {
            ::close(fd);
            assert(fs::exists(tmp_file));

            st = ::rename(tmp_file.c_str(), filepath.c_str()); //::rename是原子操作
            if (st >= 0) {
                return make_ready_future<>();
            } else {
                return make_exception_future<>(std::system_error(st, std::system_category(), "rename"));
            }
        } else {
            ::close(fd);

            return make_exception_future<>(std::system_error(st, std::system_category(), "write"));
        }
    } else {
        st = ::close(fd);

        return make_exception_future<>(std::system_error(st, std::system_category(), "ftruncate"));
    }
}

future<>
m3u8_writer::closed() {
    if (_playlist.empty()) return make_ready_future();

    return make_file("#EXT-X-ENDLIST\n");
}

m3u8_writer_v3::m3u8_writer_v3(const sstring &directroy, bool save_timestamp, float delete_delay_seconds)
: m3u8_writer(directroy, save_timestamp, delete_delay_seconds) {
    _filepath = directroy + "/ts_index.m3u8";
}

//根据playlist和end生产.m3u8文件的字符串内容
sstring
m3u8_writer_v3::make_content(const playlist_t &playlist, const sstring &end) {
    if (playlist.empty()) return "";

    std::string content;

    auto media_sequence = playlist.front()->id;

    int64_t max_duration = 0;
    for (auto &frag : playlist) max_duration = std::max(max_duration, frag->duration);

    content.append("#EXTM3U\n");
    content.append("#EXT-X-VERSION:3\n");
    content.append("#EXT-X-START:TIME-OFFSET=0\n");
    content.append("#EXT-X-MEDIA-SEQUENCE:" + std::to_string(media_sequence) + '\n');
    content.append("#EXT-X-TARGETDURATION:" + std::to_string(static_cast<int>(max_duration / 1000.f + 0.5)) + '\n');

    for (size_t i = 0; i < playlist.size(); i++) {
        auto &item = playlist[i];
        if (item->is_header) {
            if (i != 0 && i != (playlist.size() - 1)) content.append("#EXT-X-DISCONTINUITY\n");
        } else {
            content.append(make_playitem_content(item));
        }
    }
    content.append(end);
    return content;
}

//对单个item生产其m3u8文件的字符串内容
sstring
m3u8_writer_v3::make_playitem_content(fragment_info_ptr item) {
    std::string content;

    auto filename = item->filepath.filename().string();
    auto duration = fmt::format("{:.{}f}", item->duration / 1000.00f, 2);

    // the EXTINF line
    content.append("#EXTINF:" + duration + "\n");
    content.append(filename + '\n');

    return content;
}


} // namespace hls
} // namespace bilibili
