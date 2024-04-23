/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 10:52:59
 * @FilePath: /Amadeus/src/rtmp/reply.hh
 * @Description:
 */
#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/sstring.hh>
#include <seastar/util/noncopyable_function.hh>

#include "rtmp/packet.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

class input_stream;
class output_stream;

/**
 * A reply to be sent to a client.
 */
struct reply {
    /**
     * The status of the reply.
     */
    enum status_type {
        ok = 0,
        not_found,
        internal_error
    } _status;

    noncopyable_function<future<>(const reply& rep, input_stream&)> _body_reader;  // for client
    noncopyable_function<future<>(const reply& rep, output_stream&)> _body_writer; // for server

    noncopyable_function<size_t()> _read_bytes_provider;
    noncopyable_function<size_t()> _write_bytes_provider;

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

    /**
     * Done should be called before using the reply.
     * It would set the response line
     */
    reply& done() {
        return *this;
    }

    /*!
     * \brief use an output stream to write the message body
     *
     * When a handler needs to use an output stream it should call this method
     * with a function.
     *
     *  you would have used for such a content, (i.e. "txt", "html", "json", etc')
     * \param body_writer - a function that accept an output stream and use that stream to write the body.
     *   The function should take ownership of the stream while using it and must close the stream when it
     *   is done.
     *
     * Message would use chunked transfer encoding in the reply.
     *
     */

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
