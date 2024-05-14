#pragma once

#include <seastar/core/seastar.hh>

namespace amadeus {

using namespace seastar;

/*
 * @description: gop的实现，能够自动管理数据和相关属性
 * Frame_ptr 派生自frame_base实现的某个具体帧的智能指针
 * @example using media_ptr = std::shared_ptr<flv::media_t>;
            using metadata_ptr = std::shared_ptr<flv::metadata_t>;
            using frame_ptr = frame_base<media_ptr, metadata_ptr>;
            using flv_frame_queue = frame_queue_t<frame_ptr>
 */
template <typename Frame_ptr>
class frame_queue_t {
 protected:
    std::vector<Frame_ptr> _queue; //数据区

    Frame_ptr _first_metadata_frame; //_queue中第一个元数据帧的智能指针
    Frame_ptr _latest_metadata_frame; // _queue中最后一个元数据帧的智能指针

    Frame_ptr _first_media_frame; //_queue中第一个媒体帧的智能指针
    Frame_ptr _latest_media_frame; //_queue中最后一个的媒体帧的智能指针
    int64_t _start_dts = -1; // 所有media帧中起始的dts
    int64_t _end_dts = -1;   // 所有media帧中末尾的dts
    uint64_t _bytes = 0;     // 所有media帧总字节大小

 public:
    frame_queue_t() = default;
    virtual ~frame_queue_t() = default;

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

    const std::vector<Frame_ptr> &operator()() const {
        return _queue;
    }
public:
    // getter
    int64_t start_dts() const {
        return _start_dts;
    }

    int64_t end_dts() const {
        return _end_dts;
    }

    uint64_t bytes() const {
        return _bytes;
    }

    Frame_ptr first_media_frame() const {
        return _first_media_frame;
    }

    Frame_ptr latest_media_frame() const {
        return _latest_media_frame;
    }

    Frame_ptr first_metadata_frame() const {
        return _first_metadata_frame;
    }

    Frame_ptr latest_metadata_frame() const {
        return _latest_metadata_frame;
    }

    float duration() const {
        if (_start_dts == -1 || _end_dts == -1) return 0;

        auto duration = _end_dts - _start_dts;
        return duration / 1000.f;
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

    Frame_ptr front() const {
        if (_queue.empty()) return nullptr;
        return _queue.front();
    }

    Frame_ptr back() const {
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

    virtual std::vector<Frame_ptr> pop_all() {
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
public:
    virtual void push_back(Frame_ptr frame) {
        assert(frame);

        if (frame->is_media) {
            if (!_first_media_frame) _first_media_frame = frame;

            _latest_media_frame = frame;

            auto dts = frame->media->dts();

            if (_start_dts == -1) _start_dts = dts;
            _end_dts = dts;

            _bytes += frame->media->size();
        } else if (frame->is_metadata) {
            //保证不会出现连续的metadata
            if (end_with_metadata()) _queue.pop_back();

            if (empty() || !_first_metadata_frame) _first_metadata_frame = frame;
            _latest_metadata_frame = frame;
        }
        _queue.push_back(frame);
    }

    virtual void push_front(Frame_ptr frame) {
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
};

//单生产者消费者队列    
template <typename Frame_ptr>
class async_frame_queue_t : public frame_queue_t<Frame_ptr> {
 public:
    using base = frame_queue_t<Frame_ptr>;
    using frames_t = std::vector<Frame_ptr>;

    async_frame_queue_t() = default;
    async_frame_queue_t(const async_frame_queue_t &) = delete;
    async_frame_queue_t(async_frame_queue_t &&) = delete;

    //生产者调用，负责完成promise并且重置_not_empty
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

    //消费者调用，负责置位_not_empty并返回future等待结果
    future<frames_t> not_empty() {
        if (_closed) return make_ready_future<frames_t>();

        _not_empty = promise<frames_t>();
        return _not_empty->get_future();
    }

 private:
    bool _closed = false;
    //用optional包装，可检查有无消费者等待
    std::optional<promise<frames_t>> _not_empty;
};

} // namespace amadeus