/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2015 Cloudius Systems
 */

//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/util/string_utils.hh>

#include "rtmp/stream.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

class input_stream;
class output_stream;

/**
 * A request received from a client.
 */
struct request {
    socket_address _remote_address; // for server

    // Bvc Request Information
    enum mode {
        publish = 0,
        play,
        live_only,
        vod_only,

        NUM_MODE
    };

    mode _mode = mode::publish;

    sstring app_name;
    sstring stream_name;

    enum type {
        live = 0,
        record,
        append,

        ignored,

        NUM_TYPE
    };

    type _type = type::ignored; // for server publish

    double start = 0;    // for server play
    double duration = 0; // for server play
    uint8_t reset = 0;   // for server play

    sstring tcurl;

    std::unordered_map<sstring, sstring> args;

    sstring stream();

    /*
     * The handler should read the contents of this stream till reaching eof (i.e., the end of this request's content).
     * Failing to do so will force the server to close this connection, and the client will not be able to reuse this
     * connection for the next request. The stream should not be closed by the handler, the server will close it for the
     * handler.
     * */
    noncopyable_function<future<>(const request &req, output_stream &)> _body_writer; // for client
    noncopyable_function<future<>(const request &req, input_stream &)> _body_reader;  // for server

    noncopyable_function<size_t()> _read_bytes_provider;
    noncopyable_function<size_t()> _write_bytes_provider;

    /**
     * \brief Use an output stream to write the message body
     *
     * When a handler needs to use an output stream it should call this method
     * with a function.
     *
     *  you would have used for such a content, (i.e. "txt", "html", "json", etc')
     * \param body_writer - a function that accept an output stream and use that stream to write the body.
     *   The function should take ownership of the stream while using it and must close the stream when it
     *   is done.
     *
     * This method can be used to write body of unknown or hard to evaluate length. For example,
     * when sending the contents of some other input_stream or when the body is available as a
     * collection of memory buffers. Message would use chunked transfer encoding.
     *
     */
    void write_body(noncopyable_function<future<>(const request &req, output_stream &)> &&body_writer);
    void read_body(noncopyable_function<future<>(const request &req, input_stream &)> &&body_reader);

    static request make(mode m, const sstring &app_name, const sstring &stream_name, const sstring &tcurl = "");

    friend std::ostream &operator<<(std::ostream &os, const request *v) {
        os << "mode: " << v->_mode;
        os << " app_name: " << v->app_name;
        os << " stream_name: " << v->stream_name;
        os << " _type: " << v->_type;
        os << " start: " << to_sstring(v->start);
        os << " duration: " << to_sstring(v->duration);
        os << " reset: " << static_cast<int>(v->reset);
        os << " tcurl: " << v->tcurl;
        os << " args: " << v->args;
        return os;
    }

    friend std::ostream &operator<<(std::ostream &os, const request &v) {
        return os << &v;
    }
};

static inline std::ostream &
operator<<(std::ostream &os, const std::vector<unsigned char> &v) {
    for (auto c : v) { os << fmt::format("{:02x}", c); }
    return os;
}

} // namespace rtmp

} // namespace amadeus
