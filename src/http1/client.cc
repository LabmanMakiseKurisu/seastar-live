// #include "http1/client.hh"

// #include <seastar/core/loop.hh>
// #include <seastar/core/seastar.hh>
// #include <seastar/core/shared_future.hh>
// #include <seastar/core/shared_ptr.hh>
// #include <seastar/core/when_all.hh>
// #include <seastar/http/exception.hh>
// #include <seastar/net/dns.hh>
// #include <seastar/util/short_streams.hh>

// #include "http1/client.hh"
// #include "http1/log.hh"
// #include "net/dns.hh"
// #include "util/util.hh"

// namespace amadeus {
// namespace http1 {

// using namespace seastar;

// future<>
// ignore_reply(const http::request &req, const http::reply &rep, input_stream<char> &&in) {
//     return do_with(std::move(in), [](input_stream<char> &in) {
//         return seastar::util::skip_entire_stream(in);
//     });
// }

// template <typename T>
// struct object_builder {
//     T data;

//     future<> parse(input_stream<char> &&is) {
//         return data.parse(std::move(is));
//     }
// };

// static inline sstring
// _port_to_string(int port) {
//     return port == 80 ? "" : (":" + to_sstring(port));
// }

// static inline sstring
// _vaidate_http_path(const sstring &path) {
//     return path.empty() ? "" : (path.substr(0, 1) == "/" ? path : ("/" + path));
// }

// static inline sstring
// _make_http_url(const sstring &host, uint32_t port, const sstring &path = "") {
//     return fmt::format("http://{}{}{}", host, _port_to_string(port), _vaidate_http_path(path));
// }

// client::client(socket_address address, float timeout)
// : client(_make_http_url(seastar::to_sstring(address.addr()), ntohl(address.u.in.sin_port)), timeout) {}

// client::client(const sstring &host, uint32_t port, const sstring &base_path, float timeout)
// : client(_make_http_url(host, port, base_path), timeout) {}

// client::client(const sstring &base_url, float timeout)
// : client(Url((base_url.compare(0, 4, "http") == 0) ? base_url : ("http://" + base_url)), timeout) {}

// client::client(const Url &base_url, float timeout)
// : _base_url(base_url)
// , _timeout(timeout) {
//     _stats_group.add_stats("http-client", [this] {
//         statistics::stats st;
//         for_each_client([&st](auto &cln) {
//             st.total_connections += cln.connections_nr();
//             st.current_connections += (cln.connections_nr() - cln.idle_connections_nr());
//             st.recv_bytes += cln.recv_bytes();
//             st.send_bytes += cln.send_bytes();
//         });
//         return st;
//     });
// }

// client::~client() {}

// void
// client::for_each_client(std::function<void(internal::client &)> func) {
//     std::lock_guard<std::mutex> g(_lock);

//     for (auto &e : _clients) func(e.second);
// }

// template <typename T>
// future<T>
// client::make_request(http::request req, http::reply::status_type expected) {
//     object_builder<T> builder;
//     return make_request(
//                std::move(req),
//                [&builder](const http::reply &rep, input_stream<char> &&body) {
//                    return builder.parse(std::move(body));
//                },
//                expected)
//         .then([&builder] {
//             return make_ready_future<T>(std::move(builder.data));
//         });
// }

// future<>
// client::make_request(http::request req, internal::client::reply_handler handle, http::reply::status_type expected) {
//     auto sg = current_scheduling_group();

//     _lock.lock();
//     auto it = _clients.find(sg);
//     if (it == _clients.end()) [[unlikely]] {
//         auto h = host();

//         assert(h.size());
//         if (h.empty()) return make_exception_future<>(httpd::bad_request_exception("host is empty"));

//         auto p = port().empty() ? 80 : std::stoi(port());
//         auto factory = std::make_unique<net::dns_connection_factory>(h, p, false, _timeout);
//         // Limit the maximum number of connections this group's http client
//         // may have proportional to its shares. Shares are typically in the
//         // range of 100...1000, thus resulting in 1..10 connections
//         auto max_connections = std::max((unsigned)(sg.get_shares() / 100), 1u);

//         it = _clients
//                  .emplace(
//                      std::piecewise_construct,
//                      std::forward_as_tuple(sg),
//                      std::forward_as_tuple(std::move(factory), max_connections))
//                  .first;
//     }
//     auto &cln = it->second;
//     _lock.unlock();

