/*
 * @Author: Amadeus
 * @Date: 2024-05-11 15:01:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 11:55:30
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

    //http://ip:port/play/{app}/{stream}?format={flv,hls}
    extern const httpd::path_description play_stream_by_get;
    } // namespace path
} // namespace http1
} // namespace amadeus