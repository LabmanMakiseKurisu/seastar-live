/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-08 16:55:53
 * @FilePath: /Amadeus/src/rtmp/packet.hh
 * @Description:
 */
#pragma once

#include <seastar/core/temporary_buffer.hh>

#include "util/enums.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

struct packet {
    enum type_t {
        video,
        audio,
        script,
    } type = video;//tag的类型

    temporary_buffer<uint8_t> data; //tag data
    int64_t dts = 0;

    packet()
    : data(temporary_buffer<uint8_t>(0)) {}

    packet(type_t t, temporary_buffer<uint8_t> d, int64_t ts)
    : type(t)
    , data(std::move(d))
    , dts(ts) {}

    packet(packet&&) = default;
    packet& operator=(packet&&) = default;

    auto empty() const {
        return data.empty();
    }

    auto size() const {
        return data.size();
    }

    bool is_media() const {
        return type != packet::script;
    }

    static packet make_video(temporary_buffer<uint8_t> data, int64_t dts) {
        return packet(type_t::video, std::move(data), dts);
    }

    static packet make_audio(temporary_buffer<uint8_t> data, int64_t dts) {
        return packet(type_t::audio, std::move(data), dts);
    }

    static packet make_script(temporary_buffer<uint8_t> data, int64_t dts) {
        return packet(type_t::script, std::move(data), dts);
    }
};

static inline media_type_t
to_media_type(packet::type_t type) {
    switch (type) {
        case packet::type_t::video: return media_type_t::video;
        case packet::type_t::audio: return media_type_t::audio;
        default: return media_type_t::none;
    }
}
} // namespace rtmp
} // namespace amadeus
