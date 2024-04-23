/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Vepubion 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownepubhip.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
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

#include "rtmp/client.hh"

#include <seastar/core/loop.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/core/when_all.hh>
#include <seastar/core/with_timeout.hh>
#include <seastar/http/internal/content_source.hh>
#include <seastar/net/dns.hh>
#include <seastar/net/tls.hh>
#include <seastar/util/short_streams.hh>
#include <seastar/util/string_utils.hh>

#include"rtmp/data_sink.hh"
#include "rtmp-internal.h"
#include "rtmp/exception.hh"
#include "rtmp/log.hh"
#include "rtmp/reply.hh"
#include "rtmp/request.hh"
#include "util/util.hh"

namespace amadeus {
namespace rtmp {
namespace internal {

using namespace seastar;

client_ref::client_ref(client* c) noexcept
: _c(c) {
    _c->_total_connections++;
}

client_ref::~client_ref() {}

client::connection::connection(connected_socket&& fd, internal::client_ref cr)
: _fd(std::move(fd))
, _closed(_fd.wait_input_shutdown().finally([me = shared_from_this()] {}))
, _ref(std::move(cr))
, _media_input(queue<packet>(10))
, _media_output(queue<packet>(10)) {
    _read_buf = seastar::input_stream<char>(seastar::data_source(
        std::make_unique<httpd::internal::unvarnished_data_source_impl>(_fd.input().detach(), [this](size_t bytes) {
            on_recv(bytes);
        })));

    output_stream_options opts;
    opts.batch_flushes = true;
    _write_buf = seastar::output_stream<char>(
        seastar::data_sink(std::make_unique<httpd::internal::unvarnished_data_sink_impl>(
            _fd.output().detach(),
            [this](size_t bytes) {
                on_send(bytes);
            })),
        8192,
        opts);

    _input = input_stream(data_source(std::make_unique<media_data_source_impl>(_media_input)));
    _output = output_stream(data_sink(std::make_unique<media_data_sink_impl>(_media_output)), 10);
}

client::connection::~connection() {
    if (_rtmp_cln) ::rtmp_client_destroy(_rtmp_cln);
    _rtmp_cln = nullptr;

    l.debug("connection desctruct {}", _ref.get()->_new_connections->remote_address());
}

void
client::connection::on_recv(size_t bytes) {
    _recv_bytes += bytes;
    _ref.get()->_recv_bytes += bytes;
}

void
client::connection::on_send(size_t bytes) {
    _send_bytes += bytes;
    _ref.get()->_send_bytes += bytes;
}

void
client::connection::reset(request& req) {
    l.trace("reset rtmp client {}", req);

    struct rtmp_client_handler_t handler;
    handler.send = client::connection::rtmp_handler_send;
    handler.onaudio = client::connection::rtmp_handler_onaudio;
    handler.onvideo = client::connection::rtmp_handler_onvideo;
    handler.onscript = client::connection::rtmp_handler_onscript;

    if (_rtmp_cln) ::rtmp_client_destroy(_rtmp_cln);
    _rtmp_cln = ::rtmp_client_create(req.app_name.c_str(), req.stream().c_str(), req.tcurl.c_str(), this, &handler);
}

int
client::connection::send(packet pkt) {
    if (!_rtmp_cln) return -1;

    if (pkt.type == packet::type_t::video) {
        return ::rtmp_client_push_video(_rtmp_cln, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else if (pkt.type == packet::type_t::audio) {
        return ::rtmp_client_push_audio(_rtmp_cln, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else if (pkt.type == packet::type_t::script) {
        return ::rtmp_client_push_script(_rtmp_cln, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else {
        assert(0);
    }
    return 0;
}

future<reply_ptr>
client::connection::send_request(request& req) {
    assert(_rtmp_cln);

    int rt = ::rtmp_client_start(_rtmp_cln, static_cast<int>(req._mode));
    if (rt != 0) return make_exception_future<reply_ptr>(bad_request_exception("failed to start RTMP client"));

    l.trace("rtmp client connection start {}", req);
    _handshake = std::make_optional(seastar::promise<reply_ptr>());
    return flush_out().then([this] {
        return _handshake->get_future();
    });
}

future<>
client::connection::maybe_write_body(request& req) {
    if (!req._body_writer) return make_ready_future<>();

    l.trace("request write body {}", req);
    return req._body_writer(req, _output).then([this] {
        return _output.flush();
    });
}

future<>
client::connection::maybe_read_body(request& req, reply& rep, reply_handler handle) {
    if (!handle) return make_ready_future<>();

    l.trace("reply read body {}", rep);
    return handle(req, rep, _input).finally([rep = std::move(rep)] {});
}

future<>
client::connection::make_request(request req, reply_handler handle, reply::status_type expected) {
    req._read_bytes_provider = [this] {
        return _recv_bytes;
    };
    req._write_bytes_provider = [this] {
        return _send_bytes;
    };
    return do_with(
               std::move(req),
               std::move(handle),
               [expected, this](request& req, reply_handler& handle) {
                   return send_request(req).then_wrapped([&req, &handle, expected, this](auto f) mutable {
                       if (f.failed()) {
                           auto e = f.get_exception();
                           l.trace("failed to handshake {}", e);
                           return make_exception_future<>(e);
                       } else {
                           auto rep = f.get0();
                           if (!rep) {
                               l.trace("failed to handshake {}", req);
                               return make_exception_future<>(
                                   unexpected_status_error(reply::status_type::internal_error));
                           }
                           l.trace("handshake successfully {}", req);
                           if (rep->_status != expected) {
                               if (l.is_enabled(log_level::debug)) l.debug("request finished with {}", rep->_status);
                               return make_exception_future<>(unexpected_status_error(rep->_status));
                           }
                           rep->_read_bytes_provider = [this] {
                               return _recv_bytes;
                           };
                           rep->_write_bytes_provider = [this] {
                               return _send_bytes;
                           };
                           return do_with(std::move(rep), [&req, &handle, this](reply_ptr& rep) {
                               auto& _rep = *rep;
                               return maybe_write_body(req)
                                   .then([&req, &_rep, &handle, this] {
                                       return maybe_read_body(req, _rep, std::move(handle));
                                   })
                                   .finally([&req, rep = std::move(rep)] {
                                       l.trace("request complete {}", req);
                                   });
                           });
                       }
                   });
               })
        .finally([this] {
            return stop_streams().handle_exception([this](auto e) {
                l.trace("ignored exception {}", e);
            });
        });
}

future<>
client::connection::on_read_packets(std::deque<packet> pkts) {
    if (pkts.empty()) return make_ready_future<>();

    return do_with(std::move(pkts), [this](std::deque<packet>& pkts) {
        return with_lock(_flush_in_lock, [&pkts, this] {
            return do_for_each(pkts, [this](packet& pkt) {
                return _media_input.push_eventually(std::move(pkt));
            });
        });
    });
}

future<>
client::connection::on_write_buffers(std::deque<temporary_buffer<char>> bufs) {
    if (bufs.empty()) return make_ready_future<>();
    if (_done) {
        l.trace("ignored write buffer for done");
        return make_ready_future<>();
    }

    return do_with(std::move(bufs), [this](std::deque<temporary_buffer<char>>& bufs) {
        return with_lock(_flush_out_lock, [&bufs, this] {
            return do_for_each(bufs, [this](temporary_buffer<char>& buf) {
                return _write_buf.write(std::move(buf)).then([this] {
                    return _write_buf.flush();
                });
            });
        });
    });
}

future<>
client::connection::flush_in() {
    return on_read_packets(std::move(_media_input_cache));
}

future<>
client::connection::flush_out() {
    return on_write_buffers(std::move(_data_output_cache));
}

future<>
client::connection::flush() {
    return flush_in().then_wrapped([this](auto f) {
        if (f.failed()) {
            return _read_buf.close().then([e = f.get_exception()] {
                return make_exception_future<>(std::move(e));
            });
        } else {
            return flush_out().handle_exception([this](auto e) {
                abort(e);
                return _read_buf.close().then([e] {
                    return make_exception_future<>(std::move(e));
                });
            });
        }
    });
}

future<>
client::connection::on_read_buf(temporary_buffer<char> buf) {
    try {
        auto rt = ::rtmp_client_input(_rtmp_cln, buf.get(), buf.size());
        if (rt >= 0) return update_state();

        throw std::runtime_error(fmt::format("failed to parse rtmp buffer {}", rt));
    } catch (...) {
        auto e = std::current_exception();
        on_handshake(e);
        return make_exception_future<>(e);
    }
}

future<>
client::connection::on_send_packet(packet pkt) {
    try {
        int rt = send(std::move(pkt));
        if (rt >= 0) return update_state();

        return make_exception_future<>(bad_request_exception("failed to send packet"));
    } catch (...) { return current_exception_as_future(); }
}

void
client::connection::abort(int code) noexcept {
    abort(std::make_exception_ptr(std::system_error(code, std::system_category())));
}

void
client::connection::abort(const std::exception_ptr& e) noexcept {
    _media_input.abort(e);
    _media_output.abort(e);
}

void
client::connection::on_handshake(const std::exception_ptr& e) noexcept {
    if (_handshake != std::nullopt) {
        _handshake->set_exception(e);
        _handshake = std::nullopt;
    }
}

void
client::connection::on_handshake(std::nullptr_t) noexcept {
    if (_handshake != std::nullopt) {
        _handshake->set_value(nullptr);
        _handshake = std::nullopt;
    }
}

void
client::connection::on_handshake(reply_ptr rep) noexcept {
    if (_handshake != std::nullopt) {
        _handshake->set_value(std::move(rep));
        _handshake = std::nullopt;
    }
}

future<>
client::connection::input_loop() {
    return do_with(false, [this](bool& done) {
        return do_until(
                   [&done, this] {
                       return done;
                   },
                   [&done, this] {
                       return _read_buf.read()
                           .then_wrapped([&done, this](auto f) {
                               if (f.failed()) {
                                   done = true;
                                   return make_exception_future<>(f.get_exception());
                               } else {
                                   auto buf = f.get0();
                                   if (buf.empty()) {
                                       done = true;
                                       on_handshake(nullptr);
                                       abort(ENOTCONN);
                                       l.trace("recv empty data");
                                       return _read_buf.close();
                                   } else {
                                       return on_read_buf(std::move(buf)).then([&done, this] {
                                           return flush().handle_exception([&done, this](auto e) {
                                               l.trace("ignored exception {}", e);
                                               done = true;
                                           });
                                       });
                                   }
                               }
                           })
                           .then_wrapped([&done, this](auto f) {
                               if (f.failed()) {
                                   done = true;
                                   on_handshake(nullptr);
                                   abort(f.get_exception());
                                   return stop_streams().handle_exception([](auto e) {
                                       l.trace("ignored exception {}", e);
                                   });
                               } else {
                                   f.ignore_ready_future();
                                   if (!done) return make_ready_future();

                                   return stop_streams().handle_exception([this](auto e) {
                                       abort(e);
                                       return make_ready_future();
                                   });
                               };
                           });
                   })
            .finally([this] {
                l.trace("tcp read complete");

                on_handshake(nullptr);
                abort(ENOTCONN);
                return close_streams().handle_exception([](auto e) {
                    l.trace("ignored exception {}", e);
                });
            })
            .handle_exception([&done, this](auto e) {
                assert(0);
            });
    });
}

future<>
client::connection::output_loop() {
    return do_with(false, [this](bool& done) {
        return do_until(
                   [&done, this] {
                       return done;
                   },
                   [&done, this] {
                       return _media_output.pop_eventually()
                           .then_wrapped([&done, this](auto f) {
                               if (f.failed()) {
                                   done = true;
                                   f.ignore_ready_future();
                                   return make_ready_future();
                               } else {
                                   auto pkt = f.get0();
                                   if (pkt.empty()) {
                                       done = true;
                                       on_handshake(nullptr);
                                       abort(ENOTCONN);
                                       return _write_buf.close();
                                   }
                                   return on_send_packet(std::move(pkt)).then_wrapped([&done, this](auto f) {
                                       if (f.failed()) {
                                           done = true;
                                           return make_exception_future<>(f.get_exception());
                                       } else {
                                           f.ignore_ready_future();
                                           return flush().handle_exception([&done, this](auto e) {
                                               done = true;
                                           });
                                       }
                                   });
                               }
                           })
                           .then_wrapped([&done, this](auto f) {
                               if (f.failed()) {
                                   done = true;
                                   on_handshake(nullptr);
                                   abort(f.get_exception());
                                   return stop_streams().handle_exception([](auto e) {});
                               } else {
                                   f.ignore_ready_future();
                                   if (!done) return make_ready_future();

                                   return stop_streams().handle_exception([this](auto e) {
                                       abort(e);
                                       return make_ready_future();
                                   });
                               };
                           });
                   })
            .finally([] {
                l.trace("tcp write complete");
            })
            .handle_exception([&done, this](auto e) {
                assert(0);
            });
    });
}

future<>
client::connection::process(request req, reply_handler handle, reply::status_type expected) {
    reset(req);

    return when_all_succeed(make_request(std::move(req), std::move(handle), expected), input_loop(), output_loop())
        .discard_result()
        .finally([this] {
            return when_all_succeed(_read_buf.close(), _write_buf.close()).discard_result();
        });
}

future<>
client::connection::close_streams() {
    return _input.close().then([this] {
        return _output.close();
    });
}

future<>
client::connection::stop_streams() {
    return stop_once().then([this] {
        return close_streams();
    });
}

future<>
client::connection::update_state() {
    auto st = ::rtmp_client_getstate(_rtmp_cln);
    if (st == rtmp_state_t::RTMP_STATE_START) {
        auto resp = std::make_unique<reply>();
        on_handshake(std::move(resp));
    } else if (st == rtmp_state_t::RTMP_STATE_STOP) {
        _stopped = true;
        return stop_streams();
    }
    return make_ready_future<>();
}

future<>
client::connection::stop_once() {
    if (_stopped) return make_ready_future<>();

    auto st = ::rtmp_client_getstate(_rtmp_cln);
    if (st != rtmp_state_t::RTMP_STATE_CREATE_STREAM && st != rtmp_state_t::RTMP_STATE_START)
        return make_ready_future<>();

    try {
        int rt = ::rtmp_client_stop(_rtmp_cln);
        if (rt < 0) return make_exception_future<>(bad_request_exception("failed to stop RTMP client"));

        l.trace("rtmp client connection stop");

        _stopped = true;
        _done = true;
        return flush_out();
    } catch (...) {
        return make_exception_future<>(
            std::runtime_error(fmt::format("failed to stop rtmp connection {}", std::current_exception())));
    }
}

future<>
client::connection::close() {
    return when_all_succeed(_input.close(), _output.close(), _read_buf.close(), _write_buf.close())
        .discard_result()
        .then([this] {
            return std::move(_closed);
        })
        .handle_exception([](auto e) {
            l.trace("ignored exception {}", e);
        });
}

void
client::connection::on_send_new_buffer(temporary_buffer<char> buf) {
    _data_output_cache.push_back(std::move(buf));
}

void
client::connection::on_receive_new_packet(packet pkt) {
    _media_input_cache.push_back(std::move(pkt));
}

int
client::connection::rtmp_handler_send(void* param, const void* header, size_t hlen, const void* payload, size_t plen) {
    client::connection* conn = reinterpret_cast<client::connection*>(param);

    if (hlen) {
        conn->on_send_new_buffer(temporary_buffer<char>(reinterpret_cast<char*>(const_cast<void*>(header)), hlen));
    }
    if (plen) {
        conn->on_send_new_buffer(temporary_buffer<char>(reinterpret_cast<char*>(const_cast<void*>(payload)), plen));
    }

    return hlen + plen;
}

int
client::connection::rtmp_handler_onscript(void* param, const void* payload, size_t len, uint32_t timestamp) {
    client::connection* conn = reinterpret_cast<client::connection*>(param);
    conn->on_receive_new_packet(
        packet::make_script(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));
    return 0;
}

int
client::connection::rtmp_handler_onvideo(void* param, const void* payload, size_t len, uint32_t timestamp) {
    client::connection* conn = reinterpret_cast<client::connection*>(param);
    conn->on_receive_new_packet(packet::make_video(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));
    return 0;
}

int
client::connection::rtmp_handler_onaudio(void* param, const void* payload, size_t len, uint32_t timestamp) {
    client::connection* conn = reinterpret_cast<client::connection*>(param);
    conn->on_receive_new_packet(packet::make_audio(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));
    return 0;
}

class basic_connection_factory : public connection_factory {
    socket_address _addr;

 public:
    explicit basic_connection_factory(socket_address addr)
    : _addr(std::move(addr)) {}

    virtual socket_address remote_address() const override {
        return _addr;
    }

    virtual future<connected_socket> make() override {
        return seastar::connect(_addr, {}, transport::TCP);
    }
};

client::client(socket_address addr)
: client(std::make_unique<basic_connection_factory>(std::move(addr))) {}

client::client(std::unique_ptr<connection_factory> f, unsigned max_connections)
: _new_connections(std::move(f))
, _max_connections(max_connections) {}

uint64_t
client::recv_bytes() const {
    return _recv_bytes;
}

uint64_t
client::send_bytes() const {
    return _send_bytes;
}

future<client::connection_ptr>
client::get_connection() {
    if (_pool.size() >= _max_connections) {
        return _not_full.wait().then([this] {
            return get_connection();
        });
    }

    return _new_connections->make().then([cr = client_ref(this)](connected_socket cs) mutable {
        l.trace("created new tcp connection {}", cs.local_address());
        auto conn = seastar::make_shared<connection>(std::move(cs), std::move(cr));
        return make_ready_future<connection_ptr>(std::move(conn));
    });
}

future<>
client::put_connection(connection_ptr conn) {
    l.trace("push tcp connection {} to pool", conn->_fd.local_address());
    _pool.push_back(conn);
    return make_ready_future<>();
}

future<>
client::remove_connection(connection_ptr conn) {
    for (auto it = _pool.begin(); it != _pool.end();) {
        if (*it != conn) continue;

        _pool.erase(it);
        _not_full.broadcast();

        return conn->close().finally([conn] {});
    }
    return make_ready_future<>();
}

future<>
client::shrink_connections() {
    if (_pool.empty() || _pool.size() <= _max_connections) return make_ready_future<>();

    connection_ptr conn = _pool.front();
    _pool.pop_front();

    if (_pool.size() < _max_connections) _not_full.broadcast();

    return conn->close().finally([this, conn] {
        return shrink_connections();
    });
}

future<>
client::set_maximum_connections(unsigned nr) {
    if (nr > _max_connections) {
        _max_connections = nr;
        _not_full.broadcast();
        return make_ready_future<>();
    }

    _max_connections = nr;
    return shrink_connections();
}

template <typename Fn>
SEASTAR_CONCEPT(requires std::invocable<Fn, client::connection&>)
auto client::with_connection(Fn&& fn) {
    return get_connection().then([this, fn = std::move(fn)](connection_ptr conn) mutable {
        return put_connection(conn).then([this, fn = std::move(fn), conn] {
            return fn(*conn).then_wrapped([this, conn](auto f) mutable {
                return remove_connection(conn).then([f = std::move(f)]() mutable {
                    return std::move(f);
                });
            });
        });
    });
}

future<>
client::make_request(request req, reply_handler handle, reply::status_type expected) {
    return do_with(std::move(req), std::move(handle), [expected, this](request& req, reply_handler& handle) {
        return with_connection([&req, &handle, expected](client::connection& conn) {
            return conn.process(std::move(req), std::move(handle), expected);
        });
    });
}

future<>
client::close() {
    if (_pool.empty()) return make_ready_future<>();

    connection_ptr conn = _pool.front();
    _pool.pop_front();

    l.trace("closing connection {}", conn->_fd.local_address());
    return conn->close().then([this, conn] {
        return close();
    });
}

} // namespace internal

future<>
ignore_reply(const request& req, const reply& rep, input_stream& in) {
    return make_ready_future<>();
}

dns_connection_factory::dns_connection_factory(const sstring& host, int port, float timeout)
: _host(std::move(host))
, _port(port)
, _timeout(timeout)
, _state(make_lw_shared<state>())
, _done(initialize()) {}

future<>
dns_connection_factory::initialize() {
    auto state = _state;
    return seastar::net::dns::get_host_by_name(_host, seastar::net::inet_address::family::INET)
        .then([state, port = _port](seastar::net::hostent hent) mutable {
            state->addr = socket_address(hent.addr_list.front(), port);
        })
        .then([state] {
            state->initialized = true;
            return make_ready_future<>();
        });
}

socket_address
dns_connection_factory::remote_address() const {
    return _state->addr.length() ? _state->addr : socket_address();
}

future<connected_socket>
dns_connection_factory::make() {
    auto f = make_ready_future<>();
    if (!_state->initialized) f = _done.get_future();

    return f.then([state = _state, host = _host, timeout = _timeout] {
        auto f = seastar::connect(state->addr, {}, transport::TCP);
        if (timeout == -1) return f;

        auto tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int64_t>(timeout * 1000));
        return with_timeout(tp, std::move(f));
    });
}

client::client(socket_address sa, float timeout)
: client(std::make_tuple(seastar::to_sstring(sa.addr()), ntohl(sa.u.in.sin_port)), timeout) {}

client::client(const sstring& address, float timeout)
: client(util::split_address(address), timeout) {}

client::client(const sstring& host, uint32_t port, float timeout)
: client(std::make_tuple(host, port), timeout) {}

client::client(std::tuple<sstring, uint32_t> address_parts, float timeout)
: _host(std::get<0>(address_parts))
, _port(std::get<1>(address_parts))
, _timeout(timeout) {
}

client::~client() {}

void
client::for_each_client(std::function<void(internal::client&)> func) {
    std::lock_guard<std::mutex> g(_lock);

    for (auto& e : _clients) func(e.second);
}

future<>
client::make_request(request req, internal::reply_handler handle, reply::status_type expected) {
    auto sg = current_scheduling_group();

    _lock.lock();
    auto it = _clients.find(sg);
    if (it == _clients.end()) [[unlikely]] {
        assert(_host.size());
        if (_host.empty()) return make_exception_future<>(bad_request_exception("host is empty"));

        auto factory = std::make_unique<dns_connection_factory>(_host, _port ?: 1935, _timeout);
        // Limit the maximum number of connections this group's http client
        // may have proportional to its shares. Shares are typically in the
        // range of 100...1000, thus resulting in 1..10 connections
        auto max_connections = std::max((unsigned)(sg.get_shares() / 100), 1u);
        it = _clients
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(sg),
                     std::forward_as_tuple(std::move(factory), max_connections))
                 .first;
    }
    auto& cln = it->second;
    _lock.unlock();

    l.info("make request {}", req);
    return cln.make_request(std::move(req), std::move(handle), expected);
}

future<>
client::close() {
    return parallel_for_each(_clients, [](auto& it) -> future<> {
        return it.second.close();
    });
}

} // namespace rtmp
} // namespace amadeus
