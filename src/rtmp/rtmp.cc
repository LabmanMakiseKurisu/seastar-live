#include "rtmp/rtmp.hh"

#include <boost/algorithm/string.hpp>
#include <seastar/core/app-template.hh>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/distributed.hh>
#include <seastar/core/metrics.hh>
#include <seastar/core/print.hh>
#include <seastar/core/queue.hh>
#include <seastar/core/sstring.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/core/when_all.hh>
#include <seastar/http/internal/content_source.hh>
#include <seastar/util/log.hh>
#include <seastar/util/short_streams.hh>
#include <seastar/util/string_utils.hh>
#include <util/CxxUrl.hh>

#include <algorithm>
#include <bitset>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "rtmp/exception.hh"
#include "rtmp/log.hh"
#include "rtmp/reply.hh"
#include "rtmp/unvarnished_stream.hh"
#include "rtmp.hh"
namespace amadeus {
namespace rtmp {

using namespace seastar;
using reqrep = std::tuple<request_ptr, reply_ptr>;
using reqstatus = std::tuple<request_ptr, reply::status_type>;


sstring
server_control::generate_server_name() {
    static thread_local uint16_t idgen;
    return seastar::format("rtmp-{}", idgen++);
}

//创建_rtmp_svr，并设置其param和handler
rtmp_server_t*
connection::make_rtmp_server(connection* conn) {
    struct rtmp_server_handler_t handler;
    handler.send = rtmp_handler_send;
    handler.onaudio = rtmp_handler_onaudio;
    handler.onvideo = rtmp_handler_onvideo;
    handler.onscript = rtmp_handler_onscript;
    handler.onplay = rtmp_handler_onplay;
    handler.onseek = rtmp_handler_onseek;
    handler.onpause = rtmp_handler_onpause;
    handler.onpublish = rtmp_handler_onpublish;
    handler.ongetduration = rtmp_handler_ongetduration;

    return ::rtmp_server_create(conn, &handler);
}

connection::connection(server& server, connected_socket&& fd, socket_address remote_address)
: _server(server)
, _fd(std::move(fd))
, _remote_address(remote_address)
, _media_input(queue<packet>(10))
, _media_output(queue<packet>(10))
, _rtmp_svr(make_rtmp_server(this)) {
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

    on_new_connection();
}

connection::~connection() {
    --_server._current_connections;
    _server._connections.erase(_server._connections.iterator_to(*this));

    if (_rtmp_svr) ::rtmp_server_destroy(_rtmp_svr);

    l.info("connection desctruct {}", _remote_address);
}

//维护新连接的信息
void
connection::on_new_connection() {
    ++_server._total_connections;
    ++_server._current_connections;

    _fd.set_nodelay(true);
    _server._connections.push_back(*this);
}

//维护接收的字节数
void
connection::on_recv(size_t bytes) {
    _recv_bytes += bytes;
    _server._recv_bytes += bytes;
}

//维护发送的字节数
void
connection::on_send(size_t bytes) {
    _send_bytes += bytes;
    _server._send_bytes += bytes;
}

//生产一个reply，只有状态没有数据，此函数未被调用过
reply_ptr
connection::make_reply(reply::status_type status) {
    auto rep = std::make_unique<reply>();
    rep->set_status(status);

    return rep;
}

//根据req，执行server._routes.handle，生成对应的reply
future<reply_ptr>
connection::generate_reply(std::unique_ptr<rtmp::request>& req) {
    auto resp = std::make_unique<rtmp::reply>();
    resp->_read_bytes_provider = [this] {
        return _recv_bytes;
    };
    resp->_write_bytes_provider = [this] {
        return _send_bytes;
    };
    return _server._routes.handle(req->_mode, req, std::move(resp));
}

//调用req->_body_reader(*req, _input)
future<>
connection::maybe_read_body(request_ptr req) {
    if (!req->_body_reader) return make_ready_future<>();

    return do_with(std::move(req), [this](request_ptr& req) {
        auto& req_ = *req;
        return req->_body_reader(req_, _input).then_wrapped([req = std::move(req), this](auto f) {
            if (f.failed()) {
                // In case of an error during the write close the connection
                _server._read_errors++;
                _done = true;

                f.ignore_ready_future();
            }
            return make_ready_future<>();
        });
    });
}

//调用rep>_body_reader(*rep, _output)
future<>
connection::maybe_write_body(reply_ptr rep) {
    if (!rep->_body_writer) return make_ready_future<>();

    return do_with(std::move(rep), [this](reply_ptr& rep) {
        auto& rep_ = *rep;
        return rep->_body_writer(rep_, _output).then_wrapped([rep = std::move(rep), this](auto f) {
            if (f.failed()) {
                // In case of an error during the write close the connection
                _server._respond_errors++;
                _done = true;

                f.ignore_ready_future();
                return make_ready_future<>();
            }
            return make_ready_future<>();
        });
    });
}

//初始化_handshake，生成对应的reqrep
/**
 这个函数首先创建_handshake，等_handshake完成后，会处理其携带的req
*/
future<reqrep>
connection::handshake() {
    _handshake = std::make_optional(seastar::promise<request_ptr>());
    return _handshake->get_future().then([this](request_ptr req) {
        ++_server._requests_served;

        return do_with(std::move(req), [this](request_ptr& req) {
            if (!req) return make_ready_future<reqrep>(std::make_tuple(nullptr, nullptr));

            return generate_reply(req).then([&req](reply_ptr rep) {
                return make_ready_future<reqrep>(std::make_tuple(std::move(req), std::move(rep)));
            });
        });
    });
}

future<>
connection::read_one() {
    return handshake()
        .then_unpack([this](request_ptr req, reply_ptr rep) {
            if (!req || !rep || rep->_status != reply::status_type::ok) {
                _done = true;
                return make_ready_future<>();
            }
            return do_with(std::move(req), std::move(rep), [this](request_ptr& req, reply_ptr& rep) {
                return maybe_read_body(std::move(req)).then([&rep, this] {
                    return maybe_write_body(std::move(rep));
                });
            });
        })
        .finally([this] {
            return close_streams().handle_exception([this](auto e) {});
        });
}

//调用input_loop(), output_loop(), read_one()
future<>
connection::process() {
    // Launch read and write "threads" simultaneously:
    return when_all(input_loop(), output_loop(), read_one()).then_wrapped([this](auto&& f) {
        if (f.failed()) {
            f.ignore_ready_future();
            return close();
        } else {
            auto joined = f.get0();
            try {
                std::get<0>(joined).get();
            } catch (...) { l.trace("Input exception encountered: {}", std::current_exception()); }
            try {
                std::get<1>(joined).get();
            } catch (...) { l.trace("Output exception encountered: {}", std::current_exception()); }
            try {
                std::get<2>(joined).get();
            } catch (...) { l.trace("Response exception encountered: {}", std::current_exception()); }

            return make_ready_future<>();
        }
    });
}

//把pkts依次放入到_media_input
future<>
connection::on_read_packets(std::deque<packet> pkts) {
    if (pkts.empty()) return make_ready_future<>();

    return do_with(std::move(pkts), [this](std::deque<packet>& pkts) {
        return do_for_each(pkts, [this](packet& pkt) {
            return _media_input.push_eventually(std::move(pkt));
        });
    });
}

//把bufs依次放入到_write_buf并执行_write_buf.flush()
future<>
connection::on_write_buffers(std::deque<temporary_buffer<char>> bufs) {
    if (_done || bufs.empty()) return make_ready_future<>();

    return do_with(std::move(bufs), [this](std::deque<temporary_buffer<char>>& bufs) {
        return do_for_each(bufs, [this](temporary_buffer<char>& buf) {
            if (_done) return make_ready_future<>();
            return _write_buf.write(std::move(buf)).then([this] {
                return _write_buf.flush();
            });
        });
    });
}

//调用on_read_packets(std::move(_media_input_cache))
future<>
connection::flush_in() {
    return on_read_packets(std::move(_media_input_cache));
}

//调用on_write_buffers(std::move(_data_output_cache))
future<>
connection::flush_out() {
    return on_write_buffers(std::move(_data_output_cache)).handle_exception([this](auto e) {
        _server._send_errors++;
        return make_exception_future<>(e);
    });
}

//依次执行flush_in和flush_out
future<>
connection::flush() {
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

//parse rtmp buffer，结果存到_media_input_cache
future<>
connection::on_read_buf(temporary_buffer<char> buf) {
    try {
        auto rt = ::rtmp_server_input(_rtmp_svr, reinterpret_cast<uint8_t*>(const_cast<char*>(buf.get())), buf.size());
        if (rt >= 0) return make_ready_future<>();

        throw std::runtime_error(fmt::format("failed to parse rtmp buffer {}", rt));
    } catch (...) {
        auto e = std::current_exception();
        on_handshake(e);
        return make_exception_future<>(e);
    }
}

//调用send(std::move(pkt))
future<>
connection::on_send_packet(packet pkt) {
    try {
        int rt = send(std::move(pkt));
        if (rt >= 0) return make_ready_future();

        return make_exception_future<>(bad_request_exception("failed to send packet"));
    } catch (...) { return current_exception_as_future(); }
}

//调用stop_once()，然后关闭_input和_output
future<>
connection::close_streams() {
    return stop_once()
        .then([this] {
            return _input.close();
        })
        .then([this] {
            return _output.close();
        });
}

void
connection::abort(int code) {
    abort(std::system_error(code, std::system_category()));
}

void
connection::abort(std::exception e) {
    abort(std::make_exception_ptr(e));
}

void
connection::abort(std::exception_ptr e) {
    _media_input.abort(e);
    _media_output.abort(e);
}

//握手失败后调用
void
connection::on_handshake(std::exception e) {
    on_handshake(std::make_exception_ptr(e));
}

void
connection::on_handshake(std::exception_ptr e) {
    if (_handshake != std::nullopt) {
        _handshake->set_exception(e);
        _handshake = std::nullopt;
    }
}

//握手失败后调用
void
connection::on_handshake(std::nullptr_t n) {
    if (_handshake != std::nullopt) {
        _handshake->set_value(nullptr);
        _handshake = std::nullopt;
    }
}

//握手成功后调用，贤设置_handshake的futrue使得handshake能执行，再重置_handshake等待下一次连接
void
connection::on_handshake(request_ptr req) {
    if (_handshake != std::nullopt) {
        _handshake->set_value(std::move(req));
        _handshake = std::nullopt;

        _started = true;
    }
}

//循环不断_read_buf中读取数据，然后调用on_read_buf存入_media_input_cache，再执行flush
future<>
connection::input_loop() {
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
                                   _server._recv_errors++;
                                   return make_exception_future<>(f.get_exception());
                               } else {
                                   auto buf = f.get0();
                                   if (buf.empty()) {
                                       done = true;
                                       on_handshake(nullptr);
                                       abort(ENOTCONN);
                                       return _read_buf.close();
                                   } else {
                                       return on_read_buf(std::move(buf)).then_wrapped([&done, this](auto f) {
                                           if (f.failed()) {
                                               _server._recv_errors++;
                                               return make_exception_future<>(f.get_exception());
                                           } else {
                                               f.ignore_ready_future();
                                               return flush().handle_exception([&done, this](auto e) {
                                                   done = true;
                                               });
                                           }
                                       });
                                   }
                               }
                           })
                           .then_wrapped([&done, this](auto f) {
                               if (f.failed()) {
                                   done = true;
                                   on_handshake(nullptr);
                                   abort(f.get_exception());
                                   return close_streams().handle_exception([](auto e) {});
                               } else {
                                   f.ignore_ready_future();
                                   if (!done) return make_ready_future();

                                   return close_streams().handle_exception([this](auto e) {
                                       abort(e);
                                       return make_ready_future();
                                   });
                               };
                           });
                   })
            .finally([] {
                l.trace("tcp read complete");
            })
            .handle_exception([&done, this](auto e) {
                assert(0);
            });
    });
}
//不断从_media_output取出一个pkt，然后调用on_send_packet写入_data_output_cache，最后执行flush
future<>
connection::output_loop() {
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
                                   return close_streams().handle_exception([](auto e) {});
                               } else {
                                   f.ignore_ready_future();
                                   if (!done) return make_ready_future();

                                   return close_streams().handle_exception([this](auto e) {
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

//将pkt转为buf后写入_data_output_cache
int
connection::send(packet pkt) {
    if (pkt.type == packet::type_t::video) {
        return ::rtmp_server_send_video(_rtmp_svr, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else if (pkt.type == packet::type_t::audio) {
        return ::rtmp_server_send_audio(_rtmp_svr, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else if (pkt.type == packet::type_t::script) {
        return ::rtmp_server_send_script(_rtmp_svr, pkt.data.get(), pkt.data.size(), pkt.dts);
    } else {
        assert(0);
    }

    return 0;
}

void
connection::shutdown() {
    _fd.shutdown_input();
    _fd.shutdown_output();
}

future<>
connection::stop_once() {
    if (!_started || _stopped) return make_ready_future<>();

    try {
        auto rt = ::rtmp_server_stop(_rtmp_svr);
        if (rt == 0) {
            l.trace("rtmp server stop");

            _stopped = true;
            _done = true;
            return flush_out();
        }
        return make_exception_future<>(std::runtime_error(fmt::format("failed to stop rtmp connection {}", rt)));
    } catch (...) {
        return make_exception_future<>(
            std::runtime_error(fmt::format("failed to stop rtmp connection {}", std::current_exception())));
    }
}

future<>
connection::close() {
    return _input.close().then([this] {
        return _output.close();
    });
}

static request::type
to_type(const sstring& type) {
    return type == "live" ? request::type::live : (type == "record" ? request::type::record : request::type::append);
}

static const char* deprecated_stream_name_key = "streamname";

int
connection::on_play(
    const char* app, const char* stream, double start, double duration, uint8_t reset, const char* tcurl) {
    request_ptr req = std::make_unique<request>();
    req->_mode = request::mode::play;
    req->_remote_address = _remote_address;
    req->start = start;
    req->duration = duration;
    req->reset = reset;
    req->tcurl = tcurl;

    req->_read_bytes_provider = [this] {
        return _recv_bytes;
    };
    req->_write_bytes_provider = [this] {
        return _send_bytes;
    };

    Url url(tcurl);
    for (auto kv : url.query()) req->args[kv.key()] = kv.val();

    sstring path(app);
    if (strlen(stream) > 0) {
        if (stream[0] != '?') path += "/";
        path += stream;
    }

    url = Url(fmt::format("rtmp://x.y/{}", path));
    for (auto kv : url.query()) req->args[kv.key()] = kv.val();

    std::vector<std::string> paths;
    boost::split(paths, url.path(), boost::is_any_of("/"));

    // rtmp://x.y.com/app?streamname={stream_name}
    // rtmp://x.y.com/app/?streamname={stream_name}
    if (paths.size() == 2) {
        auto it = req->args.find(deprecated_stream_name_key);
        if (it == req->args.end()) return -1;

        req->app_name = paths.back();
        req->stream_name = it->second;
        req->args.erase(it);
    } else if (paths.size() == 3) { // rtmp://x.y.com/app/streamname
        req->app_name = paths[1];
        req->stream_name = paths.back();
    } else {
        return -1;
    }

    on_handshake(std::move(req));

    return 0;
}

int
connection::on_publish(const char* app, const char* stream, const char* type, const char* tcurl) {
    request_ptr req = std::make_unique<request>();
    req->_mode = request::mode::publish;
    req->_remote_address = _remote_address;
    req->_type = to_type(type);
    req->tcurl = tcurl;

    req->_read_bytes_provider = [this] {
        return _recv_bytes;
    };
    req->_write_bytes_provider = [this] {
        return _send_bytes;
    };

    Url url(tcurl);
    for (auto kv : url.query()) req->args[kv.key()] = kv.val();

    sstring path(app);
    if (strlen(stream) > 0) {
        if (stream[0] != '?') path += "/";
        path += stream;
    }

    url = Url(fmt::format("rtmp://x.y/{}", path));
    for (auto kv : url.query()) req->args[kv.key()] = kv.val();

    std::vector<std::string> paths;
    boost::split(paths, url.path(), boost::is_any_of("/"));

    // rtmp://x.y.com/app?streamname={stream_name}
    // rtmp://x.y.com/app/?streamname={stream_name}
    if (paths.size() == 2) {
        auto it = req->args.find(deprecated_stream_name_key);
        if (it == req->args.end()) return -1;

        req->app_name = paths.back();
        req->stream_name = it->second;
        req->args.erase(it);
    } else if (paths.size() == 3) { // rtmp://x.y.com/app/streamname
        req->app_name = paths[1];
        req->stream_name = paths.back();
    } else {
        return -1;
    }
    on_handshake(std::move(req));

    return 0;
}

int
connection::on_pause(int pause, uint32_t ms) {
    return 0;
}

int
connection::on_seek(uint32_t ms) {
    return 0;
}

int
connection::on_get_duration(const char* app, const char* stream, double* duration) {
    return 0;
}

//向_data_output_cache插入buf
void
connection::on_send_new_buffer(temporary_buffer<char> buf) {
    _data_output_cache.push_back(std::move(buf));
}

//向_media_input_cache插入pkt
void
connection::on_receive_new_packet(packet pkt) {
    _media_input_cache.push_back(std::move(pkt));
}

// RTMP callbacks

//把header和payload按照次序放入_data_output_cache
int
connection::rtmp_handler_send(void* param, const void* header, size_t hlen, const void* payload, size_t plen) {
    connection* conn = reinterpret_cast<connection*>(param);

    if (hlen) {
        conn->on_send_new_buffer(temporary_buffer<char>(reinterpret_cast<char*>(const_cast<void*>(header)), hlen));
    }
    if (plen) {
        conn->on_send_new_buffer(temporary_buffer<char>(reinterpret_cast<char*>(const_cast<void*>(payload)), plen));
    }

    return hlen + plen;
}

//用payload创建script packet，再将其放入_media_input_cache
int
connection::rtmp_handler_onscript(void* param, const void* payload, size_t len, uint32_t timestamp) {
    connection* conn = reinterpret_cast<connection*>(param);
    conn->on_receive_new_packet(
        packet::make_script(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));

    return 0;
}

//用payload创建video packet，再将其放入_media_input_cache
int
connection::rtmp_handler_onvideo(void* param, const void* payload, size_t len, uint32_t timestamp) {
    connection* conn = reinterpret_cast<connection*>(param);
    conn->on_receive_new_packet(packet::make_video(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));
    return 0;
}

//用payload创建audio packet，再将其放入_media_input_cache
int
connection::rtmp_handler_onaudio(void* param, const void* payload, size_t len, uint32_t timestamp) {
    connection* conn = reinterpret_cast<connection*>(param);
    conn->on_receive_new_packet(packet::make_audio(temporary_buffer<uint8_t>((const uint8_t*)payload, len), timestamp));
    return 0;
}

//调用this->on_play
int
connection::rtmp_handler_onplay(
    void* param, const char* app, const char* stream, double start, double duration, uint8_t reset, const char* tcurl) {
    connection* conn = reinterpret_cast<connection*>(param);
    return conn->on_play(app, stream, start, duration, reset, tcurl);
}

//调用this->on_publish
int
connection::rtmp_handler_onpublish(
    void* param, const char* app, const char* stream, const char* type, const char* tcurl) {
    connection* conn = reinterpret_cast<connection*>(param);
    return conn->on_publish(app, stream, type, tcurl);
}


//调用this->on_pause
int
connection::rtmp_handler_onpause(void* param, int pause, uint32_t ms) {
    connection* conn = reinterpret_cast<connection*>(param);
    return conn->on_pause(pause, ms);
}

//调用this->on_seek
int
connection::rtmp_handler_onseek(void* param, uint32_t ms) {
    connection* conn = reinterpret_cast<connection*>(param);
    return conn->on_seek(ms);
}

//调用this->on_get_duration
int
connection::rtmp_handler_ongetduration(void* param, const char* app, const char* stream, double* duration) {
    connection* conn = reinterpret_cast<connection*>(param);
    return conn->on_get_duration(app, stream, duration);
}

server::server(const sstring& name) {
    l.info("the rtmp server instance is running on shard: {}", this_shard_id());
    
}

future<>
server::listen(socket_address addr, listen_options lo) {
    _listeners.push_back(seastar::listen(addr, lo));
    return do_accepts(_listeners.size() - 1);
}

future<>
server::listen(socket_address addr) {
    listen_options lo;
    lo.reuse_address = true;
    return listen(addr, lo);
}

future<>
server::stop() {
    future<> tasks_done = _task_gate.close();
    for (auto&& l : _listeners) { l.abort_accept(); }
    for (auto&& c : _connections) { c.shutdown(); }
    return tasks_done;
}

// 不断调用do_accept_one，直到gate关闭
future<>
server::do_accepts(int which) {
    (void)try_with_gate(_task_gate, [this, which] {
        return keep_doing([this, which] {
                   return try_with_gate(_task_gate, [this, which] {
                       return do_accept_one(which);
                   });
               })
            .handle_exception_type([](const gate_closed_exception& e) {});
    }).handle_exception_type([](const gate_closed_exception& e) {});
    return make_ready_future<>();
}

//实际处理tcp连接并执行业务的函数
future<>
server::do_accept_one(int which) {
    return _listeners[which]
        .accept()
        .then([this](accept_result ar) mutable {
            auto conn = std::make_unique<connection>(*this, std::move(ar.connection), std::move(ar.remote_address));
            (void)try_with_gate(_task_gate, [conn = std::move(conn)]() mutable {
                return conn->process().handle_exception([conn = std::move(conn)](std::exception_ptr ex) {
                    l.warn("request error: {}", ex);
                });
            }).handle_exception_type([](const gate_closed_exception& e) {});
        })
        .handle_exception_type([](const std::system_error& e) {
            // We expect a ECONNABORTED when server::stop is called,
            // no point in warning about that.
            if (e.code().value() != ECONNABORTED) { l.warn("accept failed: {}", e); }
        })
        .handle_exception([](std::exception_ptr ex) {
            l.warn("accept failed: {}", ex);
        });
}

uint64_t
server::total_connections() const {
    return _total_connections;
}

uint64_t
server::current_connections() const {
    return _current_connections;
}

uint64_t
server::requests_served() const {
    return _requests_served;
}

uint64_t
server::read_errors() const {
    return _read_errors;
}

uint64_t
server::reply_errors() const {
    return _respond_errors;
}

uint64_t
server::recv_errors() const {
    return _recv_errors;
}

uint64_t
server::send_errors() const {
    return _send_errors;
}

uint64_t
server::recv_bytes() const {
    return _recv_bytes;
}

uint64_t
server::send_bytes() const {
    return _send_bytes;
}

//为所有核心创建server实例
future<>
server_control::start(const sstring& name) {
    return _server_dist->start(name);
}

//停止所有核心上的服务
future<>
server_control::stop() {
    return _server_dist->stop();
}

//为所有核心上的server实例调用fun设置路由
future<>
server_control::set_routes(std::function<void(routes& r)> fun) {
    return _server_dist->invoke_on_all([fun](auto& s) {
        fun(s._routes);
    });
}

future<>
server_control::listen(socket_address addr) {
    return _server_dist->invoke_on_all<future<> (server::*)(socket_address)>(&server::listen, addr);
}

//为所有核心上的server实例调用listen
future<>
server_control::listen(socket_address addr, listen_options lo) {
    return _server_dist->invoke_on_all<future<> (server::*)(socket_address, listen_options)>(&server::listen, addr, lo);
}

distributed<server>&
server_control::server() {
    return *_server_dist;
}

} // namespace rtmp
} // namespace amadeus
