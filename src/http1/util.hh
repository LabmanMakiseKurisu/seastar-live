/*
 * @Author: Amadeus
 * @Date: 2024-05-10 16:09:47
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 16:56:29
 * @FilePath: /Amadeus/src/http1/util.hh
 * @Description:
 */
#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/http/httpd.hh>
#include <seastar/http/request.hh>

#include "util/CxxUrl.hh"
#include "util/enums.hh"

namespace amadeus {

using arguments_t = std::unordered_map<sstring, sstring>;

namespace http1 {

using namespace seastar;

//解析被传入的req来填充以下各类字段
bool parse_remote_stream_request(
    const std::unique_ptr<http::request> &req,
    sstring &app,
    sstring &stream,
    sstring &address,
    sstring &internal_url,
    arguments_t &args,
    protocol_t &prot);

//用url填充args
void get_query(const Url &url, arguments_t &args);

//解析url的query，返回一个arguments_t
arguments_t get_query(const Url &url);
arguments_t parse_request_argumets(const std::unique_ptr<http::request> &req);

sstring get_localtion_url(const std::unique_ptr<http::request> &req);
std::vector<sstring> get_localtion_urls(const std::unique_ptr<http::request> &req);

sstring get_localtion_url(const std::unique_ptr<http::reply> &rep);
std::vector<sstring> get_localtion_urls(const std::unique_ptr<http::reply> &rep);

template <typename T>
sstring get_localtion_url(const T &headers);

template <typename T>
std::vector<sstring> get_localtion_urls(const T &headers);

} // namespace http1
} // namespace amadeus
