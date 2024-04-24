/*
 * @Author: Amadeus
 * @Date: 2024-04-24 15:23:57
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 17:21:34
 * @FilePath: /Amadeus/src/frame/media_base.hh
 * @Description:
 */
#pragma once
#include <seastar/core/seastar.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/util/log.hh>

#include "util/enums.hh"

namespace amadeus {

using namespace seastar;

//视频或音频帧
class media_base {
 protected:
    bool _is_video = false;    // 是否为视频帧
    bool _is_keyframe = false; // 是否为关键帧

    uint64_t _dts = 0;               // 解码时间戳
    temporary_buffer<uint8_t> _data; // 本帧数据区

 public:
    media_base() = default;
    virtual ~media_base() = default;

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

    virtual sstring to_string() const {
        return fmt::format(
            "{} is_video: {} is_keyframe: {} dts: {} media_data: {}",
            seastar::pretty_type_name(typeid(media_base)),
            _is_video,
            _is_keyframe,
            _dts,
            _data.size());
    }

    friend std::ostream &operator<<(std::ostream &os, const media_base *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const media_base &v) {
        return os << &v;
    }
};

} // namespace amadeus