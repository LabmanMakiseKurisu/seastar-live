#pragma once

#include <rtmp-server.h>
#include <seastar/core/app-template.hh>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/distributed.hh>
#include <seastar/core/gate.hh>
#include <seastar/core/metrics_registration.hh>
#include <seastar/core/queue.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/sstring.hh>
#include <seastar/net/tls.hh>
#include <seastar/util/modules.hh>
#include <seastar/util/std-compat.hh>

#include "rtmp/exception.hh"
#include "rtmp/request.hh"
#include "rtmp/routes.hh"
#include "rtmp/stream.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;
using namespace std::chrono_literals;

struct reply;
class server;


using request_ptr = std::unique_ptr<request>;
using reply_ptr = std::unique_ptr<reply>;

class connection : public boost::intrusive::list_base_hook<> {
 public:
    connection(server& server, connected_socket&& fd, socket_address remote_address);
    ~connection();

    void shutdown();

    future<> process();
    future<> close();

 protected:
    server& _server;
    connected_socket _fd;
    socket_address _remote_address;

    seastar::input_stream<char> _read_buf;
    seastar::output_stream<char> _write_buf;

    size_t _recv_bytes = 0;
    size_t _send_bytes = 0;

    std::optional<seastar::promise<request_ptr>> _handshake;

    std::deque<packet> _media_input_cache;
    std::deque<temporary_buffer<char>> _data_output_cache;

    seastar::queue<packet> _media_input;
    seastar::queue<packet> _media_output;

    input_stream _input;
    output_stream _output;

    ::rtmp_server_t* _rtmp_svr = nullptr;

    // null element marks eof
    bool _started = false;
    bool _stopped = false;
    bool _done = false;

    void on_send_new_buffer(temporary_buffer<char> buf);
    void on_receive_new_packet(packet pkt);

    future<> on_read_buf(temporary_buffer<char> buf);
    future<> on_send_packet(packet pkt);

    void on_new_connection();
    void on_recv(size_t bytes);
    void on_send(size_t bytes);

    using reqrep = std::tuple<request_ptr, reply_ptr>;
    using reqstatus = std::tuple<request_ptr, reply::status_type>;

    future<reqrep> handshake();
    future<> on_read_packets(std::deque<packet> pkts);
    future<> on_write_buffers(std::deque<temporary_buffer<char>> bufs);

    future<> flush_in();
    future<> flush_out();
    future<> flush();

    int send(packet pkt);

    future<> maybe_read_body(request_ptr req);
    future<> maybe_write_body(reply_ptr rep);

    future<reply_ptr> generate_reply(request_ptr& req);
    reply_ptr make_reply(reply::status_type status);

    int on_pause(int pause, uint32_t ms);
    int on_seek(uint32_t ms);
    int on_get_duration(const char* app, const char* stream, double* duration);

    future<> read_one();
    future<> input_loop();
    future<> output_loop();

    future<> stop_once();
    future<> close_streams();

    void abort(int code);
    void abort(std::exception e);
    void abort(std::exception_ptr e);

    void on_handshake(std::exception e);
    void on_handshake(std::exception_ptr e);
    void on_handshake(std::nullptr_t n);
    void on_handshake(request_ptr req);

 private:
    static rtmp_server_t* make_rtmp_server(connection* conn);

    int on_publish(const char* app, const char* stream, const char* type, const char* tcurl);
    int on_play(const char* app, const char* stream, double start, double duration, uint8_t reset, const char* tcurl);

    static int rtmp_handler_send(void* param, const void* header, size_t hlen, const void* payload, size_t len);
    static int rtmp_handler_onscript(void* param, const void* payload, size_t len, uint32_t timestamp);
    static int rtmp_handler_onaudio(void* param, const void* payload, size_t len, uint32_t timestamp);
    static int rtmp_handler_onvideo(void* param, const void* payload, size_t len, uint32_t timestamp);
    static int
    rtmp_handler_onpublish(void* param, const char* app, const char* stream, const char* type, const char* tcurl);
    static int rtmp_handler_onplay(
        void* param,
        const char* app,
        const char* stream,
        double start,
        double duration,
        uint8_t reset,
        const char* tcurl);
    static int rtmp_handler_onpause(void* param, int pause, uint32_t ms);
    static int rtmp_handler_onseek(void* param, uint32_t ms);
    static int rtmp_handler_ongetduration(void* param, const char* app, const char* stream, double* duration);
};

class server {
    std::vector<server_socket> _listeners;
    uint64_t _total_connections = 0;
    uint64_t _current_connections = 0;
    uint64_t _requests_served = 0;
    uint64_t _read_errors = 0;
    uint64_t _respond_errors = 0;
    uint64_t _recv_errors = 0;
    uint64_t _send_errors = 0;
    uint64_t _recv_bytes = 0;
    uint64_t _send_bytes = 0;

    gate _task_gate;

 public:
    routes _routes;
    using connection = rtmp::connection;

    explicit server(const sstring& name) {}

    future<> listen(socket_address addr, listen_options lo);
    future<> listen(socket_address addr);
    future<> stop();

    uint64_t total_connections() const;
    uint64_t current_connections() const;
    uint64_t requests_served() const;
    uint64_t read_errors() const;
    uint64_t reply_errors() const;
    uint64_t recv_errors() const;
    uint64_t send_errors() const;
    uint64_t recv_bytes() const;
    uint64_t send_bytes() const;

 private:
    future<> do_accepts(int which);
    future<> do_accept_one(int which);

    boost::intrusive::list<connection> _connections;
    friend class rtmp::connection;
};

/*
 * A helper class to start, set and listen an rtmp server
 * typical use would be:
 *
 * auto server = new server_control();
 *                 server->start().then([server] {
 *                 server->set_routes(set_routes);
 *              }).then([server, port] {
 *                  server->listen(port);
 *              }).then([port] {
 *                  std::cout << "Seastar HTTP server listening on port " << port << " ...\n";
 *              });
 */
class server_control {
    using rtmp_server = amadeus::rtmp::server;
    std::unique_ptr<distributed<rtmp_server>> _server_dist;

 private:
    static sstring generate_server_name();

 public:
    server_control()
    : _server_dist(new distributed<rtmp_server>) {}

    ~server_control() {}

    future<> start(const sstring& name = generate_server_name());
    future<> stop();
    future<> set_routes(std::function<void(routes& r)> fun);
    future<> listen(socket_address addr);
    future<> listen(socket_address addr, listen_options lo);
    distributed<rtmp_server>& server();
};

} // namespace rtmp
} // namespace amadeus
