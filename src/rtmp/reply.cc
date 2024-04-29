/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-29 15:14:55
 * @FilePath: /Amadeus/src/rtmp/reply.cc
 * @Description: 
 */
#include "rtmp/reply.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

namespace status_strings {

const sstring ok = "OK";
const sstring not_found = "Not Found";
const sstring internal_error = "Internal Error";

static const sstring&
to_string(reply::status_type status) {
    switch (status) {
        case reply::status_type::ok: return ok;
        case reply::status_type::not_found: return not_found;
        default: return internal_error;
    }
}

} // namespace status_strings

std::ostream&
operator<<(std::ostream& os, reply::status_type st) {
    return os << status_strings::to_string(st);
}

void
reply::write_body(noncopyable_function<future<>(const reply& rep, output_stream&)>&& body_writer) {
    _body_writer = std::move(body_writer);
}

void
reply::read_body(noncopyable_function<future<>(const reply& rep, input_stream&)>&& body_reader) {
    _body_reader = std::move(body_reader);
}

} // namespace rtmp
} // namespace amadeus
