#include "session/hls/play_session.hh"

#include <mov-format.h>
#include <seastar/core/thread.hh>
#include <seastar/core/when_all.hh>
#include <seastar/util/file.hh>
#include <seastar/util/log.hh>

#include "session/log.hh"
#include "session/publish_session.hh"
#include "session/rtmp/publish_session.hh"

namespace amadeus {
namespace hls {
namespace session {

using namespace session_ns;

play_session::play_session(
    publisher_ptr pub, const sstring &internal_url, const arguments_t &args, media_type_t media_type, version_t v)
: play_session(pub, pub->app(), pub->stream(), internal_url, args, media_type, v) {}

play_session::play_session(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    media_type_t media_type,
    hls::version_t v)
: session_ns::play_session(
    pub,
    app,
    stream,
    internal_url,
    args,
    "localhost",
    ownership_t::invisible,
    format_t::HLS,
    media_type,
    protocol_t::HTTP1)
, _version(v) {
    assert(v == version_t::v3 || v == version_t::v7);
}

version_t
play_session::version() const {
    return _version;
}

sstring
play_session::to_string() const {
    std::ostringstream os;

    os << session_ns::play_session::to_string();
    os << "-";
    os << (_version == version_t::v3 ? "v3" : "v7");

    return os.str();
}

void
play_session::subscribe() {
    auto _rtmp_pub = std::dynamic_pointer_cast<rtmp::session::publish_session>(_pub);
    if (_rtmp_pub && _rtmp_pub->protocol() == protocol_t::RTMP) {
        auto self = std::dynamic_pointer_cast<amadeus::session::subscriber>(shared_from_this());
        _delay = _rtmp_pub->delay();
        _rtmp_pub->add_rtmp_subscriber(self);
    } else {
        session_ns::play_session::subscribe();
    }
}

void
play_session::on_begin() {
    //session_ns::play_session::on_begin();
    session_impl::on_begin();

    _delay = 0;

    _flv_queue.clear();

    //if (_pub) _meta = _pub->copy_meta();

    subscribe();
    create_tracers();
}

void
play_session::on_end() {
    session_ns::play_session::on_end();

    _tracers.clear();
    _cleaners.clear();
}

void
play_session::on_settings_update() {
    session_ns::play_session::on_settings_update();

    create_tracers();
}

void
play_session::start() {
    (void)async([self = shared_from_this(), this] {
        on_launch();
        auto f = do_start()
                     .handle_exception([](auto e) {
                         l.warn("{}", e);
                     })
                     .finally([this] {
                         on_terminate();
                     })
                     .finally([self] {});
        f.get();
    });
}

future<>
play_session::do_start() {
    l.info("saving hls files to: {}/", _directory);

    on_begin();
    return init_directory(_directory)
        .then([this] {
            return do_write();
        })
        .then_wrapped([this](auto f) {
            if (f.failed()) {
                auto e = f.get_exception();
                if (!is_failed()) _fail({status_t::failed, fmt::format("{}", e)});
                return make_exception_future<>(std::move(e));
            } else {
                _end();
                return make_ready_future<>();
            }
        })
        .finally([this] {
            on_end();
        });
}

future<>
play_session::init_directory(const fs::path &directory) {
    return file_exists(directory.native())
        .then([directory](bool exists) {
            if (exists) return recursive_remove_directory(directory);
            return make_ready_future();
        })
        .then([directory] {
            return recursive_touch_directory(directory.native());
        });
}

future<>
play_session::do_write() {
    return repeat([this] {
               if (is_complete()) return make_ready_future<stop_iteration>(stop_iteration::yes);

                return write_once().then([this] {
                       return make_ready_future<stop_iteration>(
                           is_complete() ? stop_iteration::yes : stop_iteration::no);
                   });
           })
        .finally([this] {
            return for_each_tracer([](auto &tracer) {
                return tracer->closed();
            });
        })
        .finally([this] {
            if (_canceled) {
                l.info("is canceled");
            } else if (_timeout) {
                l.warn("hls session is timeout");
            } else if (_failed) {
                l.warn("is failed for {}", current_status());
            } else {
                _end();
                l.info("write completely");
            }
        });
}

future<>
play_session::write_once() {
    return with_frames<frame_ptr>(_flv_queue, [this](auto frames) {
        return do_with(
                   std::move(frames),
                   [this](auto &frames) {
                       return do_for_each(frames, [this](auto frame) {
                           return do_write_frame(frame);
                       });
                   })
            .handle_exception([this](auto f) {
                l.warn("ignored");
            });
    });
}

future<>
play_session::for_each_tracer(std::function<future<>(tracer_ptr &)> func) {
    auto tracers = all_tracers();
    return do_with(std::move(tracers), [func = std::move(func)](auto &tracers) {
        return do_for_each(tracers, [func = std::move(func)](tracer_ptr &tracer) {
            return func(tracer);
        });
    });
}

future<>
play_session::add_play_item(fragment_info_ptr item, float min_duration) {
    _playlist.push_back(item);
    return on_add_playitem(item)
        .then([item, min_duration, this] {
            auto pts = item->pts + item->duration;
            return do_until(
                [this, pts, min_duration] {
                    return !out_of_gop_duration(pts, min_duration);
                },
                [this] {
                    return remove_first_gop_play_items();
                });
        })
        .then([this] {
            return on_update_playlist(_playlist);
        });
}

bool
play_session::out_of_gop_duration(int64_t pts, float min_duration) {
    if (_playlist.size() <= 1) return false;
    if (_playlist.size() > static_cast<size_t>(g_settings().hls_ts_max_fragments())) return true;

    auto duration = min_duration * 1000;

    auto ski = find_second_key_frame_play_item();
    if (!ski) return false;

    auto second_pts = ski->pts;
    auto timeoffset = pts - second_pts;

    auto out = timeoffset > duration;

    return out;
}

fragment_info_ptr
play_session::find_second_key_frame_play_item() {
    if (_playlist.empty()) return nullptr;

    fragment_info_ptr first = nullptr;
    for (auto it = _playlist.begin(); it != _playlist.end(); it++) {
        if (!(*it)->start_by_keyframe) continue;
        if (first) return *it;

        first = *it;
    }
    return nullptr;
}

fragment_info_ptr
play_session::find_last_key_frame_play_item() {
    if (_playlist.empty()) return nullptr;

    for (auto it = _playlist.rbegin(); it != _playlist.rend(); it++) {
        if ((*it)->start_by_keyframe) return *it;
    }
    return nullptr;
}

future<>
play_session::remove_first_gop_play_items() {
    auto first = _playlist.front();

    std::vector<future<>> futs;
    for (auto it = _playlist.begin(); it != _playlist.end();) {
        auto item = *it;

        if (item->start_by_keyframe && item != first) break;

        it = _playlist.erase(it);

        futs.push_back(on_remove_playitem(item));
    }
    return do_with(std::move(futs), [](auto &futs) {
        return when_all(futs.begin(), futs.end()).discard_result();
    });
}

future<>
play_session::on_update_header(fragment_info_ptr header, metadata_ptr metadata) {
    return do_for_each_tracer([header, metadata](auto &tracer) {
        return tracer->on_update_header(header, metadata);
    });
}

future<>
play_session::on_add_playitem(fragment_info_ptr item) {
    return do_for_each_tracer([item](auto &tracer) {
        return tracer->on_add_playitem(item);
    });
}

future<>
play_session::on_remove_playitem(fragment_info_ptr item) {
    return do_for_each_tracer([item](auto &tracer) {
        return tracer->on_remove_playitem(item);
    });
}

future<>
play_session::on_update_playlist(const playlist_t &playlist) {
    return do_for_each_tracer([playlist](auto &tracer) {
        return tracer->on_update_playlist(playlist);
    });
}


} // namespace session
} // namespace hls
} // namespace bilibili
