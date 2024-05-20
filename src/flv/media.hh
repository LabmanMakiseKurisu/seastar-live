/*
 * @Author: Amadeus
 * @Date: 2024-04-22 12:57:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-20 17:33:31
 * @FilePath: /Amadeus/src/flv/media.hh
 * @Description:
 */
#pragma once

#include <flv-header.h>
#include "flv/tag.hh"
#include "frame/media_base.hh"
#include "util/enums.hh"

namespace amadeus {
namespace flv {

using namespace seastar;

struct audio_media_t;
struct video_media_t;
struct metadata_t;

// flv 媒体帧基类
struct media_t : public media_base {
 public:
    media_t() = default;
    virtual ~media_t() = default;

    type_t type() {
        return _is_video ? type_t::video : type_t::audio;
    }

    // 媒体帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const = 0;

    // 媒体帧数据
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const = 0;

    uint64_t cts() const {
        return _cts;
    }
 protected:
    uint64_t _cts = 0;
};

struct audio_media_t final : public media_t {
 protected:
 
 public:
    audio_media_t() = default;
    audio_media_t(uint64_t dts, temporary_buffer<uint8_t> data);

    virtual ~audio_media_t() = default;

    // 音频帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const override;

    // 音频帧附加头(PacketType==1) + data
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const override;

    virtual sstring to_string() const override;
};

struct video_media_t final : public media_t {
 protected:
    //uint64_t _cts = 0;

 public:
    video_media_t() = default;
    video_media_t(int keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data);
    video_media_t(bool is_keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data);

    virtual ~video_media_t() = default;

    // 视频帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const override;

    // 视频帧附加头(PacketType==1) + data
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const override;

    // uint64_t cts() const {
    //     return _cts;
    // }

    virtual sstring to_string() const override;
};

} // namespace flv
} // namespace amadeus
