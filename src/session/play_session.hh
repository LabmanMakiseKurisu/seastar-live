#pragma once

#include <seastar/core/iostream.hh>
#include <seastar/core/queue.hh>
#include <seastar/core/with_timeout.hh>
#include <seastar/http/request.hh>
#include <seastar/util/log.hh>

#include "session/publish_session.hh"

namespace amadeus {
namespace session {

using namespace seastar;

using media_ptr = std::shared_ptr<flv::media_t>;
using script_ptr = std::shared_ptr<flv::script_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;

using flv_frame_ptr = std::shared_ptr<flv::frame_t>;
using flv_frame_gop_queue_t = gop_queue_t<flv_frame_ptr>;

class play_session : public session_impl,
                     public subscriber {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

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
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);
    virtual ~play_session();

    virtual future<> start_with(output_stream<char> &out);

 protected:
    virtual shard_id cpu(publisher_ptr pub) const override;
    virtual future<> on_frames(publisher_ptr pub, std::vector<flv_frame_ptr> &frames) override;
    virtual void on_done(publisher_ptr pub) override;
    virtual void on_cancel(publisher_ptr pub) override;
    virtual void on_fail(publisher_ptr pub, status st) override;

 protected:
    struct pipe_state {
        bool bhin_sent = false;
        bool ftyp_sent = false;
    };
    virtual void on_done() override;
    virtual void on_fail() override;
    virtual void on_cancel() override;
    virtual void on_timeout() override;

    virtual void on_begin() override;
    virtual void on_end() override;
    virtual void on_terminate() override;

    virtual void on_frame(flv_frame_ptr frame) override;
    virtual void on_settings_update() override;

    virtual bool allow_media_type(media_type_t type) const;

    future<> write_once(pipe_state &st, output_stream<char> &out);

    virtual future<> write_frame(pipe_state &st, output_stream<char> &out, flv_frame_ptr frame);
    virtual future<> write_meta(pipe_state &st, output_stream<char> &out, metadata_ptr moov);
    virtual future<> write_media(pipe_state &st, output_stream<char> &out, media_ptr media);

    virtual future<> write_buffer(output_stream<char> &out, circular_buffer<temporary_buffer<uint8_t>> buffer);

    template <typename Frame>
    future<> with_frames(async_frame_queue_t<Frame> &queue, noncopyable_function<future<>(std::vector<Frame>)> func) {
        auto now = std::chrono::steady_clock::now();
        auto delay = std::chrono::milliseconds(static_cast<int64_t>(g_settings().stream_timeout_interval() * 1000));

        return with_timeout(now + delay, queue.not_empty())
            .then([func = std::move(func), &queue, this](auto frames) {
                if (frames.empty()) {
                    _end();

                    return make_ready_future<>();
                }
                return futurize_invoke(std::move(func), std::move(frames));
            })
            .handle_exception_type([this](timed_out_error &e) {
                on_timeout();
                return make_exception_future<>(e);
            });
    }

    virtual void subscribe();
    virtual void unsubscribe();

    float _delay = 0;

    publisher_ptr _pub = nullptr;

    async_frame_queue_t<flv_frame_ptr> _flv_queue;
};

namespace cln {

class play_session : public session::play_session,
                     public session::client_session,
                     public session::remote_session,
                     public util::delay_retry_runner {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &remote_app,
        const sstring &remote_stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &remote_app,
        const sstring &remote_stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    virtual ~play_session() = default;

    virtual void start() override;

    virtual future<> start_with(output_stream<char> &out) override;

 protected:
    virtual void _cancel() override;

    virtual void on_retry_finished() override;
    virtual void on_settings_update() override;
};

} // namespace cln

namespace svr {

class play_session : public session::play_session {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    play_session(
        publisher_ptr pub,
        const sstring &id,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

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
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);

    virtual ~play_session() = default;

    virtual future<> start_with(output_stream<char> &out) override;
};

} // namespace svr
} // namespace session

using player_ptr = std::shared_ptr<session::play_session>;

} // namespace amadeus