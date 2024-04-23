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

#include <map>

#include "rtmp/handlers.hh"
#include "rtmp/reply.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

/**
 * routes object do the request dispatching according to the url.
 * It uses two decision mechanism exact match, if a url matches exactly
 * (an optional leading slash is permitted) it is choosen
 * If not, the matching rules are used.
 * matching rules are evaluated by their insertion order
 */
class routes {
 public:
    using exception_handler_fun = std::function<std::unique_ptr<rtmp::reply>(std::exception_ptr eptr)>;
    using exception_handler_id = size_t;

    routes();
    /**
     * The destructor deletes the match rules and handlers
     */
    ~routes();

    /**
     * adding a handler as an exact match
     * @param url the url to match (note that url should start with /)
     * @param handler the desire handler
     * @return it self
     */
    routes& put(request::mode m, handler_base* handler) {
        if (_handlers[m]) throw std::runtime_error(format("Handler for {} already exists.", m));

        _handlers[m] = handler;
        return *this;
    }

    /**
     * removing a handler from exact match
     * @param url the url to match (note that url should start with /)
     * @return the current handler (to be removed by caller)
     */
    handler_base* drop(request::mode m) {
        auto handler = _handlers[m];
        _handlers[m] = nullptr;

        return handler;
    }

    /**
     * Add a url match to a handler:
     * Example  routes.add(GET, url("/api").remainder("path"), handler);
     * @param m
     * @param url
     * @param handler
     * @return
     */
    routes& set(request::mode m, handler_base* handler) {
        _handlers[m] = handler;
        return *this;
    }

    /**
     * Add a default handler - handles any HTTP Method and Path (/\*) combination:
     * Example  routes.add_default_handler(handler);
     * @param handler
     * @return
     */
    routes& set_default_handler(handler_base* handler) {
        _default_handler = handler;
        return *this;
    }

    /**
     * the main entry point.
     * the general handler calls this method with the request
     * the method takes the headers from the request and find the
     * right handler.
     * It then call the handler with the args (if they exists) found in the url
     * @param m mode
     * @param req the rtmp request
     * @param rep the rtmp reply
     */
    future<std::unique_ptr<rtmp::reply>>
    handle(request::mode m, std::unique_ptr<request>& req, std::unique_ptr<rtmp::reply> rep);

    /**
     * Search and return a handler by the operation m and url
     * @param m the rtmp operation m
     * @param params a parameter object that will be filled during the match
     * @return a handler based on the m/url match
     */
    handler_base* get_handler(request::mode m) {
        handler_base* handler = _handlers[m];
        if (handler != nullptr) return handler;

        return _default_handler;
    }

    /**
     * The exception_handler_fun expect to call
     * std::rethrow_exception(eptr);
     * and catch only the exception it handles
     */
    exception_handler_id register_exeption_handler(exception_handler_fun fun) {
        auto current = _exception_id++;
        _exceptions[current] = fun;
        return current;
    }

    void remove_exception_handler(exception_handler_id id) {
        _exceptions.erase(id);
    }

    std::unique_ptr<rtmp::reply> exception_reply(std::exception_ptr eptr);

 private:
    // default Handler -- for any HTTP Method and Path (/*)
    handler_base* _default_handler = nullptr;
    handler_base* _handlers[request::mode::NUM_MODE] = {0};

    std::map<exception_handler_id, exception_handler_fun> _exceptions;
    exception_handler_id _exception_id = 0;
    // for optimization reason, the lambda function
    // that calls the exception_reply of the current object
    // is stored
    exception_handler_fun _general_handler;
};

} // namespace rtmp
} // namespace amadeus