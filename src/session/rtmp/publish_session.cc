/*
 * @Author: Amadeus
 * @Date: 2024-04-23 13:29:35
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-09 18:02:36
 * @FilePath: /Amadeus/src/session/rtmp/publish_session.cc
 * @Description:
 */

#include "session/rtmp/publish_session.hh"

#include <amf0.h>
#include <flv-proto.h>
#include <seastar/core/thread.hh>
#include <seastar/core/with_timeout.hh>

#include "rtmp/client.hh"
#include "rtmp/request.hh"
#include "rtmp/stream.hh"
#include "session/log.hh"
#include "util/util.hh"

namespace amadeus {
namespace rtmp {
namespace session {

using namespace session_ns;

publish_session::publish_session(
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt)
: publish_session(util::generate_uuid(), app, stream, internal_url, args, address, os, fmt) {}

publish_session::publish_session(
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt)
: session_ns::publish_session(id, app, stream, internal_url, args, address, os, fmt, protocol_t::RTMP) {
    _zero_copy_supported = true;
}

publish_session::~publish_session() {
    remove_all_rtmp_subscribers();
}

//不断重复read_once(st, in)
future<>
publish_session::start_with(input_stream &in) {
    assert(!in.eof());

    on_begin();

    l.debug("{} will read", to_string());

    return do_with(
               pipe_state(),
               [&in, this](pipe_state &st) {
                   return repeat([&st, &in, this] {
                              if (in.eof() || is_complete())
                                  return make_ready_future<stop_iteration>(stop_iteration::yes);

                              return read_once(st, in).then([&in, this]() {
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
                           } else if (_failed) {
                               l.warn("{} is failed for {}", to_string(), current_status());
                           } else if (_timeout) {
                               l.warn("{} is timeout", to_string());
                           } else {
                               if (auto_complete()) _end();
                               l.info("{} read completely", to_string());
                           }
                           return in.close();
                       });
               })
        .finally([this] {
            on_end();
        });
}

void
publish_session::add_rtmp_subscriber(subscriber_ptr sub) {
    _sub_lock.lock();
    auto it = std::find_if(_subscribers.begin(), _subscribers.end(), [sub](auto &item) {
        return item->sub == sub;
    });
    if (it == _subscribers.end()) _subscribers.push_back(std::make_shared<subscriber_item>(sub, false));
    _sub_lock.unlock();
}

void
publish_session::remove_rtmp_subscriber(subscriber_ptr sub) {
    std::lock_guard<std::mutex> g(_sub_lock);

    auto it = std::find_if(_subscribers.begin(), _subscribers.end(), [sub](auto &item) {
        return item->sub == sub;
    });
    if (it != _subscribers.end()) _subscribers.erase(it);
}

void
publish_session::remove_all_rtmp_subscribers() {
    std::lock_guard<std::mutex> g(_sub_lock);

    _subscribers.clear();
}

void
publish_session::on_begin() {
    session_ns::publish_session::on_begin();

    _rtmp_cache.clear();

    _last_actual_dts = -1;
    _last_end_dts = -1;
}

void
publish_session::on_terminate() {
    session_ns::publish_session::on_terminate();

    remove_all_rtmp_subscribers();
}

void
publish_session::on_settings_update() {
    session_ns::publish_session::on_settings_update();

    using namespace std::chrono;
    // unix timestamp - (2022-01-01 00:00:00)
    int64_t first_fragment_id = duration_cast<seconds>(system_clock::now().time_since_epoch()).count() - 1640966400;
    _rtmp_cache.set_min_cache_duration(g_settings().rtmp_min_cache_duration());
}

//检查pkt是否过大
bool
publish_session::validate_packet(const packet &pkt) {
    if (pkt.size() <= g_settings().max_bytes_per_box()) return true;

    return false;
}

//从in立马读一个pkt，然后调用on_recv_packet(pkt)
future<>
publish_session::read_once(pipe_state &st, input_stream &in) {
    auto tp = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(static_cast<int64_t>(g_settings().stream_timeout_interval() * 1000));
    return with_timeout(tp, in.read())
        .then([&st, &in, this](packet pkt) {
            if (pkt.empty()) return make_ready_future<>();
            return on_recv_packet(st, std::move(pkt));
        })
        .handle_exception_type([this](timed_out_error e) {
            on_timeout();
            return make_exception_future<>(std::move(e));
        });
}

/*根据pkt的类型分别处理处理成script、video、audio的frame，
之后调用add_frame(frame)将其加入到自己的_rtmp_cache
最后调用publish(frame)给所有subscriber更新
*/
future<>
publish_session::on_recv_packet(pipe_state &st, packet pkt) {
    if (is_complete()) return make_ready_future<>();
    if (!validate_packet(pkt)) return make_exception_future<>(std::runtime_error("invalid packet"));

    switch (pkt.type) {
        case packet::script: return on_recv_script(st, std::move(pkt));
        case packet::video: return on_recv_video(st, std::move(pkt));
        case packet::audio: return on_recv_audio(st, std::move(pkt));
        default: return make_exception_future<>(std::runtime_error("unknown packet"));
    }
}

//将script tag的pkt解析后封装为frame，并调用add_frame(frame)和publish(frame)
future<>
publish_session::on_recv_script(pipe_state &st, packet pkt) {
    auto len = pkt.size();
    assert(len);

    auto data = pkt.data.get();

    flv::amf_command cmd = flv::amf_command::unknown;
    int n = flv::parse_amf_command(data, len, &cmd);
    if (n < 0) return make_exception_future<>(std::runtime_error("failed to parse script command"));

    auto remain = len - n;
    if (remain <= 0) { return make_ready_future<>(); }

    if (cmd == flv::amf_command::meta_data) {
        auto meta = st.metadata ? std::make_shared<flv::metadata_t>(*st.metadata) : std::make_shared<flv::metadata_t>();
        int rt = meta->load(data + n, remain);
        if (rt < 0) return make_exception_future<>(std::runtime_error("failed to parse metadata"));

        auto frame = std::make_shared<amadeus::flv::frame_t>(meta);
        if (add_frame(st, frame)) return publish(st, frame);
    } else if (cmd == flv::amf_command::rtmp_sample_access) {
        flv::rtmp_sample_access_t sa;
        int rt = sa.decode(data + n, remain);
        if (rt < 0) return make_exception_future<>(std::runtime_error("failed to parse sample_access"));
    } else {
        auto script = std::make_shared<flv::script_t>(pkt.data.share(n, remain));
        auto frame = std::make_shared<amadeus::flv::frame_t>(script);
        if (add_frame(st, frame)) return publish(st, frame);
    }
    return make_ready_future<>();
}

//将video tag的pkt封装为frame，并调用add_frame(frame)和publish(frame)
future<>
publish_session::on_recv_video(pipe_state &st, packet pkt) {
    auto len = pkt.size();
    assert(len);

    auto data = pkt.data.get();
    auto dts = pkt.dts;

    int n = 0;
    flv_video_tag_header_t header = {};
    try {
        n = ::flv_video_tag_header_read(&header, data, len);
        if (n < 0) {
            return make_exception_future<>(std::runtime_error(fmt::format("failed to read flv video header: {}", n)));
        }
    } catch (...) {
        return make_exception_future<>(
            std::runtime_error(fmt::format("failed to read flv video header: {}", std::current_exception())));
    }

    auto remain = len - n;
    if (remain <= 0) { return make_ready_future<>(); }

    if (header.avpacket == FLV_SEQUENCE_HEADER) {
        auto meta = st.metadata ? std::make_shared<flv::metadata_t>(*st.metadata) : std::make_shared<flv::metadata_t>();
        int rt = meta->video.load(header.codecid, data + n, remain);
        if (rt < 0) {
            return make_exception_future<>(std::runtime_error(fmt::format("failed to parse video header: {}", rt)));
        }
        auto frame = std::make_shared<amadeus::flv::frame_t>(meta);
        if (add_frame(st, frame)) return publish(st, frame);
    } else {
        if (!st.metadata) return make_ready_future<>();

        auto media = std::make_shared<flv::video_media_t>(header.keyframe, header.cts, dts, pkt.data.share(n, remain));
        if (!media) return make_ready_future<>();

        auto frame = std::make_shared<amadeus::flv::frame_t>(media);
        auto size = frame->media->data().size();

        if (add_frame(st, frame)) return publish(st, frame);
    }

    return make_ready_future<>();
}

//将audio tag的pkt封装为frame，并调用add_frame(frame)和publish(frame)
future<>
publish_session::on_recv_audio(pipe_state &st, packet pkt) {
    auto len = pkt.size();
    assert(len);

    auto data = pkt.data.get();
    auto dts = pkt.dts;

    int n = 0;
    flv_audio_tag_header_t header = {};
    try {
        n = ::flv_audio_tag_header_read(&header, data, len);
        if (n < 0) {
            return make_exception_future<>(std::runtime_error(fmt::format("failed to read flv audio header: {}", n)));
        }
    } catch (...) {
        return make_exception_future<>(
            std::runtime_error(fmt::format("failed to read flv audio header: {}", std::current_exception())));
    }

    auto remain = len - n;
    if (remain <= 0) { return make_ready_future<>(); }

    if (header.avpacket == FLV_SEQUENCE_HEADER) {
        auto meta = st.metadata ? std::make_shared<flv::metadata_t>(*st.metadata) : std::make_shared<flv::metadata_t>();
        int rt = meta->audio.load(header.codecid, data + n, remain);
        if (rt < 0) {
            return make_exception_future<>(std::runtime_error(fmt::format("failed to parse audio header: {}", rt)));
        }
        auto frame = std::make_shared<amadeus::flv::frame_t>(meta);
        if (add_frame(st, frame)) return publish(st, frame);
    } else {
        if (!st.metadata) return make_ready_future<>();

        auto media = std::make_shared<flv::audio_media_t>(dts, pkt.data.share(n, remain));
        if (!media) return make_ready_future<>();

        auto frame = std::make_shared<amadeus::flv::frame_t>(media);
        auto size = frame->media->data().size();
        if (add_frame(st, frame)) return publish(st, frame);
    }

    return make_ready_future<>();
}

//对_subscribers中的每个item，调用on_frame(item, frame)
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

//根据item的receiving状态，考虑是将_rtmp_cache的所有帧或者只是frame通过sub->on_frame()发送给item
future<>
publish_session::on_frame(std::shared_ptr<subscriber_item> item, frame_ptr frame) {
    auto self = dynamic_pointer_cast<publish_session>(shared_from_this());

    std::vector<frame_ptr> frames({frame});
    if (!item->receiving) frames = _rtmp_cache.all_frames();

    return do_with(std::move(frames), [item, self](auto &frames) {
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

//调用on_frame_for_each_subscriber给每个subscriber发送frame
future<>
publish_session::publish(pipe_state &st, frame_ptr frame) {
    if (validate_cache<rtmp_frame_gop_queue_t>(_rtmp_cache)) {
        return on_frame_for_each_subscriber(frame).then([&st, frame, this] {
            // return publish_bmt_frame(st, frame);
            return make_ready_future<>();
        });
    } else {
        return cancel();
    }
}

//这个函数主要是处理st，之后会调用add_frame(frame)
bool
publish_session::add_frame(pipe_state &st, frame_ptr frame) {
    if (frame->is_metadata) {
        auto meta = frame->metadata;
        if (st.metadata && *st.metadata == *meta) {
            l.debug("{} ignore {}", to_string(), meta);
            return false;
        }
        st.metadata = meta;
        st.header_sent = false;

        if (!frame->metadata->is_enabled()) return false;
    }

    if (frame->is_media) assert(frame->media->size());

    return add_frame(frame);
}

//修正frame的dts
void
publish_session::rectify_dts(frame_ptr frame) {
    if (!frame->is_media) return;

    // 0 1 2 3 4 5       0    1    2    3    4    5     0   1 2 3 4 5
    //           c       c=5  c=6  c=7  c=8  c=9  c=10
    //          le=5                              le=10

    auto dts = frame->media->dts();
    auto actual_dts = dts;
    auto backflow = dts < _last_actual_dts;
    if (backflow) _last_end_dts = (_current_dts + 1);
    if (backflow || _last_end_dts > 0) dts += _last_end_dts;

    _last_actual_dts = actual_dts;

    frame->media->set_dts(dts);
}

//向_rtmp_cache添加frame，并更新session
bool
publish_session::add_frame(frame_ptr frame) {
    rectify_dts(frame);

    bool added = _rtmp_cache.add_frame(frame);
    if (!added) return false;

    on_frame(frame);

    return true;
}

void
publish_session::on_frame(frame_ptr frame) {
    session_ns::session_impl::on_frame(frame);
    if (frame->is_media) {
        if (g_settings().frame_trace_enabled()) l.trace("{} add {}", to_string(), frame->media);
    } else {
        l.debug("{} add {}", to_string(), frame->metadata);
    }
}

namespace svr {

publish_session::publish_session(
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt)
: publish_session(util::generate_uuid(), app, stream, internal_url, args, address, os, fmt) {}

publish_session::publish_session(
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt)
: rtmp::session::publish_session(id, app, stream, internal_url, args, address, os, fmt) {}

future<>
publish_session::start_with(input_stream &in) {
    on_launch();
    return rtmp::session::publish_session::start_with(in).finally([this] {
        on_terminate();
    });
}

} // namespace svr


} // namespace session
} // namespace rtmp
} // namespace amadeus