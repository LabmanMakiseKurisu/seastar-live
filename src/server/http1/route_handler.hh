/*
 * @Author: Amadeus
 * @Date: 2024-05-10 16:59:40
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 17:24:30
 * @FilePath: /Amadeus/src/server/http1/route_handler.hh
 * @Description: 
 */
#pragma once

#include <seastar/http/file_handler.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/http/handlers.hh>
#include <seastar/http/httpd.hh>
#include <seastar/http/request.hh>
#include <seastar/http/transformers.hh>

#include "http1/response.hh"
#include "server/transmition.hh"
#include "session/session.hh"

namespace amadeus {
namespace http1 {
namespace route {

using namespace seastar;

namespace session_ns = amadeus::session;

class default_route_handler : public httpd::handler_base {
 public:
    default_route_handler() = default;
    virtual ~default_route_handler() = default;

    virtual future<std::unique_ptr<http::reply>>
    handle(const sstring &path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override;
};

class route_handler : public httpd::handler_base {
 public:
    route_handler(transmition_ptr trans)
    : _trans(trans) {}

    virtual ~route_handler() = default;

    transmition_ptr _trans;

    future<std::unique_ptr<http::reply>> play_stream(
        std::unique_ptr<http::request> req,
        std::unique_ptr<http::reply> rep,
        const sstring &app,
        const sstring &stream,
        format_t fmt,
        media_type_t media = media_type_t::all);
};

class play_stream_route_handler : public route_handler {
 public:
    play_stream_route_handler(transmition_ptr trans)
    : route_handler(trans) {}

    virtual ~play_stream_route_handler() = default;

    virtual future<std::unique_ptr<http::reply>>
    handle(const sstring &path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override;
};

} // namespace route
} // namespace http1
} // namespace amadeus
