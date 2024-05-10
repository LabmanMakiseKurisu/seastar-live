#pragma once

#include <seastar/core/iostream.hh>
#include <seastar/http/request.hh>
#include <seastar/util/log.hh>

#include "frame/gop_queue.hh"
#include "session/session.hh"
#include "session/subscriber.hh"

namespace amadeus {
namespace session {

using namespace seastar;

using media_ptr = std::shared_ptr<flv::media_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using flv_frame_ptr = std::shared_ptr<flv::frame_t>;
using flv_frame_gop_queue_t = gop_queue_t<flv_frame_ptr>;

class publish_session : public session_impl {
 private:
    std::vector<flv_frame_ptr> _temporary_frames;
    flv_frame_gop_queue_t _gops_cache;
    std::vector<std::shared_ptr<subscriber_item>> _subscribers;
 public:
    publish_session(
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        protocol_t prot = protocol_t::RTMP);
    publish_session(
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        protocol_t prot = protocol_t::RTMP);
    virtual ~publish_session();

    virtual future<> start_with(input_stream<char> &in);

    virtual void add_subscriber(subscriber_ptr sub);
    virtual void remove_subscriber(subscriber_ptr sub);

    virtual float delay() const {
        return _gops_cache.duration();
    }

 protected:
    using session_impl::fail;

    future<> read_once(input_stream<char> &in);
    bool read_buffer(temporary_buffer<char> data);

    virtual void on_begin() override;
    virtual void on_done() override;
    virtual void on_cancel() override;
    virtual void on_fail() override;
    virtual void on_terminate() override;
    virtual void on_settings_update() override;
    virtual void on_frame(flv_frame_ptr frame) override;

    bool add_frame(flv_frame_ptr frame);

    void remove_all_subscribers();
    void for_each_subscriber(std::function<void(std::shared_ptr<publish_session>, subscriber_ptr)> func);

    future<> on_frame_for_each_subscriber(flv_frame_ptr frame);
    future<> on_frame(std::shared_ptr<subscriber_item> item, flv_frame_ptr frame);

    future<> publish_frame(flv_frame_ptr frame);

    mutable std::mutex _sub_lock;

 private:
    future<> publish();
    void add_temporary_frame(flv_frame_ptr frame);
};

namespace svr {

class publish_session : public session::publish_session {
 public:
    publish_session(
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        protocol_t prot = protocol_t::RTMP);
    publish_session(
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        protocol_t prot = protocol_t::RTMP);
    virtual ~publish_session() = default;

    virtual future<> start_with(input_stream<char> &in) override;
};

} // namespace svr
} // namespace session

using publisher_ptr = std::shared_ptr<session::publish_session>;

} // namespace amadeus
