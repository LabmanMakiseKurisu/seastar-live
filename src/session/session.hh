#pragma once

#include <nlohmann/json.hpp>
#include <seastar/core/iostream.hh>
#include <seastar/core/smp.hh>
#include <seastar/core/sstring.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/timer.hh>
#include <seastar/http/request.hh>
#include <seastar/util/log.hh>

#include "app/global_setting.hh"
#include "frame/frame_queue.hh"
#include "flv/frame.hh"
#include "session/log.hh"
#include "util/enums.hh"
#include "util/retry_runner.hh"
#include "util/status.hh"

#define VALUE_TO_JSON(JSON, NAME)                    JSON["" #NAME] = NAME;
#define OBJECT_FUNC_TO_JSON(JSON, NAME, FUNC)        JSON["" #NAME] = FUNC;

#define OBJECT_VARIABLE_TO_JSON(JSON, OBJ_PTR, NAME) JSON["" #NAME] = OBJ_PTR->_##NAME;
#define OBJECT_PROPERTY_TO_JSON(JSON, OBJ_PTR, NAME) JSON["" #NAME] = OBJ_PTR->NAME;

namespace amadeus {

using arguments_t = std::unordered_map<sstring, sstring>;

namespace session {

using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using flv_frame_ptr = std::shared_ptr<flv::frame_t>;

class pipe {
 public:
    virtual ~pipe() = default;

    virtual void on_frame(flv_frame_ptr frame) = 0;

    virtual bool is_complete() const = 0;

    virtual future<> end() = 0;
    virtual future<> cancel() = 0;
    virtual future<> fail(status st) = 0;
};

struct collection {
    std::vector<int64_t> video_frame_rates;
    std::vector<int64_t> audio_frame_rates;

    std::vector<int64_t> video_bitrates;
    std::vector<int64_t> audio_bitrates;

    int64_t video_frame_rate = 0;
    int64_t audio_frame_rate = 0;
    int64_t video_bitrate = 0;
    int64_t audio_bitrate = 0;
    int64_t total_bytes = 0;
    int64_t total_io_bytes = 0;
    int64_t total_recv_io_bytes = 0;
    int64_t total_sent_io_bytes = 0;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point group_timestamp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point moni_timestamp = std::chrono::system_clock::now();
    
    collection() = default;
    virtual ~collection() = default;

    collection(collection &&x)
    : video_frame_rates(std::move(x.video_frame_rates))
    , audio_frame_rates(std::move(x.audio_frame_rates))
    , video_bitrates(std::move(x.video_bitrates))
    , audio_bitrates(std::move(x.audio_bitrates))
    , video_frame_rate(x.video_frame_rate)
    , audio_frame_rate(x.audio_frame_rate)
    , video_bitrate(x.video_bitrate)
    , audio_bitrate(x.audio_bitrate)
    , total_bytes(x.total_bytes)
    , total_io_bytes(x.total_io_bytes)
    , timestamp(x.timestamp)
    , group_timestamp(x.group_timestamp)
    , moni_timestamp(x.moni_timestamp) {
        x.video_frame_rate = 0;
        x.audio_frame_rate = 0;
        x.video_bitrate = 0;
        x.audio_bitrate = 0;
        x.total_bytes = 0;
        x.total_io_bytes = 0;
        x.timestamp = std::chrono::system_clock::now();
        x.group_timestamp = std::chrono::system_clock::now();
        x.moni_timestamp = std::chrono::system_clock::now();
    }

