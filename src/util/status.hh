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
 * Copyright 2023 bilibili
 */

#pragma once
#include <seastar/core/sstring.hh>
#include <seastar/util/log.hh>

namespace amadeus {

enum class status_t : unsigned int {
    ok = 0,
    failed,
    cancel,
    no_content,
    not_found,
    gone,
    redirect,
    timeout,
    bad_request,
    conflict,
    send_failed,
    recv_failed,
    connect,
    disconnect,
    internal_error,
};

struct status {
    status_t code = status_t::ok;
    seastar::sstring content = "";

    status() = default;

    status(const status &x)
    : code(x.code)
    , content(x.content) {}

    status(status &&x)
    : code(std::move(x.code))
    , content(std::move(x.content)) {}

    status(status_t c, seastar::sstring text = "")
    : code(c)
    , content(text) {}

    status &operator=(const status &x) {
        code = x.code;
        content = x.content;
        return *this;
    }

    status &operator=(status &&x) {
        code = std::move(x.code);
        content = std::move(x.content);
        return *this;
    }
};

class exception : public std::runtime_error {
 public:
    exception(status_t c, const seastar::sstring &reason = "")
    : std::runtime_error(reason)
    , code(c)
    , content(reason) {}

    exception(status st)
    : std::runtime_error(st.content)
    , code(st.code)
    , content(st.content) {}

    status_t code;
    seastar::sstring content;
};

seastar::sstring status_code_to_sstring(status_t c);

std::ostream &operator<<(std::ostream &os, status_t c);

std::ostream &operator<<(std::ostream &os, const status *v);

static inline std::ostream &
operator<<(std::ostream &os, const status &v) {
    return os << &v;
}

} // namespace amadeus
