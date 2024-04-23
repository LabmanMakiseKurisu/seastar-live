/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 14:25:48
 * @FilePath: /Amadeus/src/rtmp/function_handlers.hh
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
 * Copyright 2015 Cloudius Systems
 */

#pragma once

#include <seastar/json/json_elements.hh>

#include <functional>

#include "rtmp/handlers.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

/**
 * A request function is a lambda expression that gets only the request
 * as its parameter
 */
typedef std::function<void(const_req req)> request_function;

/**
 * A handle function is a lambda expression that gets request and reply
 */
typedef std::function<void(const_req req, rtmp::reply &)> handle_function;

typedef std::function<future<std::unique_ptr<rtmp::reply>>(
    std::unique_ptr<rtmp::request> &req, std::unique_ptr<rtmp::reply> rep)>
    future_handler_function;

/**
 * The function handler get a lambda expression in the constructor.
 * it will call that expression to get the result
 * This is suited for very simple handlers
 *
 */
class function_handler : public handler_base {
 public:
    function_handler(const handle_function &f_handle)
    : _f_handle([f_handle](std::unique_ptr<rtmp::request> &req, std::unique_ptr<rtmp::reply> rep) {
        f_handle(*req.get(), *rep.get());
        return make_ready_future<std::unique_ptr<rtmp::reply>>(std::move(rep));
    }) {}

    function_handler(const future_handler_function &f_handle)
    : _f_handle(f_handle) {}

    function_handler(const request_function &_handle)
    : _f_handle([_handle](std::unique_ptr<rtmp::request> &req, std::unique_ptr<rtmp::reply> rep) {
        _handle(*req.get());
        return make_ready_future<std::unique_ptr<rtmp::reply>>(std::move(rep));
    }) {}

    function_handler(const function_handler &) = default;

    future<std::unique_ptr<rtmp::reply>>
    handle(std::unique_ptr<rtmp::request> &req, std::unique_ptr<rtmp::reply> rep) override {
        return _f_handle(std::move(req), std::move(rep)).then([this](std::unique_ptr<rtmp::reply> rep) {
            rep->done();
            return make_ready_future<std::unique_ptr<rtmp::reply>>(std::move(rep));
        });
    }

 protected:
    future_handler_function _f_handle;
};

} // namespace rtmp
} // namespace amadeus
