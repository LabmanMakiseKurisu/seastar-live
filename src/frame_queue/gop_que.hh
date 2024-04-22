#pragma once

#include <seastar/core/seastar.hh>

#include "frame_queue/frame_que.hh"
#include "util/enums.hh"

namespace amadeus {

using namespace seastar;

template <typename Frame>
class gop_queue_t {
 protected:
    mutable std::mutex _lock;

    float _min_cache_duration = -1; // 最小缓存时长

    Frame _first_metadata_frame;  // 第一个metadata帧
    Frame _latest_metadata_frame; // 最新metadata帧

    Frame _first_media_frame;  // 第一个media帧
    Frame _latest_media_frame; // 最新media帧

    frame_queue_t<Frame> _current;          // 当前gop
    std::deque<frame_queue_t<Frame>> _gops; // 所有gop

 public:
    gop_queue_t() {}

    virtual ~gop_queue_t() = default;

    int64_t start_dts() const {
        std::lock_guard<std::mutex> g(_lock);

        return first_media_gop().start_dts();
    }

    int64_t end_dts() const {
        std::lock_guard<std::mutex> g(_lock);

        return last_media_gop().end_dts();
    }

    float duration() const {
        std::lock_guard<std::mutex> g(_lock);

        auto start = first_media_gop().start_dts();
        auto end = last_media_gop().end_dts();

        if (start == -1 || end == -1) return 0;
        return (end - start) / 1000.f;
    }

    float current_gop_duration() const {
        std::lock_guard<std::mutex> g(_lock);
        return _current.duration();
    }

    size_t bytes() const {
        std::lock_guard<std::mutex> g(_lock);

        size_t bytes = 0;
        for (auto &gop : _gops) bytes += gop.bytes();
        bytes += _current.bytes();

        return bytes;
    }

    size_t current_gop_bytes() const {
        std::lock_guard<std::mutex> g(_lock);

        return _current.bytes();
    }

    media_type_t media_options() const {
        std::lock_guard<std::mutex> g(_lock);

        auto lmf = _latest_metadata_frame;
        if (!lmf) return media_type_t::none;

        return lmf->metadata->media_options();
    }

    Frame current_metadata_frame() const {
        std::lock_guard<std::mutex> g(_lock);

        return _latest_metadata_frame;
    }

    void set_min_cache_duration(float min_cache_duration) {
        std::lock_guard<std::mutex> g(_lock);

        assert(_min_cache_duration == -1 || min_cache_duration > 0);

        _min_cache_duration = min_cache_duration;
    }

    float min_cache_duration() const {
        std::lock_guard<std::mutex> g(_lock);

        return _min_cache_duration;
    }

    void for_each_gop(std::function<void(const frame_queue_t<Frame> &)> func) {
        std::lock_guard<std::mutex> g(_lock);

        assert((_gops.empty() && _current.empty()) || _first_metadata_frame);

        for (auto &gop : _gops) func(gop);

        func(_current);
    }

    bool add_frame(Frame frame) {
        std::lock_guard<std::mutex> g(_lock);

        bool valid = validate_frame(frame);
        if (!valid) return false;

        if (_current.only_metadata()) {
            push_frame(frame);
            return true;
        }

        bool audio_only = allow_media_type(media_type_t::audio) && !allow_media_type(media_type_t::video);

        auto require_new_gop_by_video = frame->is_media && frame->media->is_video() && frame->media->is_keyframe();
        auto require_new_gop_by_audio = frame->is_media && !frame->media->is_video() && audio_only;

        auto require_new_gop = require_new_gop_by_video || require_new_gop_by_audio;

        if (require_new_gop && _current.has_media()) _gops.push_back(std::move(_current));

        push_frame(frame);

        if (frame->is_media && _min_cache_duration > 0) update_gop(frame);

        return true;
    }

    std::vector<Frame> all_frames() const {
        std::lock_guard<std::mutex> g(_lock);

        std::vector<Frame> frames;

        for (auto &gop : _gops) frames.insert(frames.end(), gop.begin(), gop.end());

        frames.insert(frames.end(), _current.begin(), _current.end());

        return frames;
    }

    void clear() {
        std::lock_guard<std::mutex> g(_lock);

        _current.clear();
        _gops.clear();

        _first_metadata_frame = nullptr;
        _latest_metadata_frame = nullptr;

        _first_media_frame = nullptr;
        _latest_media_frame = nullptr;
    }

