/*
 * @Author: Amadeus
 * @Date: 2024-04-22 15:22:44
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 14:07:31
 * @FilePath: /Amadeus/src/session/subscriber.hh
 * @Description:
 */
#pragma once

#include <seastar/core/sharded.hh>

#include <iostream>

#include "flv/frame.hh"
#include "util/status.hh"

namespace amadeus {

namespace session {

using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using flv_frame_ptr = std::shared_ptr<flv::frame_t>;
class publish_session;
using publisher_ptr = std::shared_ptr<publish_session>;

class subscriber {
 public:
    virtual ~subscriber() = default;
    virtual shard_id cpu(publisher_ptr pub) const = 0;
    virtual future<> on_frames(publisher_ptr pub, std::vector<flv_frame_ptr> &frames) = 0;

    virtual void on_done(publisher_ptr pub) = 0;
    virtual void on_cancel(publisher_ptr pub) = 0;
    virtual void on_fail(publisher_ptr pub, status st) = 0;
};

using subscriber_ptr = std::shared_ptr<subscriber>;

struct subscriber_item {
    subscriber_ptr sub = nullptr;

    bool receiving = false;
};
} // namespace session
} // namespace amadeus