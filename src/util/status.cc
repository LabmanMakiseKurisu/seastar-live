#include "util/status.hh"

namespace amadeus {

seastar::sstring
status_code_to_sstring(status_t c) {
    switch (c) {
        case status_t::ok: return "ok";
        case status_t::not_found: return "not found";
        case status_t::gone: return "gone";
        case status_t::timeout: return "timeout";
        case status_t::redirect: return "redirect";
        case status_t::bad_request: return "bad request";
        case status_t::send_failed: return "send failed";
        case status_t::recv_failed: return "recv failed";
        case status_t::connect: return "connect";
        case status_t::disconnect: return "disconnect";
        case status_t::internal_error: return "internal error";
        default: return "internal error";
    }
}

std::ostream &
operator<<(std::ostream &os, status_t c) {
    return os << status_code_to_sstring(c);
}

std::ostream &
operator<<(std::ostream &os, const status *v) {
    if (!v) return os;

    os << v->code;
    os << "(";
    os << static_cast<int>(v->code);
    os << "): ";
    os << v->content;

    return os;
}

} // namespace amadeus