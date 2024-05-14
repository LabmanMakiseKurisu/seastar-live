/*
 * @Author: Amadeus
 * @Date: 2024-05-10 16:09:47
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-11 10:39:58
 * @FilePath: /Amadeus/src/http1/http.hh
 * @Description:
 */
#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/http/httpd.hh>

namespace amadeus {
namespace http1 {

using namespace seastar;


sstring generate_server_name();

class server_control : public httpd::http_server_control {

 public:
    server_control() {}

    ~server_control() = default;

    future<> start(const sstring &name = amadeus::http1::generate_server_name()) {
        return httpd::http_server_control::start(name);
    }

    future<> stop() {
        return make_ready_future<>();
    }
};

} // namespace http1
} // namespace amadeus
