/*
 * @Author: Amadeus
 * @Date: 2024-04-22 19:09:03
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 12:01:28
 * @FilePath: /Amadeus/src/session/play_session.cc
 * @Description:
 *
 */
#include "session/play_session.hh"

#include <seastar/core/loop.hh>
#include <seastar/core/thread.hh>

#include "session/log.hh"
#include "util/util.hh"

namespace amadeus {
namespace session {
using namespace seastar;

play_session::play_session(
    publisher_ptr pub,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type, prot) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, id, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type, prot) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type, prot) {}

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
    media_type_t media_type,
    protocol_t prot)
: session_impl(id, app, stream, internal_url, args, address, os, fmt, media_type, prot)
, _pub(pub) {
    assert(pub);

    _type = type_t::play;
    l.info("the play_session instance is running on shard: {}", this_shard_id());
}

play_session::~play_session() {}

future<>
play_session::start_with(output_stream<char> &out) {
    on_begin();

    l.info("{} will write", to_string());

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
                       .finally([&out, this]() {
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
                       .finally([&out, this] {
                           return out.close();
                       });
               })
        .finally([this] {
            on_end();
        });
}

shard_id
play_session::cpu(publisher_ptr pub) const {
    if (_pub != pub) return false;

    return session_impl::cpu();
}

future<>
play_session::on_frames(publisher_ptr pub, std::vector<flv_frame_ptr> &frames) {
    if (_pub != pub) return make_ready_future<>();

    return do_for_each(
               frames,
               [this](auto frame) {
                   on_frame(frame);

                   return make_ready_future();
               })
        .then([this] {
            if (!is_complete()) _flv_queue.notify_not_empty();
        });
}

void
play_session::on_done(publisher_ptr pub) {
    if (_pub != pub) return;

    if (_auto_complete) _end();
}

void
play_session::on_cancel(publisher_ptr pub) {
    if (_pub != pub) return;

    if (_auto_complete) _cancel();
}

void
play_session::on_fail(publisher_ptr pub, status st) {
    if (_pub != pub) return;

    if (_auto_complete) _fail(st);
}

void
play_session::on_done() {
    _flv_queue.close();

    session_impl::on_done();
}

void
play_session::on_fail() {
    _flv_queue.close();

    session_impl::on_fail();
}

void
play_session::on_cancel() {
    _flv_queue.close();

    session_impl::on_cancel();
}

void
play_session::on_timeout() {
    _flv_queue.close();

    session_impl::on_timeout();
}

void
play_session::on_begin() {
    session_impl::on_begin();

    _delay = 0;

    _flv_queue.clear();

    if (_pub) _meta = _pub->copy_meta();

    subscribe();
}

void
play_session::on_end() {
    session_impl::on_end();

    unsubscribe();
}

void
play_session::on_terminate() {
    session_impl::on_terminate();

    _pub = nullptr;
}

void
play_session::on_frame(flv_frame_ptr frame) {
    if (!_pub || is_complete()) return;
    if (frame->is_media && !allow_media_type(frame->media->media_type())) return;

    _flv_queue.push_back(frame);

    if (!validate_cache<frame_queue_t<flv_frame_ptr>>(_flv_queue)) {
        l.warn("{} too many cached frames", to_string());
        _cancel();
    }

    session_impl::on_frame(frame);
}

void
play_session::on_settings_update() {
    session_impl::on_settings_update();
}

bool
play_session::allow_media_type(media_type_t type) const {
    return (_media_type & type) != media_type_t::none;
}

future<>
play_session::write_once(pipe_state &st, output_stream<char> &out) {
    auto f = make_ready_future<>();

    return f.then([&st, &out, this] {
        return with_frames<flv_frame_ptr>(_flv_queue, [&st, &out, this](auto frames) {
            assert(frames.front()->is_metadata);
            return do_with(std::move(frames), [&st, &out, this](auto &frames) {
                return do_for_each(frames, [&st, &out, this](auto frame) {
                    return write_frame(st, out, frame);
                });
            });
        });
    });
}

future<>
play_session::write_frame(pipe_state &st, output_stream<char> &out, flv_frame_ptr frame) {
    if (frame->is_metadata) {
        return write_meta(st, out, frame->metadata);
    } else {
        return write_media(st, out, frame->media);
    }
}

future<>
play_session::write_meta(pipe_state &st, output_stream<char> &out, metadata_ptr moov) {
    return make_ready_future<>();
}

future<>
play_session::write_media(pipe_state &st, output_stream<char> &out, media_ptr media) {
    return make_ready_future<>();
}

future<>
play_session::write_buffer(output_stream<char> &out, circular_buffer<temporary_buffer<uint8_t>> buffers) {
    return do_with(std::move(buffers), [&out, this](circular_buffer<temporary_buffer<uint8_t>> &buffers) {
        return do_for_each(buffers, [&out, this](temporary_buffer<uint8_t> &buf) {
            auto f = make_ready_future();
            if (_zero_copy_supported) {
                f = out.write(
                    temporary_buffer<char>(reinterpret_cast<char *>(buf.get_write()), buf.size(), buf.release()));
            } else {
                f = out.write(reinterpret_cast<const char *>(buf.get()), buf.size());
            }
            return f.then([&out] {
                return out.flush();
            });
        });
    });
}

void
play_session::subscribe() {
    if (!_pub) return;

    auto self = std::dynamic_pointer_cast<subscriber>(shared_from_this());
    _delay = _pub->delay();
    _pub->add_subscriber(self);
}

void
play_session::unsubscribe() {
    if (_pub) _pub->remove_subscriber(std::dynamic_pointer_cast<subscriber>(shared_from_this()));
}

namespace svr {

play_session::play_session(
    publisher_ptr pub,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type, prot) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type, prot) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &id,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: play_session(pub, id, pub->app(), pub->stream(), internal_url, args, address, os, fmt, media_type, prot) {}

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
    media_type_t media_type,
    protocol_t prot)
: session::play_session(pub, id, app, stream, internal_url, args, address, os, fmt, media_type, prot) {}

future<>
play_session::start_with(output_stream<char> &out) {
    if (_pub->is_done()) {
        on_terminate();

        auto e = exception(status_t::no_content, fmt::format("{} complete for {}", _pub, _pub->current_status()));
        return make_exception_future<>(std::move(e));
    }

    on_launch();
    return session::play_session::start_with(out).finally([&out, this] {
        on_terminate();
    });
}

} // namespace svr

} // namespace session
} // namespace amadeus