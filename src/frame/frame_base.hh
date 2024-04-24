/*
 * @Author: Amadeus
 * @Date: 2024-04-22 10:52:34
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 16:44:04
 * @FilePath: /Amadeus/src/frame/frame_base.hh
 * @Description:
 */
#pragma once

#include <assert.h>
#include <seastar/core/sstring.hh>
#include <seastar/util/log.hh>

#include <cstring>
#include <deque>
#include <memory>

#include "util/enums.hh"

namespace amadeus {

using namespace seastar;

/**
 * @description: 帧的基类，可能是媒体帧，也可能是元数据
 * Media_ptr 派生自media_base的类的智能指针
 * Metadata_ptr 派生自metadata_base的类的智能指针
 * @example using media_ptr = std::shared_ptr<flv::media_t>;
            using metadata_ptr = std::shared_ptr<flv::metadata_t>;
            using flv_frame = frame_base<media_ptr, metadata_ptr>;
 */
template <typename Media_ptr, typename Metadata_ptr>
class frame_base {
 public:
    bool is_media = false;
    bool is_metadata = false;

    Media_ptr media;
    Metadata_ptr metadata;

 public:
    frame_base() = default;

    frame_base(Media_ptr m)
    : is_media(true)
    , media(m) {
        assert(m);
    }

    frame_base(Metadata_ptr m)
    : is_metadata(true)
    , metadata(m) {
        assert(m);
    }

    bool operator==(const frame_base &x) const {
        if (is_media && x.is_media) return media == x.media;
        if (is_metadata && x.is_metadata) return metadata == x.metadata;
        return false;
    }

    bool operator!=(const frame_base &x) const {
        return !(*this == x);
    }

    friend std::ostream &operator<<(std::ostream &os, const frame_base &x) {
        if (x.is_media) os << x.media;
        if (x.is_metadata) os << x.metadata;

        return os;
    }

    friend std::ostream &operator<<(std::ostream &os, const frame_base *x) {
        if (x->is_media) os << x->media;
        if (x->is_metadata) os << x->metadata;

        return os;
    }
};

} // namespace amadeus
