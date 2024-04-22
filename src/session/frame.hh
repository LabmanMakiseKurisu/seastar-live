/*
 * @Author: Amadeus
 * @Date: 2024-04-22 10:52:34
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 18:26:53
 * @FilePath: /Amadeus/src/session/frame.hh
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
namespace session {

using namespace seastar;


// 帧的基类，可能是媒体帧，也可能是元数据
template <typename Mediadata, typename Metadata>
struct frame_t  {
 public:
    bool is_media = false;
    bool is_metadata = false;

    Mediadata media;
    Metadata metadata;

 public:
    frame_t() = default;

    frame_t(Mediadata m)
    : is_media(true)
    , media(m) {
        assert(m);
    }

    frame_t(Metadata m)
    : is_metadata(true)
    , metadata(m) {
        assert(m);
    }

    bool operator==(const frame_t &x) const {
        if (is_media && x.is_media) return media == x.media;
        if (is_metadata && x.is_metadata) return metadata == x.metadata;
        return false;
    }

    bool operator!=(const frame_t &x) const {
        return !(*this == x);
    }

    friend std::ostream &operator<<(std::ostream &os, const frame_t &x) {
        if (x.is_media) os << x.media;
        if (x.is_metadata) os << x.metadata;

        return os;
    }

    friend std::ostream &operator<<(std::ostream &os, const frame_t *x) {
        if (x->is_media) os << x->media;
        if (x->is_metadata) os << x->metadata;

        return os;
    }
};

} // namespace session
} // namespace amadeus