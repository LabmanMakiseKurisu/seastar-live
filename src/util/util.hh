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
#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
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

namespace fs = std::filesystem;

template <typename Action>
future<>
with_file_output_stream(fs::path filepath, open_flags flags, Action action) {
    auto tmp_file = filepath.string() + ".tmp";

    return open_file_dma(tmp_file, flags | open_flags::truncate)
        .then([action = std::move(action)](seastar::file f) {
            return make_file_output_stream(f).then([action = std::move(action)](output_stream<char> &&out) {
                return do_with(std::move(out), [action = std::move(action)](output_stream<char> &out) {
                    return futurize_invoke(std::move(action), out)
                        .then([&out] {
                            return out.flush();
                        })
                        .finally([&out] {
                            return out.close();
                        });
                });
            });
        })
        .then([filepath, tmp_file] {
            return rename_file(tmp_file, filepath.native());
        });
}

template <typename Action>
future<>
with_truncate_file_output_stream(fs::path filepath, size_t size, open_flags flags, Action action) {
    using namespace std::chrono;
    auto ts = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();

    auto tmp_file = filepath.string() + "." + std::to_string(ts) + ".bak";

    return open_file_dma(tmp_file, flags | open_flags::truncate)
        .then([size, action = std::move(action)](seastar::file f) {
            return f.truncate(size).then([f, action = std::move(action)] {
                return make_file_output_stream(f).then([action = std::move(action)](output_stream<char> &&out) {
                    return do_with(std::move(out), [action = std::move(action)](output_stream<char> &out) {
                        return futurize_invoke(std::move(action), out)
                            .then([&out] {
                                return out.flush();
                            })
                            .finally([&out] {
                                return out.close();
                            });
                    });
                });
            });
        })
        .then([filepath, tmp_file] {
            return file_exists(filepath.native()).then([filepath] (auto exists) {
                if (exists) {
                    return remove_file(filepath.native());
                } else {
                    return make_ready_future<>();
                }
            }).then([tmp_file, filepath] {
                return rename_file(tmp_file, filepath.native());
            });
        });
}

template <typename Action>
future<>
with_file_input_stream(
    fs::path filepath,
    open_flags flags,
    uint64_t offset,
    uint64_t len,
    file_input_stream_options options,
    Action action) {
    return open_file_dma(filepath.native(), flags)
        .then([offset, len, options, action = std::move(action)](seastar::file f) {
            auto in = make_file_input_stream(std::move(f), offset, len, options);
            return do_with(std::move(in), [action = std::move(action)](input_stream<char> &in) {
                return futurize_invoke(std::move(action), in).finally([&in] {
                    return in.close();
                });
            });
        });
}

template <typename Action>
future<>
with_file_input_stream(
    fs::path filepath, open_flags flags, uint64_t offset, file_input_stream_options options, Action action) {
    return with_file_input_stream(
        filepath, flags, offset, std::numeric_limits<uint64_t>::max(), options, std::move(action));
}

template <typename Action>
future<>
with_file_input_stream(fs::path filepath, open_flags flags, file_input_stream_options options, Action action) {
    return with_file_input_stream(filepath, flags, 0, options, std::move(action));
}

template <typename Action>
future<>
with_file_input_stream(fs::path filepath, open_flags flags, Action action) {
    return with_file_input_stream(filepath, flags, 0, file_input_stream_options{}, std::move(action));
}


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
