#pragma once

#include <seastar/http/reply.hh>

#include "util/status.hh"

namespace amadeus {
namespace http1 {

using namespace seastar;

static const sstring
to_string(http::reply::status_type status) {
    std::ostringstream oss;
    oss << status;
    return oss.str();
}

class http_error : public std::runtime_error {
 public:
    http_error(http::reply::status_type status, const sstring &reason = "", const sstring &addition = "")
    : std::runtime_error((reason.empty() ? to_string(status) : reason) + addition)
    , _status(status) {}

    http::reply::status_type _status;
};

static inline http::reply::status_type
to_http_status(status_t status) {
    switch (status) {
        case status_t::ok: return http::reply::status_type::ok;
        case status_t::gone: return http::reply::status_type::gone;
        case status_t::conflict: return http::reply::status_type::conflict;
        case status_t::not_found: return http::reply::status_type::not_found;
        case status_t::cancel: return http::reply::status_type::reset_content;
        case status_t::no_content: return http::reply::status_type::no_content;
        case status_t::timeout: return http::reply::status_type::request_timeout;
        case status_t::bad_request: return http::reply::status_type::bad_request;
        case status_t::redirect: return http::reply::status_type::temporary_redirect;
        case status_t::failed: return http::reply::status_type::internal_server_error;
        case status_t::send_failed: return http::reply::status_type::internal_server_error;
        case status_t::recv_failed: return http::reply::status_type::internal_server_error;
        case status_t::internal_error: return http::reply::status_type::internal_server_error;
        default: return http::reply::status_type::internal_server_error;
    }
}

static inline status_t
to_status(http::reply::status_type status) {
    switch (status) {
        case http::reply::status_type::ok: return status_t::ok;
        case http::reply::status_type::gone: return status_t::gone;
        case http::reply::status_type::conflict: return status_t::conflict;
        case http::reply::status_type::not_found: return status_t::not_found;
        case http::reply::status_type::reset_content: return status_t::cancel;
        case http::reply::status_type::no_content: return status_t::no_content;
        case http::reply::status_type::request_timeout: return status_t::timeout;
        case http::reply::status_type::bad_request: return status_t::bad_request;
        case http::reply::status_type::temporary_redirect: return status_t::redirect;
        case http::reply::status_type::internal_server_error: return status_t::internal_error;
        default: return status_t::internal_error;
    }
}

} // namespace http1
} // namespace amadeus
