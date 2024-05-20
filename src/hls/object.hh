#pragma once

#include <seastar/core/seastar.hh>
#include <seastar/core/sleep.hh>

#include <deque>

#include "flv/frame.hh"

namespace amadeus {
namespace hls {
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using namespace seastar;

//单个.ts或.m4s切片文件的信息
struct fragment_info {
    int64_t id;
    int64_t pts;
    int64_t duration; //
    bool start_by_keyframe;
    size_t size;
    uint32_t crc32;
    bool is_header;
    int64_t header_id;
    std::filesystem::path filepath; //存放路径
    int64_t ctime; // unix timestamp
};

using fragment_info_ptr = std::shared_ptr<fragment_info>;

//多个切片文件的信息数组
using playlist_t = std::vector<fragment_info_ptr>;

class tracer {
 public:
    tracer() = default;
    virtual ~tracer() = default;

    virtual future<> on_update_header(fragment_info_ptr header, metadata_ptr metadata) {
        return make_ready_future<>();
    }

    virtual future<> on_add_playitem(fragment_info_ptr item) {
        return make_ready_future<>();
    }

    virtual future<> on_remove_playitem(fragment_info_ptr item) {
        return make_ready_future<>();
    }

    virtual future<> on_update_playlist(const playlist_t &playlist) {
        return make_ready_future<>();
    }

    virtual future<> closed() = 0;

    //延迟删除文件
    static inline future<> remove_file_delay(std::filesystem::path filepath, float delay) {
        auto ms = static_cast<int>(delay * 1000);
        return seastar::sleep(std::chrono::milliseconds(ms)).then([filepath] {
            return file_exists(filepath.native())
                .then([filepath](auto exists) {
                    if (exists) return seastar::remove_file(filepath.native());
                    return make_ready_future<>();
                })
                .handle_exception([](auto e) {
                    // do nothing
                });
        });
    }
};

enum class version_t : unsigned int {
    none = 0,
    v3 = 1 << 0,
    v7 = 1 << 2,

    all = 0xFFFF
};

template <class T>
static inline version_t
convert_to_version(const T &_v) {
    switch (_v) {
        case T::v3: return version_t::v3;
        case T::v7: return version_t::v7;
        case T::all: return version_t::all;
        default: return version_t::all;
    }
}

} // namespace hls

inline hls::version_t
operator|(hls::version_t a, hls::version_t b) {
    return hls::version_t(std::underlying_type_t<hls::version_t>(a) | std::underlying_type_t<hls::version_t>(b));
}

inline void
operator|=(hls::version_t &a, hls::version_t b) {
    a = (a | b);
}

inline hls::version_t
operator&(hls::version_t a, hls::version_t b) {
    return hls::version_t(std::underlying_type_t<hls::version_t>(a) & std::underlying_type_t<hls::version_t>(b));
}

inline void
operator&=(hls::version_t &a, hls::version_t b) {
    a = (a & b);
}

} // namespace amadeus