 protected:
    void push_frame(Frame frame) {
        _current.push_back(frame);

        on_frame(frame);
    }

    const frame_queue_t<Frame> &first_gop() const {
        return _gops.empty() ? _current : _gops.front();
    }

    frame_queue_t<Frame> &first_gop() {
        return _gops.empty() ? _current : _gops.front();
    }

    const frame_queue_t<Frame> &second_gop() const {
        if (_gops.size() > 1) return *(_gops.begin() + 1);
        if (_gops.size() == 1) return _current;

        static frame_queue_t<Frame> empty;
        return empty;
    }

    const frame_queue_t<Frame> &last_gop() const {
        if (_current.empty() && _gops.size()) return _gops.back();
        return _current;
    }

    frame_queue_t<Frame> &last_gop() {
        if (_current.empty() && _gops.size()) return _gops.back();
        return _current;
    }

    const frame_queue_t<Frame> &first_media_gop() const {
        for (auto &gop : _gops) {
            if (gop.has_media()) return gop;
        }
        return _current;
    }

    const frame_queue_t<Frame> &last_media_gop() const {
        if (_current.has_media()) return _current;

        for (auto it = _gops.rbegin(); it != _gops.rend(); ++it) {
            if (it->has_media()) return *it;
        }

        static frame_queue_t<Frame> empty;
        return empty;
    }

    bool validate_frame(Frame frame) const {
        if (frame->is_metadata) return true;

        auto media = frame->media;
        if (!allow_media_type(media->media_type())) return false;

        bool allow_audio = allow_media_type(media_type_t::audio);
        if (!media->is_video()) return allow_audio;

        if (media->is_keyframe()) return true;

        if (_current.empty()) return true;

        auto lf = _current.first_media_frame();
        return lf && lf->media->is_keyframe();
    }

    void update_gop(Frame frame) {
        if (_gops.size() < 1) return;

        auto min_duraiton = _min_cache_duration * 1000;

        auto &sg = second_gop();
        if (sg.empty()) return;

        auto sf = sg.first_media_frame();
        auto lf = _latest_media_frame;
        if (!sf || !lf) return;

        auto timeoffset = lf->media->dts() - sf->media->dts();
        if (timeoffset < min_duraiton) return;

        auto fmf = _first_metadata_frame;
        assert(fmf);

        bool requires_pop = _gops.size();
        if (requires_pop) _gops.pop_front();

        auto &nfg = first_gop();
        if (!nfg.start_with_metadata()) nfg.push_front(fmf);

        if (requires_pop) on_pop_gop();

        update_gop(frame);
    }

    bool allow_media_type(media_type_t type) const {
        auto lmf = _latest_metadata_frame;
        if (!lmf) return false;

        auto metadata = lmf->metadata;
        return metadata->is_enabled(type);
    }

 private:
    // 用frame刷新成员变量
    void on_frame(Frame frame) {
        if (_gops.empty()) {
            _first_metadata_frame = _current.first_metadata_frame();
            _latest_metadata_frame = _current.first_metadata_frame();
            _first_media_frame = _current.first_media_frame();
            _latest_media_frame = _current.latest_media_frame();
        } else {
            if (frame->is_metadata) {
                if (!_first_metadata_frame) _first_metadata_frame = frame;
                _latest_metadata_frame = frame;
            } else if (frame->is_media) {
                if (!_first_media_frame) _first_media_frame = frame;
                _latest_media_frame = frame;
            }
        }
    }

    void on_pop_gop() {
        Frame first_media_frame;
        Frame first_metadata_frame;

        for (auto &gop : _gops) {
            if (!first_media_frame && gop.has_media()) first_media_frame = gop.first_media_frame();
            if (!first_metadata_frame && gop.has_metadata()) first_metadata_frame = gop.first_metadata_frame();

            if (first_media_frame && first_metadata_frame) break;
        }
        if (!first_media_frame) first_media_frame = _current.first_media_frame();
        if (!first_metadata_frame) first_metadata_frame = _current.first_metadata_frame();

        _first_media_frame = first_media_frame;
        _first_metadata_frame = first_metadata_frame;
    }
};

} // namespace amadeus
