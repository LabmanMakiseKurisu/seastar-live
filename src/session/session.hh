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

extern sstring make_session_name(const sstring &app, const sstring &stream);

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
    shard_id _cpu; //this_shard_id
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

std::ostream &operator<<(std::ostream &os, session::session_impl *v);
std::ostream &operator<<(std::ostream &os, const session::session_impl &v);
std::ostream &operator<<(std::ostream &os, const session::session_impl *v);

} // namespace amadeus
