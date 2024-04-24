/*
 * @Author: Amadeus
 * @Date: 2024-04-23 10:42:38
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 14:10:10
 * @FilePath: /Amadeus/src/session/rtmp/subscriber.hh
 * @Description:
 */
#pragma once
#include "flv/frame.hh"

namespace amadeus {
namespace rtmp {
namespace session {
using namespace seastar;

class publish_session;
using rtmp_publisher_ptr = std::shared_ptr<publish_session>;
using media_ptr = std::shared_ptr<flv::media_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using frame_ptr = std::shared_ptr<flv::frame_t>;

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