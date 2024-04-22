#pragma once

#include <seastar/core/seastar.hh>

namespace amadeus {

using namespace seastar;

//gop,Frame是某种具体类型帧的指针
template <typename Frame>
class frame_queue_t {
 protected:
    std::vector<Frame> _queue;

    Frame _first_media_frame;  //_queue中第一个媒体帧
    Frame _latest_media_frame; //_queue中最新的媒体帧

    Frame _first_metadata_frame;  //_queue中第一个元数据帧
    Frame _latest_metadata_frame; //_queue中最新的元数据帧

    uint64_t _bytes = 0;     // byte
    int64_t _start_dts = -1; // ms时间戳
    int64_t _end_dts = -1;   // ms时间戳

 public:
    frame_queue_t() = default;

    frame_queue_t(frame_queue_t &&x)
    : _queue(std::move(x._queue))
    , _first_media_frame(std::move(x._first_media_frame))
    , _latest_media_frame(std::move(x._latest_media_frame))
    , _first_metadata_frame(std::move(x._first_metadata_frame))
    , _latest_metadata_frame(std::move(x._latest_metadata_frame))
    , _bytes(x._bytes)
    , _start_dts(x._start_dts)
    , _end_dts(x._end_dts) {
        x._start_dts = -1;
        x._end_dts = -1;
        x._bytes = 0;
    }

    virtual ~frame_queue_t() = default;

    int64_t start_dts() const {
        return _start_dts;
    }

    int64_t end_dts() const {
        return _end_dts;
    }

    float duration() const {
        if (_start_dts == -1 || _end_dts == -1) return 0;

        auto duration = _end_dts - _start_dts;
        return duration / 1000.f;
    }

    uint64_t bytes() const {
        return _bytes;
    }

    Frame first_media_frame() const {
        return _first_media_frame;
    }

    Frame latest_media_frame() const {
        return _latest_media_frame;
    }

    Frame first_metadata_frame() const {
        return _first_metadata_frame;
    }

    Frame latest_metadata_frame() const {
        return _latest_metadata_frame;
    }

    bool only_metadata() const {
        return size() == 1 && front()->is_metadata;
    }

    bool has_media() const {
        return _first_media_frame != nullptr;
    }

    bool has_metadata() const {
        return _first_metadata_frame != nullptr;
    }

    bool start_with_metadata() const {
        return size() && front()->is_metadata;
    }

    bool end_with_metadata() const {
        return size() && back()->is_metadata;
    }

    Frame front() const {
        if (_queue.empty()) return nullptr;
        return _queue.front();
    }

    Frame back() const {
        if (_queue.empty()) return nullptr;
        return _queue.back();
    }

    size_t size() const {
        return _queue.size();
    }

    bool empty() const {
        return _queue.empty();
    }

    auto begin() const {
        return _queue.begin();
    }

    auto end() const {
        return _queue.end();
    }

    auto rbegin() const {
        return _queue.rbegin();
    }

    auto rend() const {
        return _queue.rend();
    }

    auto cbegin() const {
        return _queue.cbegin();
    }

    auto cend() const {
        return _queue.cend();
    }

    auto crbegin() const {
        return _queue.crbegin();
    }

    auto crend() const {
        return _queue.crend();
    }

    auto operator[](size_t i) const {
        return _queue[i];
    }

    auto operator[](size_t i) {
        return _queue[i];
    }

    auto at(size_t i) const {
        return _queue.at(i);
    }

    auto at(size_t i) {
        return _queue.at(i);
    }

    virtual void push_back(Frame frame) {
        assert(frame);

        if (frame->is_media) {
            if (!_first_media_frame) _first_media_frame = frame;

            _latest_media_frame = frame;

            auto dts = frame->media->dts();

            if (_start_dts == -1) _start_dts = dts;
            _end_dts = dts;

            _bytes += frame->media->size();
        } else if (frame->is_metadata) {
            if (end_with_metadata()) _queue.pop_back();

            if (empty() || !_first_metadata_frame) _first_metadata_frame = frame;
            _latest_metadata_frame = frame;
        }
        _queue.push_back(frame);
    }

    virtual void push_front(Frame frame) {
        assert(frame);

        if (frame->is_media) {
            if (start_with_metadata()) {
                _queue.insert(_queue.begin() + 1, frame);
            } else {
                _queue.insert(_queue.begin(), frame);
            }

            _first_media_frame = frame;
            if (!_latest_media_frame) _latest_media_frame = frame;

            auto dts = frame->media->dts();

            _start_dts = dts;
            if (_end_dts == -1) _end_dts = dts;

            _bytes += frame->media->size();

        } else if (frame->is_metadata) {
            if (start_with_metadata()) _queue.erase(_queue.begin());

            _queue.insert(_queue.begin(), frame);

            _first_metadata_frame = frame;
            if (!_latest_metadata_frame) _latest_metadata_frame = frame;
        }
    }

    virtual std::vector<Frame> pop_all() {
        _first_media_frame = nullptr;
        _latest_media_frame = nullptr;
        _first_metadata_frame = nullptr;
        _latest_metadata_frame = nullptr;
        _start_dts = -1; // ms
        _end_dts = -1;   // ms
        _bytes = 0;

        return std::move(_queue);
    }

    virtual void clear() {
        _first_media_frame = nullptr;
        _latest_media_frame = nullptr;
        _first_metadata_frame = nullptr;
        _latest_metadata_frame = nullptr;
        _start_dts = -1; // ms
        _end_dts = -1;   // ms
        _bytes = 0;

        _queue.clear();
    }

    const std::vector<Frame> &operator()() const {
        return _queue;
    }

    frame_queue_t &operator=(frame_queue_t &&x) {
        _queue = std::move(x._queue);
        _first_media_frame = std::move(x._first_media_frame);
        _latest_media_frame = std::move(x._latest_media_frame);
        _first_metadata_frame = std::move(x._first_metadata_frame);
        _latest_metadata_frame = std::move(x._latest_metadata_frame);
        _start_dts = x._start_dts;
        _end_dts = x._end_dts;
        _bytes = x._bytes;

        x._start_dts = -1;
        x._end_dts = -1;
        x._bytes = 0;

        return *this;
    }
};

template <typename Frame>
class async_frame_queue_t : public frame_queue_t<Frame> {
 public:
    // base = frame_queue_t<Frame>
    using base = frame_queue_t<Frame>;
    // frames_t = std::vector<Frame>;
    using frames_t = std::vector<Frame>;

    async_frame_queue_t() = default;
    async_frame_queue_t(const async_frame_queue_t &) = delete;
    async_frame_queue_t(async_frame_queue_t &&) = delete;

    void notify_not_empty() {
        if (base::empty()) return;

        if (_not_empty) {
            _not_empty->set_value(base::pop_all());
            _not_empty = std::optional<promise<frames_t>>();
        }
    }

    void close() {
        if (_not_empty) {
            _not_empty->set_value(frames_t());
            _not_empty = std::optional<promise<frames_t>>();
        }
    }

    future<frames_t> not_empty() {
        if (_closed) return make_ready_future<frames_t>();

        _not_empty = promise<frames_t>();
        return _not_empty->get_future();
    }

 private:
    bool _closed = false;
    std::optional<promise<frames_t>> _not_empty;
};

} // namespace amadeus
