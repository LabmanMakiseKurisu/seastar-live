/*
 * @Author: Amadeus
 * @Date: 2024-05-20 14:05:14
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-20 14:34:17
 * @FilePath: /Amadeus/src/hls/file_clean.hh
 * @Description: 
 */
#pragma once

#include <seastar/core/seastar.hh>

#include "hls/object.hh"

namespace amadeus {
namespace hls {

using namespace seastar;

class file_cleaner : public tracer {
 public:
    file_cleaner(float delete_delay_seconds);
    ~file_cleaner() override;

    virtual future<> on_update_header(fragment_info_ptr header, metadata_ptr metadata) override;

    virtual future<> on_add_playitem(fragment_info_ptr item) override;

    virtual future<> on_remove_playitem(fragment_info_ptr item) override;

    void update_delay(int delete_delay_seconds) {
        _delete_delay_seconds = delete_delay_seconds;
    }

    virtual future<> closed() override;

 private:
    void remove_useless_headers();

    playlist_t _playlist;
    float _delete_delay_seconds = -1; //延迟删除时间
};

} // namespace hls
} // namespace amadeus
