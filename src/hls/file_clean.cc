/*
 * @Author: Amadeus
 * @Date: 2024-05-20 14:05:14
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-20 14:33:22
 * @FilePath: /Amadeus/src/hls/file_clean.cc
 * @Description:
 */
#include "hls/file_clean.hh"

#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/thread.hh>
#include <seastar/util/file.hh>
#include <seastar/util/log.hh>
#include <sys/stat.h>

#include <chrono>
#include <cstdio>

#include "hls/log.hh"
#include "hls/object.hh"
#include "util/util.hh"

namespace amadeus {
namespace hls {

using namespace seastar;
using namespace std::chrono;
using namespace std::chrono_literals;


file_cleaner::file_cleaner(float delete_delay_seconds)
: _delete_delay_seconds(delete_delay_seconds) {}

file_cleaner::~file_cleaner() {}

//向_playlist中添加item
future<>
file_cleaner::on_add_playitem(fragment_info_ptr item) {
    if (item) _playlist.push_back(item);

    return make_ready_future<>();
}

//删除指定的item
future<>
file_cleaner::on_remove_playitem(fragment_info_ptr item) {
    if (item) {
        auto it = std::find(_playlist.begin(), _playlist.end(), item);
        if (it != _playlist.end()) _playlist.erase(it);

        (void)remove_file_delay(item->filepath, _delete_delay_seconds);
        remove_useless_headers();
    }
    return make_ready_future<>();
}

//更新header item，删除其之前的
future<>
file_cleaner::on_update_header(fragment_info_ptr header, metadata_ptr metadata) {
    if (header) {
        _playlist.push_back(header);
        remove_useless_headers();
    }
    return make_ready_future<>();
}

//移除多余的header item，只保留最前新的
void
file_cleaner::remove_useless_headers() {
    if (_playlist.size() <= 1) return;

    auto second = _playlist.begin() + 1;
    for (auto it = second; it != _playlist.end(); it++) {
        if ((*it)->is_header) continue;

        if (it != second) {
            auto begin = _playlist.begin();
            auto end = it - 1;

            for (auto itt = begin; itt != end; itt++) (void)remove_file_delay((*itt)->filepath, _delete_delay_seconds);
            _playlist.erase(begin, end); //左闭又开的删除
        }
        break;
    }
}

future<>
file_cleaner::closed() {
    return make_ready_future();
}

} // namespace hls
} // namespace amadeus
