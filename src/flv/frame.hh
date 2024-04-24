/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:41:38
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 14:14:12
 * @FilePath: /Amadeus/src/flv/frame.hh
 * @Description:
 */

#pragma once

#include "flv/media.hh"
#include "flv/metadata.hh"
#include "frame/frame_base.hh"

namespace amadeus {
namespace flv {
using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;
using script_ptr = std::shared_ptr<flv::script_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using Frame = amadeus::frame_base<media_ptr, metadata_ptr>;

class frame_t : public Frame {
public:
    frame_t() = default;

    frame_t(media_ptr f)
    : Frame(f) {}

    frame_t(metadata_ptr m)
    : Frame(m) {}

    frame_t(script_ptr s)
    : script(s)
    , is_script(true) {
        assert(s);
    }

    bool operator==(const frame_t &other) const {
        return (is_script && other.is_script && script == other.script)
            || (!is_script && !other.is_script && Frame::operator==(other));
    }

    bool operator!=(const frame_t &other) const {
        return !(*this == other);
    }

    script_ptr script = nullptr;
    bool is_script = false;

    friend std::ostream &operator<<(std::ostream &os, const frame_t *x) {
        if (x->is_media) os << x->media;
        if (x->is_metadata) os << x->metadata;
        if (x->is_script) os << x->script;

        return os;
    }
};

using frame_ptr = std::shared_ptr<frame_t>;
} // namespace flv
} // namespace amadeus