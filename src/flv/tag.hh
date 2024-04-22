/*
 * @Author: Amadeus
 * @Date: 2024-04-22 11:59:17
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 15:18:12
 * @FilePath: /Amadeus/src/flv/tag.hh
 * @Description:
 */
#pragma once

#include <flv-header.h>
#include <flv-proto.h>
#include <seastar/core/seastar.hh>
#include <seastar/core/temporary_buffer.hh>

namespace amadeus {
namespace flv {

using namespace seastar;

//flv tag 类型
enum class type_t : unsigned short {
    audio = FLV_TYPE_AUDIO,
    video = FLV_TYPE_VIDEO,
    script = FLV_TYPE_SCRIPT
};

struct standard_tag {
    /*
        brief:prev_tag_size + tag header + tag_body
        prev_tag_size:本tag之前tag的大小
        type:tag类型
        tag_body:本tag数据
        timestamp：时间戳
    */
    static temporary_buffer<uint8_t>
    make_tag_data(type_t _type, temporary_buffer<uint8_t> tag_body, uint32_t prev_tag_size = 0, int64_t timestamp = 0);

    // FLV 文件头
    static temporary_buffer<uint8_t> header_data(bool allow_audio, bool allow_video);
};

struct tag {
    /*
    brief:tag header + tag_body + prev_tag_size
    type:tag类型
    tag_body:本tag数据
    timestamp：时间戳
    prev_tag_size:本tag的大小
*/
    static temporary_buffer<uint8_t>
    make_tag_data(type_t _type, temporary_buffer<uint8_t> tag_body, int64_t timestamp = 0);

    // FLV HEADER + PREVIOUS TAG SIZE(0)
    static temporary_buffer<uint8_t> header_data(bool allow_audio, bool allow_video);
};

} // namespace flv
} // namespace amadeus