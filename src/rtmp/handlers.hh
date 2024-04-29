/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:51:20
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-29 15:33:50
 * @FilePath: /Amadeus/src/rtmp/handlers.hh
 * @Description: 
 */
#pragma once

#include <unordered_map>

#include "rtmp/reply.hh"
#include "rtmp/request.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

typedef const rtmp::request &const_req;

class handler_base {
 public:
    virtual future<std::unique_ptr<rtmp::reply>>
    handle(std::unique_ptr<rtmp::request> &req, std::unique_ptr<rtmp::reply> rep) = 0;

    virtual ~handler_base() = default;
};

} // namespace rtmp
} // namespace amadeus
