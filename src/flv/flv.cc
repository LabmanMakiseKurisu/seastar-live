/*
 * @Author: Amadeus
 * @Date: 2024-04-22 11:53:36
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 11:55:17
 * @FilePath: /Amadeus/src/flv/flv.cc
 * @Description:
 */
#include "flv/flv.hh"

#include <flv-proto.h>

namespace amadeus {

namespace flv {

sstring
codec_name_by_id(int codecid) {
    switch (codecid) {
        case FLV_AUDIO_ADPCM: return "ADPCM";
        case FLV_AUDIO_AAC:
        case FLV_AUDIO_ASC: return "AAC";
        case FLV_AUDIO_MP3: return "MP3";
        case FLV_AUDIO_OPUS: return "OPUS";
        case FLV_AUDIO_G711A: return "G771A";
        case FLV_AUDIO_G711U: return "G771U";
        case FLV_AUDIO_MP3_8K: return "MP3-8K";

        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: return "H264";
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: return "H265";
        case FLV_VIDEO_AV1:
        case FLV_VIDEO_AV1C: return "AV1";
        case FLV_VIDEO_VP6: return "VP6";
        default: return "unknown";
    }
}

} // namespace flv
} // namespace amadeus