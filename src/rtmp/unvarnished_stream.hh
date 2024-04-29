/*
 * @Author: Amadeus
 * @Date: 2024-04-23 11:54:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-29 16:24:10
 * @FilePath: /Amadeus/src/rtmp/unvarnished_stream.hh
 * @Description:
 */
#include <seastar/http/internal/content_source.hh>

namespace seastar {

namespace httpd {

namespace internal {

//未经处理的数据源，即不对数据进行任何加工或修改，直接将数据传递给下游
class unvarnished_data_source_impl : public data_source_impl {
    data_source _source;
    noncopyable_function<void(size_t bytes)> _func;

 public:
    unvarnished_data_source_impl(data_source source, noncopyable_function<void(size_t bytes)> func)
    : _source(std::move(source))
    , _func(std::move(func)) {}

    virtual future<temporary_buffer<char>> get() override {
        return _source.get().then([this](auto buf) {
            _func(buf.size());
            return make_ready_future<temporary_buffer<char>>(std::move(buf));
        });
    }

    virtual future<temporary_buffer<char>> skip(uint64_t n) override {
        return _source.skip(n);
    }

    virtual future<> close() override {
        return _source.close();
    }
};

//未经处理的数据汇，即不对数据进行任何加工或修改，直接将数据传递给上游
class unvarnished_data_sink_impl : public data_sink_impl {
    data_sink _sink;
    noncopyable_function<void(size_t bytes)> _func;

 public:
    unvarnished_data_sink_impl(data_sink sink, noncopyable_function<void(size_t bytes)> func)
    : _sink(std::move(sink))
    , _func(std::move(func)) {}

    virtual future<> put(net::packet data) override {
        _func(data.len());
        return _sink.put(std::move(data));
    }

    virtual future<> flush() override {
        return _sink.flush();
    }

    virtual future<> close() override {
        return _sink.close();
    }

    virtual size_t buffer_size() const noexcept {
        return _sink.buffer_size();
    }
};

} // namespace internal
} // namespace httpd
} // namespace seastar