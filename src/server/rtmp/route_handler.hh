/*
 * @Author: Amadeus
 * @Date: 2024-04-23 14:49:09
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 15:13:49
 * @FilePath: /Amadeus/src/server/rtmp/route_handler.hh
 * @Description:
 */
#pragma once

#include "rtmp/handlers.hh"
#include "rtmp/request.hh"
#include "rtmp/rtmp.hh"
#include "server/transmition.hh"

namespace amadeus {
namespace rtmp {
namespace route {
    
using namespace seastar;

class route_handler : public handler_base {
 public:
    route_handler(transmition_ptr trans)
    : _trans(trans) {}

    virtual ~route_handler() = default;

 protected:
    transmition_ptr _trans;
};

class publish_stream_route_handler : public route_handler {
 public:
    publish_stream_route_handler(transmition_ptr trans)
    : route_handler(trans) {}

    virtual ~publish_stream_route_handler() = default;

    virtual future<std::unique_ptr<reply>> handle(std::unique_ptr<request> &req, std::unique_ptr<reply> rep) override;
};

class play_stream_route_handler : public route_handler {
 public:
    play_stream_route_handler(transmition_ptr trans)
    : route_handler(trans) {}

    virtual ~play_stream_route_handler() = default;

    virtual future<std::unique_ptr<reply>> handle(std::unique_ptr<request> &req, std::unique_ptr<reply> rep) override;
};
}
} // namespace rtmp
} // namespace amadeus