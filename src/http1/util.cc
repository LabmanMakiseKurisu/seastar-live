#include "http1/util.hh"

#include "util/util.hh"

namespace amadeus {
namespace http1 {

using namespace seastar;

bool
parse_remote_stream_request(
    const std::unique_ptr<http::request> &req,
    sstring &app,
    sstring &stream,
    sstring &address,
    sstring &internal_url,
    arguments_t &args,
    protocol_t &prot) {
    app = req->get_query_param("remote_app") != "" ? req->get_query_param("remote_app") : app;
    stream = req->get_query_param("remote_stream") != "" ? req->get_query_param("remote_stream") : stream;
    address = req->get_query_param("remote_address") != "" ? req->get_query_param("remote_address") : address;
    args = req->get_query_param("remote_args") != "" ? util::to_query(req->get_query_param("remote_args")) : args;
    internal_url =
        req->get_query_param("remote_internal_url") != "" ? req->get_query_param("remote_internal_url") : internal_url;

    prot = req->get_query_param("protocol") != "" ? str2protocol(req->get_query_param("protocol")) : prot;

    auto remote_url = req->get_query_param("remote_url");
    if (remote_url.size()) {
        bool success = util::parse_stream_url(remote_url, app, stream, address, internal_url, args, prot);
        if (!success) return false;
    }

    return true;
}

void
get_query(const Url &url, arguments_t &args) {
    for (auto kv : url.query()) { args[kv.key()] = kv.val(); }
}

arguments_t
get_query(const Url &url) {
    arguments_t args;
    get_query(url, args);
    return args;
}

arguments_t
parse_request_argumets(const std::unique_ptr<http::request> &req) {
    arguments_t args;
    if (req->_url.size()) {
        Url url(req->_url);
        get_query(url, args);
    }

    auto internal_url = req->get_header(""); // TODO?
    if (internal_url.size()) {
        Url url(internal_url);
        get_query(url, args);
    }
    return args;
}

sstring
get_localtion_url(const std::unique_ptr<http::request> &req) {
    return get_localtion_url(req->_headers);
}

std::vector<sstring>
get_localtion_urls(const std::unique_ptr<http::request> &req) {
    return get_localtion_urls(req->_headers);
}

sstring
get_localtion_url(const std::unique_ptr<http::reply> &rep) {
    return get_localtion_url(rep->_headers);
}

std::vector<sstring>
get_localtion_urls(const std::unique_ptr<http::reply> &rep) {
    return get_localtion_urls(rep->_headers);
}

template <typename T>
sstring
get_localtion_url(const T &headers) {
    auto res = headers.find("Location");
    if (res != headers.end()) return res->second;

    res = headers.find("location");
    if (res != headers.end()) return res->second;

    return "";
}

template <typename T>
std::vector<sstring>
get_localtion_urls(const T &headers) {
    std::vector<sstring> locations;
    for (auto e : headers) {
        sstring low(e.first.size(), 0);
        std::transform(e.first.begin(), e.first.end(), low.begin(), ::tolower);

        auto contains = low.find("location") != sstring::npos;
        if (!contains) continue;

        locations.push_back(e.second);
    }
    return locations;
}

} // namespace http1
} // namespace amadeus
