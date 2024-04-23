/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http3://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2023 bilibili
 */

#pragma once

#include <seastar/core/iostream.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/queue.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/util/concepts.hh>
#include <deque>

#include "rtmp/exception.hh"
#include "rtmp/packet.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

class data_source_impl {
 public:
    virtual ~data_source_impl() {}

    virtual future<packet> get() = 0;

    virtual future<> close() {
        return make_ready_future<>();
    }
};

class data_source {
    std::unique_ptr<data_source_impl> _dsi;

 protected:
    data_source_impl* impl() const {
        return _dsi.get();
    }

 public:
    data_source() noexcept = default;

    explicit data_source(std::unique_ptr<data_source_impl> dsi) noexcept
    : _dsi(std::move(dsi)) {}

    data_source(data_source&& x) noexcept = default;
    data_source& operator=(data_source&& x) noexcept = default;

    future<packet> get() noexcept {
        try {
            return _dsi->get();
        } catch (...) { return current_exception_as_future<packet>(); }
    }

    future<> close() noexcept {
        try {
            return _dsi->close();
        } catch (...) { return current_exception_as_future<>(); }
    }
};

class data_sink_impl {
 public:
    virtual ~data_sink_impl() {}

    virtual future<> put(std::vector<packet> pkts) {
        return do_with(std::move(pkts), [this](auto& pkts) {
            return do_for_each(pkts, [this](auto& pkt) {
                return put(std::move(pkt));
            });
        });
    }

    virtual future<> put(packet pkt) = 0;

    virtual future<> flush() {
        return make_ready_future<>();
    }

    virtual future<> close() = 0;
};

class data_sink {
    std::unique_ptr<data_sink_impl> _dsi;

 public:
    data_sink() noexcept = default;

    explicit data_sink(std::unique_ptr<data_sink_impl> dsi) noexcept
    : _dsi(std::move(dsi)) {}

    data_sink(data_sink&& x) noexcept = default;
    data_sink& operator=(data_sink&& x) noexcept = default;

    future<> put(std::vector<packet> data) noexcept {
        try {
            return _dsi->put(std::move(data));
        } catch (...) { return current_exception_as_future(); }
    }

    future<> put(packet data) noexcept {
        try {
            return _dsi->put(std::move(data));
        } catch (...) { return current_exception_as_future(); }
    }

    future<> flush() noexcept {
        try {
            return _dsi->flush();
        } catch (...) { return current_exception_as_future(); }
    }

    future<> close() noexcept {
        try {
            return _dsi->close();
        } catch (...) { return current_exception_as_future(); }
    }
};

struct continue_consuming {};

class stop_consuming {
 public:
    explicit stop_consuming(packet pkt)
    : _pkt(std::move(pkt)) {}

    packet& get_packet() {
        return _pkt;
    }

    const packet& get_packet() const {
        return _pkt;
    }

 private:
    packet _pkt;
};

class consumption_result {
 public:
    using consumption_variant = std::variant<continue_consuming, stop_consuming>;

    consumption_result(std::optional<packet> opt_pkt) {
        if (opt_pkt) { _result = stop_consuming(std::move(opt_pkt.value())); }
    }

    consumption_result(const continue_consuming&) {}

    consumption_result(stop_consuming&& stop)
    : _result(std::move(stop)) {}

    consumption_variant& get() {
        return _result;
    }

    const consumption_variant& get() const {
        return _result;
    }

 private:
    consumption_variant _result;
};

// Consumer concept, for consume() method
SEASTAR_CONCEPT(
    template <typename Consumer> concept InputStreamConsumer =
        requires (Consumer c) {
            { c(packet()) } -> std::same_as<future<consumption_result>>;
        };

    template <typename Consumer> concept ObsoleteInputStreamConsumer =
        requires (Consumer c) {
            { c(packet()) } -> std::same_as<future<std::optional<packet>>>;
        };

)

class input_stream final {
    data_source _fd;
    packet _pkt;
    bool _eof = false;

 private:
    size_t available() const noexcept {
        return _pkt.data.size();
    }

 protected:
    void reset() noexcept {
        _pkt = {};
    }

    data_source* fd() noexcept {
        return &_fd;
    }

 public:
    input_stream() noexcept = default;

    explicit input_stream(data_source fd) noexcept
    : _fd(std::move(fd))
    , _pkt() {}

    input_stream(input_stream&&) = default;
    input_stream& operator=(input_stream&&) = default;

    template <typename Consumer>
    SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
    future<> consume(Consumer&& c) noexcept(std::is_nothrow_move_constructible_v<Consumer>);
    template <typename Consumer>
    SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
    future<> consume(Consumer& c) noexcept(std::is_nothrow_move_constructible_v<Consumer>);

    bool eof() const noexcept {
        return _eof;
    }

    future<packet> read() noexcept;

    future<> close() noexcept {
        return _fd.close();
    }

    data_source detach() &&;
};

class output_stream final {
    data_sink _fd;
    std::deque<packet> _pkts;

    size_t _size = 0;

    std::exception_ptr _ex;

 private:
    future<> put(packet pkt) noexcept;
    future<> do_flush() noexcept;

 public:
    output_stream() noexcept = default;

    output_stream(data_sink fd, size_t size) noexcept
    : _fd(std::move(fd))
    , _size(size) {}

    output_stream(data_sink fd) noexcept
    : _fd(std::move(fd)) {}

    output_stream(output_stream&&) noexcept = default;
    output_stream& operator=(output_stream&&) noexcept = default;

    ~output_stream() {
        assert(_pkts.empty() && "Was this stream properly closed?");
    }

    future<> write(packet) noexcept;
    future<> flush() noexcept;

    future<> close() noexcept;

    data_sink detach() &&;
};

future<> copy(input_stream&, output_stream&);
future<> skip(input_stream&);

class media_data_source_impl : public data_source_impl {
    seastar::queue<packet>& _data;
    bool _closed = false;

 public:
    media_data_source_impl(seastar::queue<packet>& data)
    : _data(data) {}

    virtual future<packet> get() override {
        if (_closed) return make_ready_future<packet>(packet());
        return _data.pop_eventually();
    }

    virtual future<> close() override {
        if (_closed) return make_ready_future();
        _closed = true;

        return _data.push_eventually(packet());
    }
};

class media_data_sink_impl : public data_sink_impl {
    seastar::queue<packet>& _data;
    bool _closed = false;

 public:
    media_data_sink_impl(seastar::queue<packet>& data)
    : _data(data) {}

    virtual future<> put(packet pkt) override {
        // if (_closed) return make_exception_future(std::system_error(ENOTCONN, std::system_category()));
        return _data.push_eventually(std::move(pkt));
    }

    virtual future<> close() override {
        if (_closed) return make_ready_future();
        _closed = true;

        _data.consume([](auto pkt) {
            return true;
        });
        return _data.push_eventually(packet());
    }
};

} // namespace rtmp
} // namespace amadeus
