#pragma once

#include <rtmp-client.h>
#include <seastar/core/condition-variable.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/shared_future.hh>
#include <seastar/core/shared_mutex.hh>
#include <seastar/net/api.hh>

#include "rtmp/reply.hh"
#include "rtmp/stream.hh"

namespace bi = boost::intrusive;

namespace amadeus {
namespace rtmp {

struct request;
struct reply;

namespace internal {

using namespace seastar;

class client;

class client_ref {
    client* _c;

 public:
    client_ref(client* c) noexcept;
    ~client_ref();

    client_ref(client_ref&& o) noexcept
    : _c(std::exchange(o._c, nullptr)) {}

    client_ref(const client_ref&) = delete;

    client* get() {
        return _c;
    }
};

class connection_factory {
 public:
    /**
     * \brief Make a \ref connected_socket
     *
     * The implementations of this method should return ready-to-use socket that will
     * be used by \ref client as transport for its http connections
     */
    virtual future<connected_socket> make() = 0;
    virtual socket_address remote_address() const = 0;

    virtual ~connection_factory() {}
};

/**
 * \brief Class client wraps communications using HTTP protocol
 *
 * The class allows making HTTP requests and handling replies. It's up to the caller to
 * provide a transport, though for simple cases the class provides out-of-the-box
 * facilities.
 *
 * The main benefit client provides against \ref connection is the transparent support
 * for Keep-Alive transport sockets.
 */

using reply_ptr = std::unique_ptr<reply>;
using reply_handler = noncopyable_function<future<>(const request&, const reply&, input_stream&)>;

class client {
    /**
     * \brief Class connection represents an HTTP connection over a given transport
     *
     * Check the demos/tcp_client_demo.cc for usage example
     */

    class connection : public seastar::enable_shared_from_this<connection> {
        friend class client;

        ::rtmp_client_t* _rtmp_cln = nullptr;

        connected_socket _fd;
        seastar::input_stream<char> _read_buf;
        seastar::output_stream<char> _write_buf;
        size_t _recv_bytes = 0;
        size_t _send_bytes = 0;
        future<> _closed;
        internal::client_ref _ref;

        std::optional<seastar::promise<reply_ptr>> _handshake;

        std::deque<packet> _media_input_cache;
        shared_mutex _flush_in_lock;

        std::deque<temporary_buffer<char>> _data_output_cache;
        shared_mutex _flush_out_lock;

        seastar::queue<packet> _media_input;
        seastar::queue<packet> _media_output;

        input_stream _input;
        output_stream _output;

        bool _stopped = false;
        bool _done = false;

     public:
        /**
         * \brief Create an rtmp connection
         *
         * Construct the connection that will work over the provided \fd transport socket
         *
         */
        connection(connected_socket&& fd, internal::client_ref cr);
        ~connection();

        /**
         * \brief Send the request and wait for response
         *
         * Sends the provided request to the client, and returns a future that will resolve
         * into the client response.
         *
         * If the request was configured with the set_expects_continue() and the client replied
         * early with some error code, this early reply will be returned back.
         *
         * The returned reply only contains the status and headers. To get the reply body the
         * caller should read it via the input_stream provided by the connection.in() method.
         *
         * \param rq -- request to be sent
         *
         */
        future<reply> make_request(request rq);

        /**
         * \brief Closes the connection
         *
         * Connection must be closed regardless of whether there was an exception making the
         * request or not
         */
        future<> close();

     private:
        void on_send_new_buffer(temporary_buffer<char> buf);
        void on_receive_new_packet(packet pkt);

        future<> on_read_buf(temporary_buffer<char> buf);
        future<> on_send_packet(packet pkt);

        void on_recv(size_t bytes);
        void on_send(size_t bytes);

        void reset(request& req);

        int send(packet pkt);

        future<> make_request(request req, reply_handler handle, reply::status_type expected = reply::status_type::ok);
        future<> process(request req, reply_handler handle, reply::status_type expected = reply::status_type::ok);

        future<> input_loop();
        future<> output_loop();

        future<> stop_once();
        future<> close_streams();
        future<> stop_streams();

        void abort(int code) noexcept;
        void abort(const std::exception_ptr& e) noexcept;

        void on_handshake(const std::exception_ptr& e) noexcept;
        void on_handshake(std::nullptr_t) noexcept;
        void on_handshake(reply_ptr rep) noexcept;

        future<> on_read_packets(std::deque<packet> pkts);
        future<> on_write_buffers(std::deque<temporary_buffer<char>> bufs);

        future<> flush_in();
        future<> flush_out();
        future<> flush();

        future<reply_ptr> send_request(request& req);

