/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-07 15:49:40
 * @FilePath: /Amadeus/src/rtmp/request.hh
 * @Description:
 */
#pragma once

#include <seastar/core/iostream.hh>
#include <seastar/core/sstring.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/util/noncopyable_function.hh>
#include <seastar/util/string_utils.hh>

namespace amadeus {
namespace rtmp {

using namespace seastar;

class input_stream;
class output_stream;

/**
 * A request received from a client.
 */
struct request {
 public:
    // Bvc Request Information
    enum mode {
        publish = 0,
        play,
        live_only,
        vod_only,

        NUM_MODE
    };

    enum type {
        live = 0,
        record,
        append,

        ignored,

        NUM_TYPE
    };

 public:
    socket_address _remote_address; // 请求方地址
    mode _mode = mode::publish; //模式，默认为推流
    sstring app_name; //app名
    sstring stream_name; //流名
    type _type = type::ignored; // 类型
    double start = 0;           // 起始时间戳
    double duration = 0;        // 时长
    uint8_t reset = 0;          // for server play
    sstring tcurl; //服务器 URL
    std::unordered_map<sstring, sstring> args; //请求参数
    noncopyable_function<future<>(const request& req, input_stream&)> _body_reader;  //handler 从输入流读取到req
    noncopyable_function<future<>(const request& req, output_stream&)> _body_writer; //handler 写入请求体到输出流
    noncopyable_function<size_t()> _read_bytes_provider;  //handler 统计读入的字节数 
    noncopyable_function<size_t()> _write_bytes_provider; //handler 统计写入的字节数

 public:
    sstring stream();
    void read_body(noncopyable_function<future<>(const request &req, input_stream &)> &&body_reader);
    void write_body(noncopyable_function<future<>(const request &req, output_stream &)> &&body_writer);
    static request make(mode m, const sstring &app_name, const sstring &stream_name, const sstring &tcurl = "");

    friend std::ostream &operator<<(std::ostream &os, const request *v) {
        os << "mode: " << v->_mode;
        os << " app_name: " << v->app_name;
        os << " stream_name: " << v->stream_name;
        os << " _type: " << v->_type;
        os << " start: " << to_sstring(v->start);
        os << " duration: " << to_sstring(v->duration);
        os << " reset: " << static_cast<int>(v->reset);
        os << " tcurl: " << v->tcurl;
        os << " args: " << v->args;
        return os;
    }

    friend std::ostream &operator<<(std::ostream &os, const request &v) {
        return os << &v;
    }
};

static inline std::ostream &
operator<<(std::ostream &os, const std::vector<unsigned char> &v) {
    for (auto c : v) { os << fmt::format("{:02x}", c); }
    return os;
}

} // namespace rtmp

} // namespace amadeus
