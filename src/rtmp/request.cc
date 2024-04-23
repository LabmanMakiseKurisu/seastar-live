/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 11:01:04
 * @FilePath: /Amadeus/src/rtmp/request.cc
 * @Description: 
 */
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
 * Copyright (C) 2022 Scylladb, Ltd.
 */

#include "rtmp/request.hh"

#include "rtmp/stream.hh"
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