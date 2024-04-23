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
#include <seastar/util/log.hh>
#include <seastar/util/modules.hh>

#include "rtmp/reply.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

/**
 * The base_exception is a base for all rtmp exception.
 * It contains a message that will be return as the message content
 * and a status that will be return as a status code.
 */
class base_exception : public std::exception {
 public:
    base_exception(const std::string& msg, reply::status_type status)
    : _msg(msg)
    , _status(status) {}

    virtual const char* what() const throw() {
        return _msg.c_str();
    }

    reply::status_type status() const {
        return _status;
    }

    virtual const std::string& str() const {
        return _msg;
    }

 private:
    std::string _msg;
    reply::status_type _status;
};

/**
 * Throwing this exception will result in a redirect to the given url
 */
class redirect_exception : public base_exception {
 public:
    redirect_exception(const std::string& url)
    : base_exception("", reply::status_type::internal_error)
    , url(url) {}

    std::string url;
};

/**
 * Throwing this exception will result in a 404 not found result
 */
class not_found_exception : public base_exception {
 public:
    not_found_exception(const std::string& msg = "Not found")
    : base_exception(msg, reply::status_type::internal_error) {}
};

class server_error_exception : public base_exception {
 public:
    server_error_exception(const std::string& msg)
    : base_exception(msg, reply::status_type::internal_error) {}
};

/**
 * Client-side exception to report unexpected server reply status
 */
class unexpected_status_error : public base_exception {
 public:
    unexpected_status_error(reply::status_type st)
    : base_exception("Unexpected reply status", st) {}
};

/**
 * Throwing this exception will result in a 400 bad request result
 */

class bad_request_exception : public base_exception {
 public:
    bad_request_exception(const std::string& msg)
    : base_exception(msg, reply::status_type::internal_error) {}
};

static inline std::exception_ptr
make_nested_exception(std::vector<std::exception_ptr> exceptions) {
    if (exceptions.empty()) return nullptr;
    if (exceptions.size() == 1) return exceptions.front();
    if (exceptions.size() == 2) return std::make_exception_ptr(nested_exception(exceptions.front(), exceptions.back()));

    std::exception_ptr rt = std::make_exception_ptr(nested_exception(exceptions[0], exceptions[1]));
    for (size_t i = 2; i < exceptions.size(); i++) rt = std::make_exception_ptr(nested_exception(rt, exceptions[i]));

    return rt;
}

} // namespace rtmp
} // namespace amadeus
