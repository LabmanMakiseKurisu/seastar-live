/*
 * @Author: Amadeus
 * @Date: 2024-05-10 17:06:13
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 14:06:04
 * @FilePath: /Amadeus/src/session/http1/play_session.hh
 * @Description: 
 */
#pragma once

#include "session/rtmp/play_session.hh"
#include "session/play_session.hh"

namespace amadeus {
namespace http1 {
namespace session {

using namespace seastar;

namespace session_ns = amadeus::session;
namespace rtmp_session = amadeus::rtmp::session;
using publisher_ptr = session_ns::publisher_ptr;

namespace svr {

class play_session : public session_ns::svr::play_session {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);
    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);
    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    virtual ~play_session() = default;

    virtual void subscribe() override;
    virtual void unsubscribe() override;
};

} // namespace svr

} // namespace session
} // namespace http1
} // namespace amadeus
