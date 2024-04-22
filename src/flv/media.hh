#pragma once

#include <flv-header.h>
#include <seastar/core/seastar.hh>
#include <seastar/core/temporary_buffer.hh>

#include "flv/tag.hh"
#include "util/enums.hh"

namespace amadeus {
namespace flv {

using namespace seastar;

struct audio_media_t;
struct video_media_t;
struct metadata_t;

//flv 媒体帧基类
struct media_t {
 protected:
    bool _is_video = false; //是否为视频帧
    bool _is_keyframe = false; //是否为关键帧

    uint64_t _dts = 0; //解码时间戳
    temporary_buffer<uint8_t> _data; //本帧数据区

 public:
    virtual ~media_t() = default;

    //媒体帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const = 0;

    //媒体帧数据
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const = 0;

    type_t type() {
        return _is_video ? type_t::video : type_t::audio;
    }

    media_type_t media_type() {
        return _is_video ? media_type_t::video : media_type_t::audio;
    }

    bool is_video() const {
        return _is_video;
    }

    bool is_keyframe() const {
        return _is_keyframe;
    }

    int64_t dts() const {
        return _dts;
    }

    void set_dts(int64_t dts) {
        _dts = dts;
    }

    size_t size() const {
        return _data.size();
    }

    temporary_buffer<uint8_t> &data() {
        return _data;
    }

    const temporary_buffer<uint8_t> &data() const {
        return _data;
    }

    virtual sstring to_string() const;

    friend std::ostream &operator<<(std::ostream &os, const media_t *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const media_t &v) {
        return os << &v;
    }
};

struct audio_media_t final : public media_t {
 protected:

 public:
    audio_media_t() = default;
    audio_media_t(uint64_t dts, temporary_buffer<uint8_t> data);

    virtual ~audio_media_t() = default;

    //音频帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const override;

    //音频帧附加头(PacketType==1) + data
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const override;

    virtual sstring to_string() const override;
};

struct video_media_t final : public media_t {
 protected:
    uint64_t _cts = 0;

 public:
    video_media_t() = default;
    video_media_t(int keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data);
    video_media_t(bool is_keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data);

    virtual ~video_media_t() = default;

    //视频帧附加头(PacketType==1)
    virtual temporary_buffer<uint8_t> tag_header(metadata_t *metadata) const override;

    // 视频帧附加头(PacketType==1) + data
    virtual temporary_buffer<uint8_t> to_tag_data(metadata_t *metadata) const override;

    uint64_t cts() const {
        return _cts;
    }

    virtual sstring to_string() const override;
};

} // namespace flv
} // namespace amadeus
