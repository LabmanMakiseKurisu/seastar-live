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

#include <boost/algorithm/string.hpp>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/sstring.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/net/inet_address.hh>

#include "util/enums.hh"

using namespace seastar;

namespace amadeus {
using arguments_t = std::unordered_map<sstring, sstring>;

namespace util {

sstring generate_uuid();
sstring generate_sequence();
sstring generate_stream_index(const sstring &app, const sstring &stream);

sstring generate_session_id(const sstring &stream_id, const sstring &extra = "");

unsigned int cal_crc32(unsigned int value, const char *buf, size_t len, size_t interval = 1);
unsigned int cal_crc32(unsigned int value, const std::vector<char> &v, size_t interval = 1);

unsigned int cal_crc32(const char *buf, size_t len, size_t interval = 1);
unsigned int cal_crc32(const std::vector<char> &v, size_t interval = 1);

sstring to_query_string(const std::unordered_map<sstring, sstring> &query);
std::unordered_map<sstring, sstring> to_query(const sstring &str);

sstring make_host_string(const sstring &host, const sstring &port);
std::tuple<sstring, uint32_t> split_address(const sstring &address);

uint32_t default_port(protocol_t prot);
socket_address make_socket_address(protocol_t prot, const sstring &address);

sstring make_valid_ip_address(protocol_t prot, const sstring &address);

int get_local_ip_addresses(int ipv4_6, std::vector<sstring> &ip_addresses);

bool is_local_server_ip_address(protocol_t prot, const sstring &address);
bool is_local_server_ip_address(protocol_t prot, const sstring &address, const std::vector<sstring> &local_hosts);

bool is_ip_address_equal(const sstring &lhs, const sstring &rhs);
bool is_ip_address_equal(protocol_t prot, const sstring &lhs, const sstring &rhs);

bool parse_stream_url(
    const sstring &url_string,
    sstring &app,
    sstring &stream,
    sstring &address,
    sstring &internal_url,
    arguments_t &args,
    protocol_t &prot);

bool is_true(const sstring &value);
bool is_false(const sstring &value);


} // namespace util
} // namespace amadeus

static inline std::ostream &
operator<<(std::ostream &os, const std::filesystem::path &p) {
    os << p.native();
    return os;
}

static inline std::ostream &
operator<<(std::ostream &os, const std::vector<uint8_t> &v) {
    for (auto c : v) { os << fmt::format("{:02x}", c); }
    return os;
}

namespace seastar {

static inline sstring
to_sstring(const std::vector<uint8_t> &v) {
    sstring str;
    for (auto c : v) { str += fmt::format("{:02x}", c); }

    return str;
}

static inline sstring
to_sstring(const seastar::net::inet_address &v) {
    std::stringstream ss;
    ss << v;

    return ss.str();
}

} // namespace seastar
