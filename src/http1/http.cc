/*
 * @Author: Amadeus
 * @Date: 2024-05-10 16:09:47
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 16:41:54
 * @FilePath: /Amadeus/src/http1/http.cc
 * @Description: 
 */
#include "http1/http.hh"
#include"http1/response.hh"
#include "http1/status.hh"

namespace amadeus {
namespace http1 {

using namespace seastar;

sstring generate_server_name() {
    static thread_local uint16_t idgen;
    return seastar::format("http-{}", idgen++);
}

} // namespace http1
} // namespace amadeus
