/*
 * @Author: Amadeus
 * @Date: 2024-04-22 11:53:24
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 11:58:22
 * @FilePath: /Amadeus/src/flv/flv.hh
 * @Description:
 */
#pragma once

#include <seastar/core/sstring.hh>

namespace amadeus {
namespace flv {

using namespace seastar;

#define HEADER_ID_NULL            LONG_MIN

#define FLV_HEADER_SIZE           9 //FLV文件头字节数
#define FLV_TAG_HEADER_SIZE       11 //FLV每个Tag的头字节数
#define FLV_AUDIO_TAG_HEADER_SIZE 2 //AAC audio Tag附加头字节数
#define FLV_VIDEO_TAG_HEADER_SIZE 5 //AVC video Tag附加头字节数

sstring codec_name_by_id(int codec); //编码方式转字符串

} // namespace flv
} // namespace amadeus
