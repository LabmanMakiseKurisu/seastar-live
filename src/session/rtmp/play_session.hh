/*
 * @Author: Amadeus
 * @Date: 2024-04-23 14:00:11
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 14:34:16
 * @FilePath: /Amadeus/src/session/rtmp/play_session.hh
 * @Description:
 */
#pragma once

#include <seastar/core/queue.hh>
#include <seastar/core/temporary_buffer.hh>

#include "flv/flv.hh"
#include "rtmp/stream.hh"
#include "session/play_session.hh"
#include "session/subscriber.hh"
#include "session/rtmp/publish_session.hh"


namespace amadeus {
namespace rtmp {
namespace session {
namespace session_ns = amadeus::session;
using publisher_ptr = session_ns::publisher_ptr;

using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;
using script_ptr = std::shared_ptr<flv::script_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using frame_ptr = std::shared_ptr<flv::frame_t>;
using rtmp_publisher_ptr = std::shared_ptr<rtmp::session::publish_session>;


using subscriber_ptr = std::shared_ptr<session_ns::subscriber>;

class play_session : public session_ns::play_session {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    virtual ~play_session() = default;

    virtual future<> start_with(output_stream &out);

 protected:
    virtual void on_done() override;
    virtual void on_fail() override;
    virtual void on_cancel() override;
    virtual void on_timeout() override;
    virtual void on_begin() override;
    virtual void on_terminate() override;


    virtual shard_id cpu(publisher_ptr pub) const override;
    virtual future<> on_frames(publisher_ptr pub, std::vector<frame_ptr> &frames) override;

    struct pipe_state {
        metadata_ptr metadata = nullptr;
    };

    future<> write_once(pipe_state &st, output_stream &out);
    future<> write_frame(pipe_state &st, output_stream &out, frame_ptr frame);
    future<> write_metadata(output_stream &out, metadata_ptr metadata);
    future<> write_audio_metadata(output_stream &out, const flv::audio_meta_t &audio);
    future<> write_video_metadata(output_stream &out, const flv::video_meta_t &video);
    future<> write_media(output_stream &out, metadata_ptr metadata, media_ptr media);

    future<> write(output_stream &out, packet frame);

    virtual void subscribe() override;
    virtual void unsubscribe() override;

    void on_frame(frame_ptr frame);

 protected:
    metadata_ptr latest_metadata() const;

    async_frame_queue_t<frame_ptr> _rtmp_queue;
    rtmp_publisher_ptr _rtmp_pub = nullptr;
};

namespace svr {

class play_session : public rtmp::session::play_session {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);
    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);

    play_session(
        session_ns::publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);
    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all);
    virtual ~play_session() = default;

    virtual future<> start_with(output_stream &out) override;
};

} // namespace svr

} // namespace session
} // namespace rtmp

using rtmp_player_ptr = std::shared_ptr<rtmp::session::play_session>;
} // namespace amadeus