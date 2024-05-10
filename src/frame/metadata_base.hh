/*
 * @Author: Amadeus
 * @Date: 2024-04-24 16:04:06
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 11:34:21
 * @FilePath: /Amadeus/src/frame/metadata_base.hh
 * @Description:
 */
#pragma once
#include <seastar/core/seastar.hh>
#include <seastar/core/temporary_buffer.hh>

#include "util/enums.hh"
#include "util/status.hh"

namespace amadeus {
class meta_base {
 public:
    double bitrate = 0;
    double datarate = 0;  // kbps
    int32_t codecid = -1; // 编码方式代号

    meta_base() = default;

    meta_base(int32_t _codecid, double _bitrate, double _datarate)
    : codecid(_codecid)
    , bitrate(_bitrate)
    , datarate(_datarate) {}

    virtual ~meta_base() = default;

    bool is_enabled() const {
        return codecid >= 0 && _valid;
    }

    virtual sstring to_string() const {
        return fmt::format(
            "{} valid: {} codecid: {} bitrate: {} datarate: {}",
            seastar::pretty_type_name(typeid(meta_base)),
            _valid,
            codecid,
            bitrate,
            datarate);
    }

    friend std::ostream &operator<<(std::ostream &os, const meta_base *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const meta_base &v) {
        return os << &v;
    }

 protected:
    bool _valid = false;
};

/*
 * video_meta_t: 派生自meta_base的视频元信息类
 * audio_meta_t：派生自meta_base的音频元信息类
 */
template <typename video_meta_t, typename audio_meta_t>
class metadata_base {
 public:
    video_meta_t video;
    audio_meta_t audio;

    metadata_base() = default;
    virtual ~metadata_base() = default;

 public:
    video_meta_t *video_meta() {
        return &video;
    }

    audio_meta_t *audio_meta() {
        return &audio;
    }

    bool is_enabled() const {
        return audio.is_enabled() || video.is_enabled();
    }

    bool is_enabled(media_type_t type) const {
        auto is_video = (type & media_type_t::video) != media_type_t::none;
        if (is_video && !video.is_enabled()) return false;

        auto is_audio = (type & media_type_t::audio) != media_type_t::none;
        if (is_audio && !audio.is_enabled()) return false;

        return true;
    }

    //获取支持的媒体类型
    media_type_t media_options() const {
        media_type_t options = media_type_t::none;
        if (video.is_enabled()) options |= media_type_t::video;
        if (audio.is_enabled()) options |= media_type_t::audio;
        return options;
    }

    virtual sstring to_string() const {
        return fmt::format("{} video: {} audio: {}", seastar::pretty_type_name(typeid(metadata_base)), video, audio);
    }

    friend std::ostream &operator<<(std::ostream &os, const metadata_base *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const metadata_base &v) {
        return os << &v;
    }
};

} // namespace amadeus