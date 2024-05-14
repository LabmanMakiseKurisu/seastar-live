/*
 * @Author: Amadeus
 * @Date: 2024-04-22 18:15:16
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-13 15:46:51
 * @FilePath: /Amadeus/src/session/publish_session.cc
 * @Description:
 */
#include <seastar/core/thread.hh>
#include <seastar/core/with_timeout.hh>

#include "session/log.hh"
#include "session/play_session.hh"
#include "util/util.hh"

namespace amadeus {
namespace session {
publish_session::publish_session(
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    protocol_t prot)
: publish_session(util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, prot) {}

publish_session::publish_session(
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    protocol_t prot)
: session_impl(id, app, stream, internal_url, args, address, os, fmt, media_type_t::none, prot) {
    _type = type_t::publish;
    l.info("the publish_session instance is running on shard: {}", this_shard_id());
}

publish_session::~publish_session() {
    remove_all_subscribers();
}

future<>
publish_session::start_with(input_stream<char> &in) {
    assert(!in.eof());

    on_begin();

    l.info("{} will read", to_string());

    return repeat([&in, this] {
               if (is_complete() || in.eof()) return make_ready_future<stop_iteration>(stop_iteration::yes);

               return read_once(in).then([&in, this]() {
                   return make_ready_future<stop_iteration>(
                       (in.eof() || is_complete()) ? stop_iteration::yes : stop_iteration::no);
               });
           })
        .then_wrapped([this](auto &&f) {
            if (f.failed()) {
                try {
                    f.get();
                } catch (timed_out_error &e) { return make_exception_future<>(e); } catch (...) {
                    auto e = std::current_exception();
                    if (!is_failed()) {
                        status st = {status_t::recv_failed, fmt::format("{}", e)};
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
        .finally([&in, this] {
            if (_canceled) {
                l.info("{} is canceled", to_string());
            } else if (_timeout) {
                l.warn("{} is timeout", to_string());
            } else if (_failed) {
                l.warn("{} is failed for {}", to_string(), current_status());
            } else {
                if (auto_complete()) _end();
                l.info("{} read completely", to_string());
            }
            return make_ready_future<>();
        })
        .finally([&in, this] {
            return in.close();
        })
        .finally([&in, this] {
            on_end();
        });
}

void
publish_session::add_subscriber(subscriber_ptr sub) {
    std::lock_guard<std::mutex> g(_sub_lock);
    auto it = std::find_if(_subscribers.begin(), _subscribers.end(), [sub](auto &item) {
        return item->sub == sub;
    });
    if (it == _subscribers.end()) _subscribers.push_back(std::make_shared<subscriber_item>(sub, false));
}

void
publish_session::remove_subscriber(subscriber_ptr sub) {
    std::lock_guard<std::mutex> g(_sub_lock);

    auto it = std::find_if(_subscribers.begin(), _subscribers.end(), [sub](auto &item) {
        return item->sub == sub;
    });
    if (it != _subscribers.end()) _subscribers.erase(it);
}

future<>
publish_session::read_once(input_stream<char> &in) {
    auto tp = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(static_cast<int64_t>(g_settings().stream_timeout_interval() * 1000));
    return with_timeout(tp, in.read())
        .then([&in, this](temporary_buffer<char> buf) {
            if (!buf.empty()) {
                auto success = read_buffer(std::move(buf));
                if (!success) return make_exception_future<>(std::runtime_error("failed to decode buffer"));

                return publish();
            }
            return make_ready_future<>();
        })
        .handle_exception_type([&in, this](timed_out_error e) {
            on_timeout();
            return make_exception_future<>(std::move(e));
        });
}

bool
publish_session::read_buffer(temporary_buffer<char> data) {
    return false;
}

void
publish_session::on_begin() {
    session_impl::on_begin();
    _gops_cache.clear();
}

void
publish_session::on_done() {
    session_impl::on_done();

    for_each_subscriber([](std::shared_ptr<publish_session> self, subscriber_ptr sub) {
        sub->on_done(self);
    });
}

void
publish_session::on_cancel() {
    session_impl::on_cancel();

    for_each_subscriber([](std::shared_ptr<publish_session> self, subscriber_ptr sub) {
        sub->on_cancel(self);
    });
}

void
publish_session::on_fail() {
    session_impl::on_fail();

    auto st = current_status();
    for_each_subscriber([st](std::shared_ptr<publish_session> self, subscriber_ptr sub) {
        sub->on_fail(self, st);
    });
}

void
publish_session::on_terminate() {
    session_impl::on_terminate();

    remove_all_subscribers();
}

void
publish_session::on_settings_update() {
    session_impl::on_settings_update();
    _gops_cache.set_min_cache_duration(g_settings().rtmp_min_cache_duration());
}

void
publish_session::on_frame(frame_ptr frame) {
    session_impl::on_frame(frame);

    if (frame->is_media) {
        // if (g_settings().frame_trace_enabled()) l.trace("{} add {}", to_string(), frame->media);
    } else {
        _media_type = _gops_cache.media_options();

        l.trace("{} add {}", to_string(), frame->metadata);
    }
}

bool
publish_session::add_frame(frame_ptr frame) {
    bool added = _gops_cache.add_frame(frame);
    if (!added) return false;

    on_frame(frame);

    return true;
}

void
publish_session::remove_all_subscribers() {
    std::lock_guard<std::mutex> g(_sub_lock);

    _subscribers.clear();
}

void
publish_session::for_each_subscriber(std::function<void(std::shared_ptr<publish_session>, subscriber_ptr)> func) {
    _sub_lock.lock();
    std::vector<std::shared_ptr<subscriber_item>> items(_subscribers.begin(), _subscribers.end());
    _sub_lock.unlock();

    auto self = dynamic_pointer_cast<publish_session>(shared_from_this());
    for (auto &item : items) { func(self, item->sub); }
}

future<>
publish_session::on_frame_for_each_subscriber(frame_ptr frame) {
    _sub_lock.lock();
    std::vector<std::shared_ptr<subscriber_item>> items(_subscribers.begin(), _subscribers.end());
    _sub_lock.unlock();

    return do_with(std::move(items), [frame, this](auto &items) {
        return do_for_each(items, [frame, this](auto item) {
            return on_frame(item, frame);
        });
    });
}

future<>
publish_session::on_frame(std::shared_ptr<subscriber_item> item, frame_ptr frame) {
    auto self = dynamic_pointer_cast<publish_session>(shared_from_this());

    std::vector<frame_ptr> temp({frame});
    if (!item->receiving) temp = _gops_cache.all_frames();

    return do_with(std::move(temp), [item, self](auto &frames) {
        return smp::submit_to(
                   item->sub->cpu(self),
                   [item, &frames, self] {
                       return item->sub->on_frames(self, frames);
                   })
            .finally([item] {
                item->receiving = true;
            });
    });
}

future<>
publish_session::publish_frame(frame_ptr frame) {
    if (is_complete()) return make_ready_future<>();

    // on_track_frame(frame);
    if (!add_frame(frame)) return make_ready_future<>();

    if (validate_cache<flv_frame_gop_queue_t>(_gops_cache)) {
        return on_frame_for_each_subscriber(frame);
    } else {
        return cancel();
    }
}

future<>
publish_session::publish() {
    auto frames = std::move(_temporary_frames);
    if (frames.empty()) return make_ready_future<>();

    return do_with(std::move(frames), [this](auto &frames) {
        return do_for_each(frames, [this](auto &frame) {
            return publish_frame(frame);
        });
    });
}

void
publish_session::add_temporary_frame(frame_ptr frame) {
    _temporary_frames.push_back(frame);
}

namespace svr {

publish_session::publish_session(
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    protocol_t prot)
: publish_session(util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, prot) {}

publish_session::publish_session(
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    protocol_t prot)
: session::publish_session(id, app, stream, internal_url, args, address, os, fmt, prot) {}

future<>
publish_session::start_with(input_stream<char> &in) {
    on_launch();
    return session::publish_session::start_with(in).finally([&in, this] {
        on_terminate();
    });
}

} // namespace svr

} // namespace session
} // namespace amadeus