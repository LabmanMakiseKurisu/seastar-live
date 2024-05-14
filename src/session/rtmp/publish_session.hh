#pragma once
#include "flv/flv.hh"
#include "rtmp/packet.hh"
#include "rtmp/stream.hh"
#include "session/publish_session.hh"
#include "session/subscriber.hh"

namespace amadeus {
namespace rtmp {
namespace session {

namespace session_ns = amadeus::session;

using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;
using script_ptr = std::shared_ptr<flv::script_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using frame_ptr = std::shared_ptr<flv::frame_t>;
using rtmp_frame_gop_queue_t = gop_queue_t<frame_ptr>;

using subscriber_ptr = std::shared_ptr<session_ns::subscriber>;

class publish_session : public session_ns::publish_session {
 public:
    publish_session(
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV);

    publish_session(
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV);
    virtual ~publish_session();

    virtual future<> start_with(input_stream &in);

    virtual void add_rtmp_subscriber(subscriber_ptr subscriber);
    virtual void remove_rtmp_subscriber(subscriber_ptr subscriber);

    virtual float delay() const override {
        return _rtmp_cache.duration();
    }

 protected:
    virtual void on_begin() override;
    virtual void on_terminate() override;
    virtual void on_settings_update() override;

    struct pipe_state {
        metadata_ptr metadata;

        int video_track_id = -1;
        int audio_track_id = -1;

        int64_t header_id = HEADER_ID_NULL;
        bool header_sent = false;
    };

    bool validate_packet(const packet &pkt);

    future<> read_once(pipe_state &st, input_stream &in);
    future<> on_recv_packet(pipe_state &st, packet pkt);
    future<> on_recv_script(pipe_state &st, packet pkt);
    future<> on_recv_video(pipe_state &st, packet pkt);
    future<> on_recv_audio(pipe_state &st, packet pkt);

    void remove_all_rtmp_subscribers();
    future<> on_frame_for_each_subscriber(frame_ptr frame);
    future<> on_frame(std::shared_ptr<session_ns::subscriber_item> item, frame_ptr frame);

    future<> publish(pipe_state &st, frame_ptr frame);

    // gop
    bool add_frame(pipe_state &st, frame_ptr frame);
    bool add_frame(frame_ptr frame);

    void on_frame(frame_ptr frame);
    void rectify_dts(frame_ptr frame);

 private:
    int64_t _last_actual_dts = -1; // ms
    int64_t _last_end_dts = -1;    // ms

    rtmp_frame_gop_queue_t _rtmp_cache; 

    std::vector<std::shared_ptr<session_ns::subscriber_item>> _subscribers; //带receiveing状态的subscriber数组
};

namespace svr {

class publish_session : public rtmp::session::publish_session {
 public:
    publish_session(
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV);
    publish_session(
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV);
    virtual ~publish_session() = default;

    virtual future<> start_with(input_stream &in) override;
};

} // namespace svr

} // namespace session
} // namespace rtmp

using rtmp_publisher_ptr = std::shared_ptr<rtmp::session::publish_session>;
} // namespace amadeus