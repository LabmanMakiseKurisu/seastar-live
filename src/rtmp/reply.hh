/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-29 15:29:44
 * @FilePath: /Amadeus/src/rtmp/reply.hh
 * @Description:
 */
#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/sstring.hh>
#include <seastar/util/noncopyable_function.hh>

namespace amadeus {
namespace rtmp {

using namespace seastar;

class input_stream;
class output_stream;

/**
 * A reply to be sent to a client.
 */
struct reply {
 public:
    enum status_type {
        ok = 0,
        not_found,
        internal_error
    };

 public:
    status_type _status;
    noncopyable_function<future<>(const reply& rep, input_stream&)> _body_reader;  // for client
    noncopyable_function<future<>(const reply& rep, output_stream&)> _body_writer; // for server
    noncopyable_function<size_t()> _read_bytes_provider;
    noncopyable_function<size_t()> _write_bytes_provider;

 public:
    reply()
    : _status(status_type::ok) {}

    reply(const reply& rep) = default;
    reply(reply&& rep) = default;
    reply& operator=(const reply& rep) = default;
    reply& operator=(reply&& rep) = default;

    reply& set_status(status_type status) {
        _status = status;
        return *this;
    }

    reply& done() {
        return *this;
    }

    void write_body(noncopyable_function<future<>(const reply& rep, output_stream&)>&& body_writer);
    void read_body(noncopyable_function<future<>(const reply& rep, input_stream&)>&& body_reader);

    friend std::ostream& operator<<(std::ostream& os, const reply* v) {
        os << v->_status;
        return os;
    }

    friend std::ostream& operator<<(std::ostream& os, const reply& v) {
        return os << &v;
    }
};

std::ostream& operator<<(std::ostream& os, reply::status_type st);

} // namespace rtmp

} // namespace amadeus
