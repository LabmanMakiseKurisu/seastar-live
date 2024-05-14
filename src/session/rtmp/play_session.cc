/*
 * @Author: Amadeus
 * @Date: 2024-04-23 14:00:04
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 14:36:01
 * @FilePath: /Amadeus/src/session/rtmp/play_session.cc
 * @Description:
 */
#include "session/rtmp/play_session.hh"

#include <amf0.h>
#include <flv-proto.h>
#include <seastar/core/thread.hh>

#include "rtmp/client.hh"
#include "rtmp/request.hh"
#include "rtmp/stream.hh"
#include "session/log.hh"
#include "session/rtmp/publish_session.hh"
#include "util/util.hh"

namespace amadeus {
namespace rtmp {
namespace session {
using namespace session_ns;

play_session::play_session(
    publisher_ptr pub,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, id, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: session_ns::play_session(pub, id, app, stream, internal_url, args, address, os, fmt, media_type, protocol_t::RTMP) {
    _zero_copy_supported = true;

    _rtmp_pub = std::dynamic_pointer_cast<publish_session>(pub);
}

//不断调用write_once(st, out)
future<>
play_session::start_with(output_stream &out) {
    on_begin();

    l.debug("{} will write", to_string());

    return do_with(
               pipe_state(),
               [&out, this](pipe_state &st) {
                   return repeat([&st, &out, this] {
                              if (is_complete()) return make_ready_future<stop_iteration>(stop_iteration::yes);

                              return write_once(st, out).then([this]() {
                                  return make_ready_future<stop_iteration>(
                                      is_complete() ? stop_iteration::yes : stop_iteration::no);
                              });
                          })
                       .then_wrapped([this](auto &&f) {
                           if (f.failed()) {
                               try {
                                   f.get();
                               } catch (timed_out_error &e) { return make_exception_future<>(e); } catch (...) {
                                   auto e = std::current_exception();
                                   if (!is_failed()) {
                                       status st = {status_t::send_failed, fmt::format("{}", e)};
                                       if (auto_complete()) {
                                           _fail(st);
                                       } else {
                                           _set_status(st);
                                       }
                                   }
                                   return make_exception_future<>(e);
                               }
                           }
                           return make_ready_future<>(f.get());
                       })
                       .finally([&out, this] {
                           if (_canceled) {
                               l.info("{} is canceled", to_string());
                           } else if (_timeout) {
                               l.warn("{} is timeout", to_string());
                           } else if (_failed) {
                               l.warn("{} is failed for {}", to_string(), current_status());
                           } else {
                               if (auto_complete()) _end();
                               l.info("{} write completely", to_string());
                           }
                           return make_ready_future<>();
                       })
                       .finally([&out, this]() {
                           return out.close();
                       });
               })
        .finally([this] {
            on_end();
        });
}

void
play_session::on_done() {
    _rtmp_queue.close();

    session_impl::on_done();
}

void
play_session::on_fail() {
    _rtmp_queue.close();

    session_impl::on_fail();
}

void
play_session::on_cancel() {
    _rtmp_queue.close();

    session_impl::on_cancel();
}

void
play_session::on_timeout() {
    _rtmp_queue.close();

    session_impl::on_timeout();
}

void
play_session::on_begin() {
    _rtmp_queue.clear();

    session_ns::play_session::on_begin();
}

void
play_session::on_terminate() {
    session_ns::play_session::on_terminate();

    _rtmp_pub = nullptr;
}

shard_id
play_session::cpu(publisher_ptr pub) const {

    if (_rtmp_pub != pub) return false;

    return session_ns::session_impl::cpu();
}

//对frames中的每个frame都调用on_frame(frame)
//当frames全部处理结束后，调用_rtmp_queue.notify_not_empty()
future<>
play_session::on_frames(publisher_ptr pub, std::vector<frame_ptr> &frames) {
    if (_rtmp_pub != pub || is_complete()) return make_ready_future();

    assert(_rtmp_pub && _rtmp_pub->format() == format_t::FLV);
    assert(_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP);

    return do_for_each(
               frames,
               [this](auto frame) {
                   on_frame(frame);

                   return make_ready_future();
               })
        .then([this] {
            if (!is_complete()) _rtmp_queue.notify_not_empty();
        });
}

//把_rtmp_queue中的frame都取出来，放入out中等待发送
future<>
play_session::write_once(pipe_state &st, output_stream &out) {
    return with_frames<frame_ptr>(_rtmp_queue, [&st, &out, this](auto frames) {
        return do_with(std::move(frames), [&st, &out, this](auto &frames) {
            return do_for_each(frames, [&st, &out, this](auto frame) {
                return write_frame(st, out, frame);
            });
        });
    });
}

//根据frame的类型调用write_metadata或write_media
future<>
play_session::write_frame(pipe_state &st, output_stream &out, frame_ptr frame) {
    if (frame->is_metadata) {
        st.metadata = frame->metadata;
        if (g_settings().frame_trace_enabled()) l.trace("{} write {}", to_string(), frame->metadata);
        return write_metadata(out, st.metadata);
    } else if (frame->is_script) {
        if (g_settings().frame_trace_enabled()) l.trace("{} write {}", to_string(), frame->script);
        return write(out, packet::make_script(frame->script->data.share(), 0));
    } else if (frame->is_media) {
        return write_media(out, st.metadata, frame->media);
    } else {
        return make_ready_future();
    }
}

//写script和紧接着的音视频配置tag到out中
future<>
play_session::write_metadata(output_stream &out, metadata_ptr metadata) {
    auto media_options = metadata->media_options();
    auto miss = _media_type & ~media_options;
    if (miss != media_type_t::none) l.warn("missing {} metadata", miss == media_type_t::audio ? "audio" : "video");

    auto duplicating = (media_options & ~_media_type) != media_type_t::none;
    if (duplicating) metadata = std::make_shared<flv::metadata_t>(*metadata, _media_type);

    auto f = make_ready_future();
    auto script = metadata->to_tag_data();
    if (script.size()) f = write(out, packet::make_script(std::move(script), 0));

    return f
        .then([metadata, &out, this] {
            return write_audio_metadata(out, metadata->audio);
        })
        .then([metadata, &out, this] {
            return write_video_metadata(out, metadata->video);
        });
}

//写音视频配置tag到out中
future<>
play_session::write_audio_metadata(output_stream &out, const flv::audio_meta_t &audio) {
    if (!audio.is_enabled()) return make_ready_future<>();

    auto ahb = audio.to_tag_data();
    if (ahb.empty() || !allow_media_type(media_type_t::audio)) return make_ready_future<>();

    return write(out, packet::make_audio(std::move(ahb), 0));
}

//写音视频配置tag到out中
future<>
play_session::write_video_metadata(output_stream &out, const flv::video_meta_t &video) {
    if (!video.is_enabled()) return make_ready_future<>();

    auto vhb = video.to_tag_data();
    if (vhb.empty() || !allow_media_type(media_type_t::video)) return make_ready_future<>();

    return write(out, packet::make_video(std::move(vhb), 0));
}

future<>
play_session::write_media(output_stream &out, metadata_ptr metadata, media_ptr media) {
    assert(metadata);
    if (!metadata) return make_ready_future();

    auto data = media->to_tag_data(metadata.get());
    if (data.empty()) {
        l.info("{} recv invalid frame {} with matadata {}", to_string(), media, metadata);

        return make_ready_future<>();
    }

    if (g_settings().frame_trace_enabled()) l.trace("{} write {}", to_string(), media);

    auto dts = media->dts();
    if (media->is_video()) {
        return write(out, packet::make_video(std::move(data), dts));
    } else {
        return write(out, packet::make_audio(std::move(data), dts));
    }
}

//写入frame到out中
future<>
play_session::write(output_stream &out, packet frame) {
    return out.write(std::move(frame)).then([&out] {
        return out.flush();
    });
}

void
play_session::subscribe() {
    if (_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP) {
        auto self = std::dynamic_pointer_cast<rtmp::session::subscriber>(shared_from_this());
        _delay = _rtmp_pub->delay();
        _rtmp_pub->add_rtmp_subscriber(self);
    } else {
        session_ns::play_session::subscribe();
    }
}

void
play_session::unsubscribe() {
    if (_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP) {
        _rtmp_pub->remove_rtmp_subscriber(std::dynamic_pointer_cast<rtmp::session::subscriber>(shared_from_this()));
    } else {
        session_ns::play_session::unsubscribe();
    }
}

//更新session信息并且向_rtmp_queue中添加
void
play_session::on_frame(frame_ptr frame) {
    if (is_complete()) return;

    if (frame->is_media && !allow_media_type(frame->media->media_type())) return;

    if (frame->is_metadata) {
        auto meta = frame->metadata;
        auto local = latest_metadata();

        media_type_t update = media_type_t::none;
        if (!local || local->video != meta->video) update |= media_type_t::video;
        if (!local || local->audio != meta->audio) update |= media_type_t::audio;
        if (update != media_type_t::none) {
            on_codec(update);
        }
    }

    if (frame->is_media) {
        if (_start_dts == -1) _start_dts = frame->media->dts();
        _current_dts = frame->media->dts();
    }

    _rtmp_queue.push_back(frame);

    if (!validate_cache<frame_queue_t<frame_ptr>>(_rtmp_queue)) {
        l.warn("{} too many cached frames", to_string());

        _cancel();
    }
}

metadata_ptr
play_session::latest_metadata() const {
    auto f = _rtmp_queue.latest_metadata_frame();
    return f ? f->metadata : nullptr;
}

namespace svr {

play_session::play_session(
    publisher_ptr pub,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: play_session(pub, id, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type)
: rtmp::session::play_session(pub, id, app, stream, internal_url, args, address, os, fmt, media_type) {}

future<>
play_session::start_with(output_stream &out) {
    on_launch();

    if (_pub->is_done()) {
        on_terminate();
        return make_ready_future<>();
    }
    return rtmp::session::play_session::start_with(out).finally([this] {
        on_terminate();
    });
}

} // namespace svr


} // namespace session
} // namespace rtmp
} // namespace amadeus