//     l.info(
//         "make request {} {}", req._method, req.get_protocol_name() + "://" + req.get_header("Host") + req.format_url());

//     return cln.make_request(std::move(req), std::move(handle), expected);
// }

// future<>
// client::close() {
//     return parallel_for_each(_clients, [](auto &it) -> future<> {
//         return it.second.close();
//     });
// }

// request_builder::request_builder() noexcept {}

// request_builder::request_builder(const Url &base_url, client *client) noexcept
// : _base_url(base_url)
// , _client(client) {}

// request_builder::request_builder(request_builder &&builder) noexcept
// : _base_url(std::move(builder._base_url))
// , _client(std::move(builder._client)) {}

// request_builder::~request_builder() {}

// request_builder &
// request_builder::operator=(request_builder &&builder) noexcept {
//     _base_url = std::move(builder._base_url);
//     _client = std::move(builder._client);

//     return *this;
// }

// http::request
// request_builder::make_request(
//     httpd::operation_type op,
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     std::unique_ptr<body> b) {
//     sstring base_path = _base_url.path();
//     sstring absolute_path = path.empty() ? base_path : (base_path == "/" ? path : (base_path + path));

//     auto req = http::request::make(op, util::make_host_string(_client->host(), _client->port()), absolute_path);
//     req._headers.insert(headers.begin(), headers.end());
//     req.query_parameters.insert(query.begin(), query.end());

//     b->handle(req);

//     return req;
// }

// template <typename T>
// future<T>
// request_builder::GET(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers) {
//     auto req = make_request(httpd::operation_type::GET, path, query, headers);
//     return _client->make_request<T>(std::move(req));
// }

// future<>
// request_builder::GET(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     internal::client::reply_handler reph) {
//     auto req = make_request(httpd::operation_type::GET, path, query, headers);
//     return _client->make_request(std::move(req), std::move(reph));
// }

// template <typename T>
// future<T>
// request_builder::POST(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     std::unique_ptr<body> b) {
//     auto req = make_request(httpd::operation_type::POST, path, query, headers, std::move(b));
//     return _client->make_request<T>(std::move(req));
// }

// future<>
// request_builder::POST(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     std::unique_ptr<body> b,
//     internal::client::reply_handler reph) {
//     auto req = make_request(httpd::operation_type::POST, path, query, headers, std::move(b));
//     return _client->make_request(std::move(req), std::move(reph));
// }

// future<>
// request_builder::HEAD(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers) {
//     auto req = make_request(httpd::operation_type::HEAD, path, query, headers);
//     return _client->make_request(std::move(req), ignore_reply);
// }

// template <typename T>
// future<T>
// request_builder::PUT(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     std::unique_ptr<body> b) {
//     auto req = make_request(httpd::operation_type::PUT, path, query, headers, std::move(b));
//     return _client->make_request<T>(std::move(req));
// }

// template <typename T>
// future<T>
// request_builder::OPTIONS(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers,
//     std::unique_ptr<body> b) {
//     auto req = make_request(httpd::operation_type::OPTIONS, path, query, headers, std::move(b));
//     return _client->make_request<T>(std::move(req));
// }

// template <typename T>
// future<T>
// request_builder::TRACE(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers) {
//     auto req = make_request(httpd::operation_type::TRACE, path, query, headers);
//     return _client->make_request<T>(std::move(req));
// }

// template <typename T>
// future<T>
// request_builder::CONNECT(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers) {
//     auto req = make_request(httpd::operation_type::CONNECT, path, query, headers);
//     return _client->make_request<T>(std::move(req));
// }

// template <typename T>
// future<T>
// request_builder::DELETE(
//     const sstring &path,
//     const std::unordered_map<sstring, sstring> &query,
//     const std::unordered_map<sstring, sstring> &headers) {
//     auto req = make_request(httpd::operation_type::DELETE, path, query, headers);
//     return _client->make_request<T>(std::move(req));
// }

// // static const sstring get_request_ip(http::request* req) {
// //     auto ip = req->get_header("X-Real-Ip");
// //     if (ip.empty()) {
// //         ip = req->get_header("X-Forwarded-For");
// //     }
// //     return ip;
// // }

// } // namespace http1
// } // namespace amadeus