        future<> maybe_write_body(request& req);
        future<> maybe_read_body(request& req, reply& rep, reply_handler handle);

        future<> update_state();
        static int rtmp_handler_send(void* param, const void* header, size_t hlen, const void* payload, size_t len);
        static int rtmp_handler_onscript(void* param, const void* payload, size_t len, uint32_t timestamp);
        static int rtmp_handler_onaudio(void* param, const void* payload, size_t len, uint32_t timestamp);
        static int rtmp_handler_onvideo(void* param, const void* payload, size_t len, uint32_t timestamp);
    };

    friend class connection;
    friend class rtmp::internal::client_ref;

    using connection_ptr = seastar::shared_ptr<connection>;
    using connections_list_t = std::deque<connection_ptr>;

    static constexpr unsigned default_max_connections = 100;

    uint64_t _recv_bytes = 0;
    uint64_t _send_bytes = 0;

    std::unique_ptr<connection_factory> _new_connections;
    unsigned _total_connections = 0;
    unsigned _max_connections;
    condition_variable _not_full;
    connections_list_t _pool;

    future<connection_ptr> get_connection();
    future<> put_connection(connection_ptr con);
    future<> remove_connection(connection_ptr conn);
    future<> shrink_connections();

    template <typename Fn>
    SEASTAR_CONCEPT(requires std::invocable<Fn, connection&>)
    auto with_connection(Fn&& fn);

 public:
    /**
     * \brief Construct a simple client
     *
     * This creates a simple client that connects to provided address via plain
     * socket
     *
     * \param addr -- host address to connect to
     *
     */
    explicit client(socket_address addr);

    /**
     * \brief Construct a client with connection factory
     *
     * This creates a client that uses factory to get \ref connected_socket that is then
     * used as transport. The client may withdraw more than one socket from the factory and
     * may re-use the sockets on its own
     *
     * \param f -- the factory pointer
     *
     */
    explicit client(std::unique_ptr<connection_factory> f, unsigned max_connections = default_max_connections);

    /**
     * \brief Send the request and handle the response
     *
     * Sends the provided request to the client and calls the provided callback to handle
     * the response when it arrives. If the reply's status code is not equals the expected
     * value, the handler is not called and the method resolves with exceptional future.
     * Otherwise returns the handler's future
     *
     * \param req -- request to be sent
     * \param handle -- the response handler
     * \param expected -- the expected reply status code
     *
     */
    future<> make_request(request req, reply_handler handle, reply::status_type expected = reply::status_type::ok);

    /**
     * \brief Updates the maximum number of connections a client may have
     *
     * If the new limit is less than the amount of connections a client has, they will be
     * closed. The returned future resolves when all excessive connections get closed
     *
     * \param nr -- the new limit on the number of connections
     */
    future<> set_maximum_connections(unsigned nr);

    /**
     * \brief Closes the client
     *
     * Client must be closed before destruction unconditionally
     */
    future<> close();

    /**
     * \brief Returns the total number of connections
     */

    unsigned total_connections_count() const noexcept {
        return _total_connections;
    }

    unsigned current_connections_count() const noexcept {
        return _pool.size();
    }

    uint64_t recv_bytes() const;
    uint64_t send_bytes() const;
};

} // namespace internal

future<> ignore_reply(const request& req, const reply& rep, input_stream& in);

class dns_connection_factory : public internal::connection_factory {
 public:
    dns_connection_factory(const sstring& host, int port, float timeout = -1);

    virtual future<connected_socket> make() override;
    virtual socket_address remote_address() const override;

 protected:
    sstring _host;
    int _port;
    float _timeout;

    struct state {
        bool initialized = false;
        socket_address addr;
    };

    lw_shared_ptr<state> _state;
    shared_future<> _done;

    future<> initialize();
};

class client : public enable_shared_from_this<client> {
 public:
    client(socket_address sa, float timeout = -1);
    client(const sstring& address, float timeout = -1);
    client(const sstring& host, uint32_t port, float timeout = -1);

    virtual ~client();

    sstring host() const {
        return _host;
    }

    int port() const {
        return _port;
    }

    const std::unordered_map<scheduling_group, internal::client>& all_clients() {
        return _clients;
    }

    future<> make_request(
        request req,
        internal::reply_handler handle = ignore_reply,
        reply::status_type expected = reply::status_type::ok);
    future<> close();

 private:
    client(const std::tuple<sstring, uint32_t> address_parts, float timeout = -1);

    void for_each_client(std::function<void(internal::client&)> func);

    sstring _host;
    uint32_t _port;

    float _timeout;

    std::mutex _lock;
    std::unordered_map<scheduling_group, internal::client> _clients;
};

} // namespace rtmp
} // namespace amadeus
