/*
 * @Author: Amadeus
 * @Date: 2024-05-11 15:01:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-22 15:05:38
 * @FilePath: /Amadeus/src/server/http1/path.hh
 * @Description:
 */

#pragma once
#include <seastar/http/api_docs.hh>
#include <seastar/http/json_path.hh>
namespace amadeus {
namespace http1 {
namespace path {
    using namespace seastar;

    extern const httpd::path_description play_stream_by_get;
    extern const httpd::path_description play_hls_stream;
    } // namespace path
} // namespace http1
} // namespace amadeus