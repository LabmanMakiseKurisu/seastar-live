/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:42:38
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 10:43:41
 * @FilePath: /Amadeus/src/session/rtmp/subscriber.hh
 * @Description:
 */
#pragma once
#include "session/rtmp/frame.hh"

namespace amadeus {
namespace rtmp {
namespace session {
using namespace seastar;

class publish_session;
using rtmp_publisher_ptr = std::shared_ptr<publish_session>;

class subscriber {
 public:
    virtual ~subscriber() = default;

    virtual shard_id cpu(rtmp_publisher_ptr pub) const = 0;
    virtual future<> on_frames(rtmp_publisher_ptr pub, std::vector<frame_ptr> &frames) = 0;
};

using subscriber_ptr = std::shared_ptr<subscriber>;

struct subscriber_item {
    subscriber_ptr sub = nullptr;

    bool receiving = false;
};
} // namespace session
} // namespace rtmp
} // namespace amadeus