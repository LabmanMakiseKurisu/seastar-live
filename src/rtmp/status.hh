/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:21
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 10:56:05
 * @FilePath: /Amadeus/src/rtmp/status.hh
 * @Description: 
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE f in the top-level directory.
 *
 *  This is an Auto-Generated-code
 *  Changes you do in this file will be erased on next code generation
 */

#pragma once

#include "util/status.hh"
#include "rtmp/reply.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

static inline const sstring
to_string(rtmp::reply::status_type status) {
    std::ostringstream oss;
    oss << status;
    return oss.str();
}

static inline rtmp::reply::status_type
to_tcp_status(status_t status) {
    switch (status) {
        case status_t::ok: return rtmp::reply::status_type::ok;
        case status_t::redirect: return rtmp::reply::status_type::internal_error;
        case status_t::not_found: return rtmp::reply::status_type::not_found;
        case status_t::conflict: return rtmp::reply::status_type::internal_error;
        case status_t::failed: return rtmp::reply::status_type::internal_error;
        case status_t::gone: return rtmp::reply::status_type::internal_error;
        case status_t::cancel: return rtmp::reply::status_type::internal_error;
        case status_t::no_content: return rtmp::reply::status_type::internal_error;
        case status_t::timeout: return rtmp::reply::status_type::internal_error;
        case status_t::bad_request: return rtmp::reply::status_type::internal_error;
        case status_t::send_failed: return rtmp::reply::status_type::internal_error;
        case status_t::recv_failed: return rtmp::reply::status_type::internal_error;
        case status_t::internal_error: return rtmp::reply::status_type::internal_error;
        default: return rtmp::reply::status_type::internal_error;
    }
}

static inline status_t
to_status(rtmp::reply::status_type status) {
    switch (status) {
        case rtmp::reply::status_type::ok: return status_t::ok;
        case rtmp::reply::status_type::not_found: return status_t::not_found;
        case rtmp::reply::status_type::internal_error: return status_t::internal_error;
        default: return status_t::internal_error;
    }
}

} // namespace rtmp
} // namespace amadeus
