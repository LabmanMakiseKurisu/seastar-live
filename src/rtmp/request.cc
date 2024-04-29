/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-29 15:13:43
 * @FilePath: /Amadeus/src/rtmp/request.cc
 * @Description: 
 */
#include "rtmp/request.hh"
#include "util/util.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

sstring
request::stream() {
    sstring path = stream_name;
    if (args.empty()) return stream_name;

    return stream_name + "?" + util::to_query_string(args);
}

void
request::write_body(noncopyable_function<future<>(const request &req, output_stream &)> &&body_writer) {
    this->_body_writer = std::move(body_writer);
}

void
request::read_body(noncopyable_function<future<>(const request &req, input_stream &)> &&body_reader) {
    this->_body_reader = std::move(body_reader);
}

request
request::make(mode m, const sstring &app_name, const sstring &stream_name, const sstring &tcurl) {
    request req;
    req._mode = m;
    req.app_name = app_name;
    req.stream_name = stream_name;
    req.tcurl = tcurl;

    return req;
}

} // namespace rtmp
} // namespace amadeus