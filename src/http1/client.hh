// #pragma once

// #include <CxxUrl/CxxUrl.hpp>
// #include <seastar/core/sstring.hh>
// #include <seastar/http/client.hh>
// #include <seastar/http/httpd.hh>

// #include "http1/status.hh"
// #include "stats/stats.hh"

// namespace com {
// namespace bilibili {
// namespace http1 {

// namespace internal {
// using client = http::experimental::client;
// } // namespace internal

// using namespace seastar;

// future<> ignore_reply(const http::request &req, const http::reply &rep, input_stream<char> &&in);

// struct ignored_object {
//     future<> parse(input_stream<char> &&is) {
//         return make_ready_future<>();
//     }
// };

// class body {
//  public:
//     virtual ~body() = default;

//     virtual void handle(http::request &req) = 0;
// };

// class content_body : public body {
//  public:
//     explicit content_body(sstring content_type, sstring content)
//     : _content_type(std::move(content_type))
//     , _content(std::move(content)) {}

//  protected:
//     sstring _content_type;
//     sstring _content;

//     virtual void handle(http::request &req) override {
//         req.write_body(std::move(_content_type), std::move(_content));
//     }
// };

// class stream_body : public body {
//  public:
//     explicit stream_body(
//         sstring content_type,
//         noncopyable_function<future<>(const http::request &req, output_stream<char> &&)> body_writer)
//     : _content_type(std::move(content_type))
//     , _body_writer(std::move(body_writer)) {}

//  protected:
//     sstring _content_type;
//     noncopyable_function<future<>(const http::request &req, output_stream<char> &&)> _body_writer;

//     virtual void handle(http::request &req) override {
//         req.write_body(
//             std::move(_content_type),
//             [&req, writer = std::move(_body_writer)](const http::request &rq, output_stream<char> &&out) {
//                 return writer(rq, std::move(out));
//             });
//     }
// };

// class body_ignore : public body {
//     virtual void handle(http::request &req) override {}

//  public:
//     explicit body_ignore() = default;
// };

// class client;

// struct request_builder {
//  private:
//     Url _base_url;

//     client *_client = nullptr;

//     http::request make_request(
//         httpd::operation_type op,
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query,
//         const std::unordered_map<sstring, sstring> &headers,
//         std::unique_ptr<body> b = std::make_unique<body_ignore>());

//  public:
//     request_builder() noexcept;
//     request_builder(request_builder &&builder) noexcept;
//     explicit request_builder(const Url &base_url, client *client) noexcept;

//     ~request_builder();

//     request_builder &operator=(request_builder &&builder) noexcept;

//     template <typename T>
//     future<T>
//     GET(const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {});

//     future<>
//     GET(const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {},
//         internal::client::reply_handler reph = ignore_reply);

//     template <typename T>
//     future<T> POST(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {},
//         std::unique_ptr<body> b = std::make_unique<body_ignore>());

//     future<> POST(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {},
//         std::unique_ptr<body> b = std::make_unique<body_ignore>(),
//         internal::client::reply_handler reph = ignore_reply);

//     future<> HEAD(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {});

//     template <typename T>
//     future<T>
//     PUT(const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {},
//         std::unique_ptr<body> b = std::make_unique<body_ignore>());

//     template <typename T>
//     future<T> OPTIONS(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {},
//         std::unique_ptr<body> b = std::make_unique<body_ignore>());

//     template <typename T>
//     future<T> TRACE(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {});

//     template <typename T>
//     future<T> CONNECT(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {});

//     template <typename T>
//     future<T> DELETE(
//         const sstring &path,
//         const std::unordered_map<sstring, sstring> &query = {},
//         const std::unordered_map<sstring, sstring> &headers = {});
// };

// class client : public enable_shared_from_this<client> {
//  public:
//     client(socket_address address, float timeout = -1);
//     client(const sstring &base_url, float timeout = -1);
//     client(const sstring &host, uint32_t port, const sstring &base_path, float timeout = -1);
//     virtual ~client();

//     sstring host() const {
//         return _base_url.host();
//     }

//     sstring port() const {
//         return _base_url.port();
//     }

//     const std::unordered_map<scheduling_group, internal::client> &all_clients() {
//         std::lock_guard<std::mutex> g(_lock);
//         return _clients;
//     }

//     template <typename T>
//     future<T> make_request(http::request req, http::reply::status_type expected = http::reply::status_type::ok);

//     future<> make_request(
//         http::request req,
//         internal::client::reply_handler handle = ignore_reply,
//         http::reply::status_type expected = http::reply::status_type::ok);
//     future<> close();

//     request_builder build() {
//         return request_builder(_base_url, this);
//     }

//  protected:
//     client(const Url &base_url, float timeout = -1);

//     void for_each_client(std::function<void(internal::client &)> func);

//     Url _base_url;
//     float _timeout;

//     std::mutex _lock;
//     std::unordered_map<scheduling_group, internal::client> _clients;

//     statistics::stats_group _stats_group;
// };

// } // namespace http1
// } // namespace bilibili
// } // namespace com
