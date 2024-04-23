/*
 * @Author: Amadeus
 * @Date: 2024-04-23 11:54:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 11:54:36
 * @FilePath: /Amadeus/src/rtmp/data_sink.hh
 * @Description: 
 */
#include <seastar/http/internal/content_source.hh>

namespace seastar {

namespace httpd {

namespace internal {


class unvarnished_data_source_impl : public data_source_impl {
    data_source _source;
    noncopyable_function<void(size_t bytes)> _func;
 public:
    unvarnished_data_source_impl(data_source source, noncopyable_function<void(size_t bytes)> func)
    : _source(std::move(source))
    , _func(std::move(func)) {}
    
    virtual future<temporary_buffer<char>> get() override {
        return _source.get().then([this] (auto buf) {
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

}}}