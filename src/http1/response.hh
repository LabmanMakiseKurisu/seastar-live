#pragma once

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <seastar/core/sstring.hh>
#include <seastar/http/reply.hh>
#include <seastar/json/json_elements.hh>

#include "http1/status.hh"

namespace amadeus {
namespace http1 {

using http_status_type = http::reply::status_type;

/**
 * response info object
 */
struct response {
 public:
    /**
     * status
     */
    http_status_type status;

    /**
     * response detail
     */
    sstring content;

    sstring content_type = "html";

    response() = default;

    response(http_status_type status, const sstring &content, const sstring &content_type) {
        this->status = status;
        this->content = content;
        this->content_type = content_type;
    }

    response(const response &e) {
        status = e.status;
        content = e.content;
        content_type = e.content_type;
    }

    response &operator=(const response &e) {
        status = e.status;
        content = e.content;
        content_type = e.content_type;
        return *this;
    }

    response &operator=(response &e) {
        status = e.status;
        content = e.content;
        content_type = e.content_type;
        return *this;
    }
};

namespace resp {

static inline response
make(http_status_type code, const sstring &content = "", const sstring &content_type = "html") {
    return response(code, content.empty() ? to_string(code) : content, content_type);
}

static inline response
make(status_t code, const sstring &content = "", const sstring &content_type = "html") {
    return response(to_http_status(code), content.empty() ? to_string(to_http_status(code)) : content, content_type);
}

static inline response
make(exception e) {
    return make(e.code, e.content);
}

static inline response
make(status st) {
    return make(st.code, st.content);
}

static inline response
make_not_found(const sstring &content = "") {
    return make(http_status_type::not_found, content);
}

static inline response
make_no_content(const sstring &content = "") {
    return make(http_status_type::no_content, content);
}

static inline response
make_conflict(const sstring &content = "") {
    return make(http_status_type::conflict, content);
}

static inline response
make_internal_server_error(const sstring &content = "") {
    return make(http_status_type::internal_server_error, content);
}

static inline response
make_success(const sstring &content = "") {
    return make(http_status_type::ok, content);
}

static inline response
make_json(nlohmann::json j) {
    return make(http_status_type::ok, j.dump(), "json");
}

static inline response
make_json_string(const sstring &content) {
    return make(http_status_type::ok, content, "json");
}

static inline response
make_success(const std::vector<sstring> &l) {
    return make(http_status_type::ok, "[" + boost::algorithm::join(l, ",") + "]");
}

static const response success = make_success();
static const response not_found = make_not_found();
static const response internal_server_error = make_internal_server_error();
static const response switching_protocols = make(http_status_type::switching_protocols);
static const response created = make(http_status_type::created);
static const response accepted = make(http_status_type::accepted);
static const response nonauthoritative_information = make(http_status_type::nonauthoritative_information);
static const response no_content = make(http_status_type::no_content);
static const response reset_content = make(http_status_type::reset_content);
static const response partial_content = make(http_status_type::partial_content);
static const response multiple_choices = make(http_status_type::multiple_choices);
static const response moved_permanently = make(http_status_type::moved_permanently);
static const response moved_temporarily = make(http_status_type::moved_temporarily);
static const response see_other = make(http_status_type::see_other);
static const response not_modified = make(http_status_type::not_modified);
static const response use_proxy = make(http_status_type::use_proxy);
static const response temporary_redirect = make(http_status_type::temporary_redirect);
static const response bad_request = make(http_status_type::bad_request);
static const response unauthorized = make(http_status_type::unauthorized);
static const response payment_required = make(http_status_type::payment_required);
static const response forbidden = make(http_status_type::forbidden);
static const response method_not_allowed = make(http_status_type::method_not_allowed);
static const response not_acceptable = make(http_status_type::not_acceptable);
static const response request_timeout = make(http_status_type::request_timeout);
static const response conflict = make(http_status_type::conflict);
static const response gone = make(http_status_type::gone);
static const response length_required = make(http_status_type::length_required);
static const response payload_too_large = make(http_status_type::payload_too_large);
static const response uri_too_long = make(http_status_type::uri_too_long);
static const response unsupported_media_type = make(http_status_type::unsupported_media_type);
static const response expectation_failed = make(http_status_type::expectation_failed);
static const response unprocessable_entity = make(http_status_type::unprocessable_entity);
static const response upgrade_required = make(http_status_type::upgrade_required);
static const response too_many_requests = make(http_status_type::too_many_requests);
static const response not_implemented = make(http_status_type::not_implemented);
static const response bad_gateway = make(http_status_type::bad_gateway);
static const response service_unavailable = make(http_status_type::service_unavailable);
static const response gateway_timeout = make(http_status_type::gateway_timeout);
static const response http_version_not_supported = make(http_status_type::http_version_not_supported);
static const response insufficient_storage = make(http_status_type::insufficient_storage);

} // namespace resp

} // namespace http1
} // namespace bilibili
