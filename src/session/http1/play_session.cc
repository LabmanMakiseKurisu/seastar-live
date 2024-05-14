/*
 * @Author: Amadeus
 * @Date: 2024-05-10 17:06:13
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 14:37:09
 * @FilePath: /Amadeus/src/session/http1/play_session.cc
 * @Description:
 */
#include "session/http1/play_session.hh"

#include <seastar/core/thread.hh>

#include "http1/http.hh"
#include "session/log.hh"
#include "util/util.hh"
#include "session/rtmp/publish_session.hh"

namespace amadeus {
namespace http1 {
namespace session {

using namespace seastar;
using namespace session_ns;

namespace svr {

play_session::play_session(
    publisher_ptr pub,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, id, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: session_ns::svr::play_session(
      pub, id, app, stream, internal_url, args, address, os, fmt, media_type, protocol_t::HTTP1) {}

void
play_session::subscribe() {
    auto _rtmp_pub = std::dynamic_pointer_cast<rtmp::session::publish_session>(_pub);
    if (_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP) {
        auto self = std::dynamic_pointer_cast<amadeus::session::subscriber>(shared_from_this());
        _delay = _rtmp_pub->delay();
        _rtmp_pub->add_rtmp_subscriber(self);
    } else {
        session_ns::play_session::subscribe();
    }
}

void
play_session::unsubscribe() {
    auto _rtmp_pub = std::dynamic_pointer_cast<rtmp::session::publish_session>(_pub);
    if (_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP) {
        _rtmp_pub->remove_rtmp_subscriber(std::dynamic_pointer_cast<amadeus::session::subscriber>(shared_from_this()));
    } else {
        session_ns::play_session::unsubscribe();
    }
}

} // namespace svr

} // namespace session
} // namespace http1
} // namespace amadeus
