/*
 * @Author: Amadeus
 * @Date: 2024-05-20 14:05:14
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-22 16:46:07
 * @FilePath: /Amadeus/src/hls/m3u8.hh
 * @Description: 
 */
#pragma once

#include <seastar/core/seastar.hh>
#include <seastar/core/shared_mutex.hh>

#include "hls/object.hh"

namespace amadeus {
namespace hls {

using namespace seastar;

namespace fs = std::filesystem;

class m3u8_writer : public tracer {
 public:
    m3u8_writer(const sstring &directroy, bool save_timestamp, float delete_delay_seconds);
    virtual ~m3u8_writer() = default;
    
    //更新playlist的header
    virtual future<> on_update_header(fragment_info_ptr header, metadata_ptr metadata) override;
    
    //将item放入playlist并且生成新的m3u8文件
    virtual future<> on_add_playitem(fragment_info_ptr item) override;
    
    //将item从playlist中移除，并生成新的m3u8文件
    virtual future<> on_remove_playitem(fragment_info_ptr item) override;

    future<> closed() override;

    const fs::path &filepath() const {
        return _filepath;
    }

 protected:
   //根据playlist和end生产.m3u8文件的字符串内容
    virtual sstring make_content(const playlist_t &playlist, const sstring &end = "") = 0;

    //对单个item生产其m3u8文件的字符串内容
    virtual sstring make_playitem_content(fragment_info_ptr frag) = 0;

    future<> make_file(const sstring &end = "");
    future<> write_playlist_files(sstring content);
    future<> write_file(fs::path filepath, sstring content);

    void remove_useless_headers();

    playlist_t _playlist;

    fs::path _directory; //m3u8文件所在目录
    fs::path _filepath; //m3u8文件路径

    bool _save_timestamp = false;
    int64_t _next_timstamp = -1;
    float _delete_delay_seconds = -1;
};

class m3u8_writer_v3 : public m3u8_writer {
 public:
    m3u8_writer_v3(const sstring &directroy, bool save_timestamp, float delete_delay_seconds);
    virtual ~m3u8_writer_v3() override = default;

 protected:
    virtual sstring make_content(const playlist_t &playlist, const sstring &end = "") override;
    virtual sstring make_playitem_content(fragment_info_ptr frag) override;
};


} // namespace hls
} // namespace amadeus
