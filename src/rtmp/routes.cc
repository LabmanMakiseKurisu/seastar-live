/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 11:04:58
 * @FilePath: /Amadeus/src/rtmp/routes.cc
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

#include "rtmp/routes.hh"

#include <seastar/http/json_path.hh>

#include "rtmp/exception.hh"
#include "rtmp/reply.hh"
#include "rtmp/request.hh"

namespace amadeus {
namespace rtmp {

using namespace std;
using namespace seastar;

routes::routes()
: _general_handler([this](std::exception_ptr eptr) mutable {
    return exception_reply(eptr);
}) {}

routes::~routes() {
    for (int i = 0; i < request::mode::NUM_MODE; i++) {
        if (_handlers[i]) delete _handlers[i];
    }
}

std::unique_ptr<rtmp::reply>
routes::exception_reply(std::exception_ptr eptr) {
    auto rep = std::make_unique<rtmp::reply>();
    try {
        // go over the register exception handler
        // if one of them handle the exception, return.
        for (auto e : _exceptions) {
            try {
                return e.second(eptr);
            } catch (...) {
                // this is needed if there are more then one register exception handler
                // so if the exception handler throw a new exception, they would
                // get the new exception and not the original one.
                eptr = std::current_exception();
            }
        }
        std::rethrow_exception(eptr);
    } catch (const base_exception& e) { rep->set_status(e.status()); } catch (...) {
        rep->set_status(rtmp::reply::status_type::internal_error);
    }

    rep->done();
    return rep;
}

future<std::unique_ptr<rtmp::reply>>
routes::handle(request::mode m, std::unique_ptr<rtmp::request>& req, std::unique_ptr<rtmp::reply> rep) {
    handler_base* handler = get_handler(m);
    if (handler != nullptr) {
        try {
            auto r = handler->handle(req, std::move(rep));
            return r.handle_exception(_general_handler);
        } catch (const redirect_exception& _e) {
            rep.reset(new rtmp::reply());
            rep->set_status(_e.status()).done();
        } catch (...) { rep = exception_reply(std::current_exception()); }
    } else {
        rep.reset(new rtmp::reply());
        rep->set_status(rtmp::reply::status_type::not_found).done();
    }
    return make_ready_future<std::unique_ptr<rtmp::reply>>(std::move(rep));
}

} // namespace rtmp
} // namespace amadeus
