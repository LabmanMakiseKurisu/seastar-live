/*
 * @Author: Amadeus
 * @Date: 2024-05-11 15:03:46
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-22 15:05:17
 * @FilePath: /Amadeus/src/server/http1/path.cc
 * @Description:
 */
#include "server/http1/path.hh"

#include <seastar/http/exception.hh>
#include <seastar/http/common.hh>
#include <seastar/http/httpd.hh>

namespace amadeus {
namespace http1 {
namespace path {
using namespace seastar::httpd;

const httpd::path_description play_stream_by_get(
    "/play",                    // 路径强匹配子串
    httpd::operation_type::GET, // HTTP 方法
    "play_stream_by_get",       // 路由昵称
    {
        // 路径参数
        {"app_name",    httpd::path_description::url_component_type::PARAM},
        {"stream_name", httpd::path_description::url_component_type::PARAM}
},
    {"format"} // 必需的查询参数
);

const path_description play_hls_stream("/hls",GET,"play_hls_stream",
{{"app_name", path_description::url_component_type::PARAM}
,{"stream_name", path_description::url_component_type::PARAM}
,{"filename", path_description::url_component_type::PARAM}},{});


} // namespace path
} // namespace http1
} // namespace amadeus