    collection &operator=(collection &&x) {
        video_frame_rates = std::move(x.video_frame_rates);
        audio_frame_rates = std::move(x.audio_frame_rates);
        video_bitrates = std::move(x.video_bitrates);
        audio_bitrates = std::move(x.audio_bitrates);
        video_frame_rate = x.video_frame_rate;
        audio_frame_rate = x.audio_frame_rate;
        video_bitrate = x.video_bitrate;
        audio_bitrate = x.audio_bitrate;
        total_bytes = x.total_bytes;
        total_io_bytes = x.total_io_bytes;
        timestamp = x.timestamp;
        group_timestamp = x.group_timestamp;
        moni_timestamp = x.moni_timestamp;

        x.video_frame_rate = 0;
        x.audio_frame_rate = 0;
        x.video_bitrate = 0;
        x.audio_bitrate = 0;
        x.total_bytes = 0;
        x.total_io_bytes = 0;
        x.timestamp = std::chrono::system_clock::now();
        x.group_timestamp = std::chrono::system_clock::now();
        x.moni_timestamp = std::chrono::system_clock::now();
        return *this;
    }
};

extern sstring make_session_name(const sstring &app, const sstring &stream);

class remote_session {
 public:
    remote_session(const sstring &app, const sstring &stream)
    : _remote_app(app)
    , _remote_stream(stream) {}

    virtual ~remote_session() = default;

    const sstring &remote_app() {
        return _remote_app;
    }

    const sstring &remote_stream() {
        return _remote_stream;
    }

 protected:
    sstring _remote_app;
    sstring _remote_stream;
};

class client_session {
 public:
    virtual ~client_session() = default;

    virtual void start() = 0;

    virtual future<> start_async() {
        start();

        return connected_future();
    }

    virtual future<> connected_future() {
        _connected = std::make_optional(promise<>());

        return _connected->get_future();
    }

 protected:
    void on_connect() {
        if (_connected != std::nullopt) _connected->set_value();
        _connected = std::nullopt;
    }

    template <typename E>
    void on_connect_failed(E e) {
        if (_connected != std::nullopt) _connected->set_exception(std::move(e));
        _connected = std::nullopt;
    }

