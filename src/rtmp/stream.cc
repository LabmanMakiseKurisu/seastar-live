#include "rtmp/stream.hh"

namespace amadeus {
namespace rtmp {

template <typename Consumer>
SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
future<> input_stream::consume(Consumer&& consumer) noexcept(std::is_nothrow_move_constructible_v<Consumer>) {
    return repeat([consumer = std::move(consumer), this]() mutable {
        if (_pkt.empty() && !_eof) {
            return _fd.get().then([this](packet pkt) {
                _pkt = std::move(pkt);
                _eof = _pkt.data.empty();
                return make_ready_future<stop_iteration>(stop_iteration::no);
            });
        }
        return consumer(std::move(_pkt)).then([this](consumption_result result) {
            return seastar::visit(
                result.get(),
                [this](const continue_consuming&) {
                    //处理成功，根据_eof的状态，判断是否需要继续消费
                    return make_ready_future<stop_iteration>(stop_iteration(this->_eof));
                },
                [this](stop_consuming& stop) {
                    // 遇到需要停止的情况，说明当前_pkt没有被处理，返还给流
                    this->_pkt = std::move(stop.get_packet());
                    return make_ready_future<stop_iteration>(stop_iteration::yes);
                });
        });
    });
}

template <typename Consumer>
SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
future<> input_stream::consume(Consumer& consumer) noexcept(std::is_nothrow_move_constructible_v<Consumer>) {
    return consume(std::ref(consumer));
}

//从_fd中读pkt到_pkt
future<packet>
input_stream::read() noexcept {
    if (_eof) return make_ready_future<packet>();

    return _fd.get().then([this](packet pkt) {
        _eof = pkt.empty();
        return make_ready_future<packet>(std::move(pkt));
    });
}

data_source
input_stream::detach() && {
    if (_pkt.data) { throw std::logic_error("detach() called on a used input_stream"); }

    return std::move(_fd);
}

future<>
output_stream::write(packet pkt) noexcept {
    if (!_pkts.empty() && (_pkts.size() + 1) > _size) {
        return do_with(std::move(pkt), [this](auto& pkt) {
            return flush().then([&pkt, this] {
                return put(std::move(pkt));
            });
        });
    }
    _pkts.push_back(std::move(pkt));
    return make_ready_future<>();
}

future<>
output_stream::do_flush() noexcept {
    if (_pkts.size()) {
        return do_with(std::move(_pkts), [this](auto& pkts) {
            return do_for_each(pkts, [this](auto& pkt) {
                return _fd.put(std::move(pkt)).then([this] {
                    return _fd.flush();
                });
            });
        });
    } else {
        return _fd.flush();
    }
}

future<>
output_stream::flush() noexcept {
    if (_ex) {
        // flush is a good time to deliver outstanding errors
        return make_exception_future<>(std::move(_ex));
    } else {
        return do_flush();
    }
}

future<>
output_stream::put(packet pkt) noexcept {
    return _fd.put(std::move(pkt));
}

future<>
output_stream::close() noexcept {
    return flush()
        .then([this] {
            // report final exception as close error
            if (_ex) { std::rethrow_exception(_ex); }
        })
        .finally([this] {
            return _fd.close();
        });
}

data_sink
output_stream::detach() && {
    if (_pkts.size()) throw std::logic_error("detach() called on a used output_stream");

    return std::move(_fd);
}

struct stream_copy_consumer {
 private:
    output_stream& _os;
    using unconsumed_remainder = std::optional<packet>;

 public:
    stream_copy_consumer(output_stream& os)
    : _os(os) {}

    future<unconsumed_remainder> operator()(packet pkt) {
        if (pkt.empty()) return make_ready_future<unconsumed_remainder>();

        return _os.write(std::move(pkt)).then([]() {
            return make_ready_future<unconsumed_remainder>();
        });
    }
};

future<>
copy(input_stream& in, output_stream out) {
    return in.consume(stream_copy_consumer(out));
}

extern future<> copy(input_stream&, output_stream&);

struct stream_skip_consumer {
 private:
    using unconsumed_remainder = std::optional<packet>;

 public:
    stream_skip_consumer() = default;

    future<unconsumed_remainder> operator()(packet pkt) {
        return make_ready_future<unconsumed_remainder>();
    }
};

future<>
skip(input_stream& in) {
    return in.consume(stream_skip_consumer());
}

extern future<> skip(input_stream&);

} // namespace rtmp
} // namespace amadeus