    std::optional<promise<>> _connected = std::nullopt;
};

class session_impl;
using session_ptr = std::shared_ptr<session::session_impl>;

class lifecycle {
 public:
    virtual ~lifecycle() = default;
    virtual void on_fail(session_ptr s, status_t status, const sstring &message) = 0;
    virtual void on_cancel(session_ptr s) = 0;
    virtual void on_launch(session_ptr s) = 0;
    virtual void on_begin(session_ptr s) = 0;
    virtual void on_done(session_ptr s) = 0;
};

class session_impl : public pipe,
                     public std::enable_shared_from_this<session_impl> {
 protected:
    shard_id _cpu;
    sstring _id;
    sstring _index;
    sstring _sequence;
    sstring _app;
    sstring _stream;
    sstring _internal_url;
    std::unordered_map<sstring, sstring> _args;
    sstring _address;
    sstring _name;
    type_t _type = type_t::none;
    ownership_t _ownership = ownership_t::user;
    format_t _format = format_t::FLV;
    media_type_t _media_type = media_type_t::all;
    protocol_t _protocol = protocol_t::RTMP;
    std::chrono::system_clock::time_point _create_timestamp;
    int64_t _start_dts = -1;   // ms
    int64_t _current_dts = -1; // ms
    lifecycle *_lifecycle = nullptr;

    mutable std::mutex _meta_lock;
    metadata_ptr _meta;

    noncopyable_function<size_t()> _io_read_bytes_func;
    noncopyable_function<size_t()> _io_write_bytes_func;

    std::atomic_bool _done = false;
    std::atomic_bool _canceled = false;
    std::atomic_bool _failed = false;
    std::atomic_bool _timeout = false;
    std::atomic_bool _auto_complete = true;
    std::atomic_bool _zero_copy_supported = false;
    status _status;

    collection _current;
    std::deque<collection> _collections;
 public:
    session_impl(
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        ownership_t os = ownership_t::user,
        format_t fmt = format_t::FLV,
        media_type_t media_type = media_type_t::all,
        protocol_t prot = protocol_t::RTMP);
    session_impl(
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

    virtual ~session_impl();

    shard_id cpu() const;
    const sstring &id() const;
    const sstring &index() const;
    const sstring &sequence() const;
    const sstring &app() const;
    const sstring &stream() const;
    const sstring &name() const;
    const sstring &internal_url() const;
    const sstring &address() const;
    const arguments_t &args() const;
    ownership_t owner() const;
    type_t type() const;
    format_t format() const;
    protocol_t protocol() const;
    virtual media_type_t media_type() const;
    virtual sstring to_string() const;

    void set_lifecycle(lifecycle *lc);
    double duration() const;
    const metadata_ptr &meta() const;
    metadata_ptr copy_meta() const;

    void set_io_bytes_func(noncopyable_function<size_t()> read_func, noncopyable_function<size_t()> write_func);

    status current_status() const;
    void set_settings();
    bool is_done() const;
    bool is_canceled() const;
    bool is_failed() const;
    bool is_timeout() const;

    virtual bool is_complete() const override;
    virtual future<> end() override;
    virtual future<> cancel() override;
    virtual future<> fail(status st) override;


    template <typename Func, typename... Args>
    auto invoke(Func &&func, Args &&...args) {
        auto self = shared_from_this();
        return smp::submit_to(
                   _cpu,
                   [this, func = std::forward<Func>(func), args = std::tuple(std::move(args)...)]() mutable {
                       return std::apply(
                           [this, &func](Args &&...args) mutable {
                               return futurize_apply(
                                   func,
                                   std::tuple_cat(
                                       std::forward_as_tuple(*this), std::tuple(std::forward<Args>(args)...)));
                           },
                           std::move(args));
                   })
            .finally([self] {});
    }

    template <typename Func, typename... Args>
    auto apply(Func &&func, Args &&...args) {
        auto self = shared_from_this();
        return smp::submit_to(
                   _cpu,
                   [this, func = std::forward<Func>(func), args = std::tuple(std::move(args)...)]() mutable {
                       return std::apply(
                           std::forward<Func>(func), std::tuple_cat(std::forward_as_tuple(*this), std::move(args)));
                   })
            .finally([self] {});
    }

    template <typename Func>
    auto invoke_in(Func func) {
        auto self = shared_from_this();
        return smp::submit_to(
                   _cpu,
                   [this, func = std::move(func)] {
                       return func(this);
                   })
            .finally([self] {});
    }

 protected:
    virtual void _end();
    virtual void _cancel();
    virtual void _fail(status st);
    virtual void _set_status(status st);
    virtual void _set_status(status_t code);
    virtual void on_done();
    virtual void on_fail();
    virtual void on_cancel();
    virtual void on_timeout();
    virtual void on_codec(media_type_t update);
    virtual void on_settings_update();
    virtual void on_begin();
    virtual void on_end();
    virtual void on_launch();
    virtual void on_terminate();

    virtual void on_frame(flv_frame_ptr frame) override;
    void on_media(media_ptr frame);
    void on_meta(metadata_ptr frame);

    void print_status(status_t sc);
    void print_access_log();
    seastar::timer<> _access_log_timer;

    const global_settings &g_settings() const;

    bool auto_complete() const;

    template <typename C>
    bool validate_cache(C &cache) {
        auto max_bytes = g_settings().rtmp_max_gop_bytes();
        auto max_duration = g_settings().rtmp_max_gop_duration();

        auto bytes = cache.bytes();
        auto duration = cache.duration();

        auto out_of_bytes = bytes > max_bytes;
        if (out_of_bytes) l.warn("{} out of bytes: {} > {}", to_string(), bytes, max_bytes);

        auto out_of_duration = duration > max_duration;
        if (out_of_duration) l.warn("{} out of duration: {} > {}", to_string(), duration, max_duration);

        return !out_of_bytes && !out_of_duration;
    }
};

template <typename Container, typename Func>
static inline auto
invoke_in(Container &c, Func func) {
    return do_for_each(c, [func = std::move(func)](auto &s) mutable {
        return s->invoke_in(func);
    });
}

} // namespace session

using pipe_ptr = std::shared_ptr<session::pipe>;
using session_ptr = std::shared_ptr<session::session_impl>;
using remote_session_ptr = std::shared_ptr<session::remote_session>;
using client_session_ptr = std::shared_ptr<session::client_session>;

std::ostream &operator<<(std::ostream &os, session::session_impl *v);
std::ostream &operator<<(std::ostream &os, const session::session_impl &v);
std::ostream &operator<<(std::ostream &os, const session::session_impl *v);

} // namespace amadeus
