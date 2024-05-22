#include "server/transmition.hh"

#include <boost/algorithm/hex.hpp>
#include <seastar/core/do_with.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/when_any.hh>
#include <seastar/http/exception.hh>

#include <chrono>
#include <fstream>

#include "server/log.hh"
#include "util/CxxUrl.hh"
#include "util/util.hh"
#include "transmition.hh"
#include "session/hls/play_session_v3.hh"
#include "balancer/load_balancer.hh"

namespace amadeus {
namespace server {

using namespace seastar;
using namespace std::chrono_literals;

template <typename S>
using element_to = std::shared_ptr<S>;

template <typename T>
template <typename S>
bool
session_list<T>::remove_when(std::function<bool(element_to<S>)> condition) {
    std::lock_guard<std::mutex> g(_lock);
    bool rt = false;
    for (auto it = _sessions.begin(); it != _sessions.end();) {
        auto r = dynamic_pointer_cast<S>(*it);
        if (r && condition(r)) {
            it = _sessions.erase(it);
            rt = true;
        } else {
            ++it;
        }
    }
    return rt;
}

template <typename T>
template <typename S>
size_t
session_list<T>::size_when(std::function<bool(element_to<S>)> condition) const {
    std::lock_guard<std::mutex> g(_lock);
    size_t size = 0;
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r && condition(r)) { ++size; }
    }
    return size;
}

template <typename T>
template <typename S>
element_to<S>
session_list<T>::find_any_when(std::function<bool(element_to<S>)> condition) const {
    std::lock_guard<std::mutex> g(_lock);
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r && condition(r)) return r;
    }
    return nullptr;
}

template <typename T>
template <typename S>
std::vector<element_to<S>>
session_list<T>::find_all_when(std::function<bool(element_to<S>)> condition) const {
    std::lock_guard<std::mutex> g(_lock);
    std::vector<element_to<S>> sessions;
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r && condition(r)) sessions.push_back(r);
    }
    return sessions;
}

template <typename T>
template <typename S>
bool
session_list<T>::is_exist_when(std::function<bool(element_to<S>)> condition) const noexcept {
    std::lock_guard<std::mutex> g(_lock);
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r && condition(r)) return true;
    }
    return false;
}

template <typename T>
template <typename S>
void
session_list<T>::for_each(std::function<void(element_to<S>)> action) const noexcept {
    std::lock_guard<std::mutex> g(_lock);
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r) action(r);
    }
}

template <typename T>
template <typename S>
void
session_list<T>::for_each_when(
    std::function<bool(element_to<S>)> condition, std::function<void(element_to<S>)> action) const noexcept {
    std::lock_guard<std::mutex> g(_lock);
    for (auto s : _sessions) {
        auto r = dynamic_pointer_cast<S>(s);
        if (r && condition(r)) action(r);
    }
}

template <typename T>
static inline bool
default_session_comparation(
    std::shared_ptr<T> s,
    const sstring &app,
    const sstring &stream,
    ownership_t os = ownership_t::ignored,
    format_t fmt = format_t::ignored,
    protocol_t prot = protocol_t::none) {
    bool app_equal = app == "" || s->app() == app;
    bool stream_equal = stream == "" || s->stream() == stream;
    bool ownership_equal = ownership_t::ignored == os || s->owner() == os;
    bool format_equal = format_t::ignored == fmt || s->format() == fmt;
    bool protocol_contains = protocol_t::none == prot || (s->protocol() & prot) != protocol_t::none;
    return app_equal && stream_equal && ownership_equal && format_equal && protocol_contains;
}

#define default_session_condition                                                          \
    [&app, stream, os, fmt, prot, condition = std::move(condition)](auto s) {              \
        if (!condition) return default_session_comparation(s, app, stream, os, fmt, prot); \
        return default_session_comparation(s, app, stream, os, fmt, prot) && condition(s); \
    }

template <typename T>
template <typename S>
void
session_list<T>::for_each(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(element_to<S>)> condition,
    std::function<void(element_to<S>)> action) const {
    for_each_when<S>(default_session_condition, std::move(action));
}

template <typename T>
template <typename S>
size_t
session_list<T>::size(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(element_to<S>)> condition) const {
    if (app == "" && stream == "" && os == ownership_t::ignored && fmt == format_t::ignored && prot == protocol_t::none)
        return _sessions.size();

    return size_when<S>(default_session_condition);
}

template <typename T>
template <typename S>
element_to<S>
session_list<T>::find_any(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(element_to<S>)> condition) const {
    return find_any_when<S>(default_session_condition);
}

template <typename T>
template <typename S>
std::vector<element_to<S>>
session_list<T>::find_all(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(element_to<S>)> condition) const {
    return find_all_when<S>(default_session_condition);
}

template <typename T>
template <typename S>
void
session_list<T>::remove_all(const sstring &app, const sstring &stream) {
    remove_when<S>([app, stream](auto s) {
        return s->app() == app && s->stream() == stream;
    });
}

template <typename T>
template <typename S>
bool
session_list<T>::is_exist(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(element_to<S>)> condition) const {
    return is_exist_when<T>(default_session_condition);
}

template <typename T>
bool
session_list<T>::add(std::shared_ptr<T> session) {
    if (is_exist(session)) return false;

    std::lock_guard<std::mutex> g(_lock);
    _sessions.push_back(session);
    return true;
}

template <typename T>
void
session_list<T>::remove(std::shared_ptr<T> session) {
    std::lock_guard<std::mutex> g(_lock);

    auto it = std::find(_sessions.begin(), _sessions.end(), session);
    if (it != _sessions.end()) _sessions.erase(it);
}

template <typename T>
bool
session_list<T>::is_exist(std::shared_ptr<T> session) const {
    std::lock_guard<std::mutex> g(_lock);
    if (_sessions.empty()) return false;

    auto it = std::find(_sessions.begin(), _sessions.end(), session);
    return it != _sessions.end();
}

template <typename T>
const std::vector<std::shared_ptr<T>> &
session_list<T>::get() const {
    std::lock_guard<std::mutex> g(_lock);
    return _sessions;
}

const sstring transmition::version = "0.0.1";

transmition::transmition() {
    l.info("the trans instance is running on shard: {}", this_shard_id());
}

void
transmition::initialize(const global_settings &settings) {

}

future<publisher_ptr>
transmition::make_valid_publisher(
    protocol_t prot,
    format_t fmt,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address) {
    if (is_exist_publisher(app, stream)) return make_ready_future<publisher_ptr>(nullptr);

    auto pub = make_publisher(prot, fmt, app, stream, internal_url, args, address);
    return make_ready_future<publisher_ptr>(pub);
    // if (!_host_server) return make_ready_future<publisher_ptr>(pub);

    // return _host_server->validate(pub.get())
    //     .then([pub] {
    //         return make_ready_future<publisher_ptr>(pub);
    //     })
    //     .handle_exception([this](auto e) {
    //         l.warn("failed to publish: {}", e);
    //         return make_exception_future<publisher_ptr>(std::move(e));
    //     });
}

future<hls_player_ptr>
transmition::_schedule_hls_player(
    publisher_ptr pub, const sstring &internal_url, const arguments_t &args, media_type_t t, hls::version_t v) {
    return smp::submit_to(load_balancer::global.next_cpu(), [pub, internal_url, args, t, v, this] {
        hls_player_ptr plyr;

        if (v == hls::version_t::v3) {
            plyr = std::make_shared<hls::session::play_session_v3>(pub, internal_url, args, t);
        } 
        add_player(plyr);

        plyr->set_settings();
        plyr->start();

        return make_ready_future<hls_player_ptr>(plyr);
    });
}

future<>
transmition::validate(
    type_t type,
    protocol_t prot,
    format_t fmt,
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address) {
    return make_ready_future<>();
}

future<player_ptr>
transmition::make_valid_player(
    protocol_t prot,
    format_t fmt,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    media_type_t media_type) {
    auto id = util::generate_uuid();
    return validate(type_t::play, prot, fmt, id, app, stream, internal_url, args, address)
        .then_wrapped([prot, fmt, id, app, stream, internal_url, args, address, media_type, this](auto f) {
            if (f.failed()) {
                try {
                    f.get();
                    return make_ready_future<player_ptr>(nullptr);
                } catch (httpd::unexpected_status_error &e) {
                    if (e.status() == http::reply::status_type::moved_temporarily) {
                        auto pub = find_publisher(app, stream);
                        if (pub) {
                            auto plyr =
                                make_player(pub, prot, fmt, id, app, stream, internal_url, args, address, media_type);
                            return make_ready_future<player_ptr>(plyr);
                        } else {
                            l.warn("no valid pub is provided");
                            return make_ready_future<player_ptr>(nullptr);
                        }
                    } else {
                        l.warn("failed to play: {}", e.what());
                        return make_exception_future<player_ptr>(std::move(e));
                    }
                } catch (...) { return current_exception_as_future<player_ptr>(); }
            } else {
                auto pub = find_publisher(app, stream);
                if (pub) {
                    auto plyr = make_player(pub, prot, fmt, id, app, stream, internal_url, args, address, media_type);
                    return make_ready_future<player_ptr>(plyr);
                } else {
                    return make_ready_future<player_ptr>(nullptr);
                }
            }
        });
}

// publisher operator

publisher_ptr
transmition::find_publisher(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(publisher_ptr)> condition) const {
    return _publishers.find_any<session_ns::publish_session>(app, stream, os, fmt, prot, std::move(condition));
}

std::vector<publisher_ptr>
transmition::find_publishers(
    ownership_t os, format_t fmt, protocol_t prot, std::function<bool(publisher_ptr)> condition) const {
    return _publishers.find_all<session_ns::publish_session>("", "", os, fmt, prot, std::move(condition));
}

size_t
transmition::get_rtmp_publishers_size(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(rtmp_publisher_ptr)> condition) const {
    return _publishers.size<rtmp::session::publish_session>(app, stream, os, fmt, prot, std::move(condition));
}

rtmp_publisher_ptr
transmition::find_rtmp_publisher(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(rtmp_publisher_ptr)> condition) const {
    return _publishers.find_any<rtmp::session::publish_session>(app, stream, os, fmt, prot, std::move(condition));
}

std::vector<rtmp_publisher_ptr>
transmition::find_rtmp_publishers(
    ownership_t os, format_t fmt, protocol_t prot, std::function<bool(rtmp_publisher_ptr)> condition) const {
    return _publishers.find_all<rtmp::session::publish_session>("", "", os, fmt, prot, std::move(condition));
}

const std::vector<publisher_ptr> &
transmition::all_publishers() const {
    return _publishers.get();
}

bool
transmition::add_publisher(publisher_ptr pub) {
    assert(pub);

    if (_publishers.is_exist<session_ns::publish_session>(pub->app(), pub->stream())) return false;

    bool success = _publishers.add(pub);
    if (success) {
        // pub->set_settings(settings_for(pub->app(), pub->stream()));
        pub->set_settings();
        pub->set_lifecycle(this);

        l.info("add {}", pub->to_string());
        l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

        on_add_publisher(pub);
    }

    return success;
}

void
transmition::remove_publisher(session::publish_session *pub) {
    assert(pub);

    auto ptr = _publishers.find_any_when<session_ns::publish_session>([pub](auto s) {
        return s.get() == pub;
    });
    if (!ptr) return;

    _publishers.remove(ptr);

    l.info("remove {}", ptr->to_string());
    l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

    on_remove_publisher(ptr);
}

void
transmition::remove_publisher(publisher_ptr pub) {
    assert(pub);

    _publishers.remove(pub);

    l.info("remove {}", pub->to_string());
    l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

    on_remove_publisher(pub);
}

void
transmition::remove_publishers(const sstring &app, const sstring &stream) {
    assert(!app.empty() || !stream.empty());

    _publishers.remove_all<session_ns::publish_session>(app, stream);
}

bool
transmition::is_exist_publisher(const sstring &app, const sstring &stream) const {
    return _publishers.is_exist(app, stream);
}

bool
transmition::is_exist_publisher(publisher_ptr session) const {
    return _publishers.is_exist(session);
}

player_ptr
transmition::find_any_player(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(player_ptr)> condition) const {
    return _players.find_any<session_ns::play_session>(app, stream, os, fmt, prot, std::move(condition));
}

std::vector<player_ptr>
transmition::find_players(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(player_ptr)> condition) const {
    return _players.find_all<session_ns::play_session>(app, stream, os, fmt, prot, std::move(condition));
}

size_t
transmition::get_rtmp_players_size(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(rtmp_player_ptr)> condition) const {
    return _players.size<rtmp::session::play_session>(app, stream, os, fmt, prot, std::move(condition));
}

rtmp_player_ptr
transmition::find_any_rtmp_player(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(rtmp_player_ptr)> condition) const {
    return _players.find_any<rtmp::session::play_session>(app, stream, os, fmt, prot, std::move(condition));
}

std::vector<rtmp_player_ptr>
transmition::find_rtmp_players(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(rtmp_player_ptr)> condition) const {
    return _players.find_all<rtmp::session::play_session>(app, stream, os, fmt, prot, std::move(condition));
}

size_t
transmition::get_hls_players_size(
    const sstring &app,
    const sstring &stream,
    media_type_t media_type,
    hls::version_t version,
    std::function<bool(hls_player_ptr)> condition) const {
    return _players.size<hls::session::play_session>(
        app,
        stream,
        ownership_t::invisible,
        format_t::HLS,
        protocol_t::HTTP,
        [media_type, version, condition = std::move(condition)](auto plyr) {
            if ((plyr->media_type() & media_type) == media_type_t::none) return false;
            if ((plyr->version() & version) == hls::version_t::none) return false;
            if (condition) return condition(plyr);
            return true;
        });
}

hls_player_ptr
transmition::find_any_hls_player(
    const sstring &app,
    const sstring &stream,
    media_type_t media_type,
    hls::version_t version,
    std::function<bool(hls_player_ptr)> condition) const {
    return _players.find_any<hls::session::play_session>(
        app,
        stream,
        ownership_t::invisible,
        format_t::HLS,
        protocol_t::HTTP,
        [media_type, version, condition = std::move(condition)](auto plyr) {
            if ((plyr->media_type() & media_type) == media_type_t::none) return false;
            if ((plyr->version() & version) == hls::version_t::none) return false;
            if (condition) return condition(plyr);
            return true;
        });
}

std::vector<hls_player_ptr>
transmition::find_hls_players(
    const sstring &app,
    const sstring &stream,
    media_type_t media_type,
    hls::version_t version,
    std::function<bool(hls_player_ptr)> condition) const {
    return _players.find_all<hls::session::play_session>(
        app,
        stream,
        ownership_t::invisible,
        format_t::HLS,
        protocol_t::HTTP,
        [media_type, version, condition = std::move(condition)](auto plyr) {
            if ((plyr->media_type() & media_type) == media_type_t::none) return false;
            if ((plyr->version() & version) == hls::version_t::none) return false;
            if (condition) return condition(plyr);
            return true;
        });
}

future<std::vector<hls_player_ptr>>
transmition::schedule_hls_players_if_needs(
    publisher_ptr pub, const sstring &internal_url, const arguments_t &args, media_type_t t) {
    // auto settings = global_settings::global.to_settings();
    // settings += settings_for(pub->app(), pub->stream());

    auto v = hls::version_t::none;
    if (global_settings::global.hls_ts_enabled()) { v = v | hls::version_t::v3; }

    if (v == hls::version_t::none) return make_ready_future<std::vector<hls_player_ptr>>();

    return schedule_hls_players(pub, internal_url, args, t, v);
}

future<std::vector<hls_player_ptr>>
transmition::schedule_hls_players(
    publisher_ptr pub, const sstring &internal_url, const arguments_t &args, media_type_t t, hls::version_t v) {
    // settings_t settings;
    // settings += global_settings::global.to_settings();
    // settings += settings_for(pub->app(), pub->stream());
    // settings += additional_settings;

    return do_with(std::vector<hls_player_ptr>(), [pub, internal_url, args, t, v, this](auto &plyrs) {
        auto f = make_ready_future<>();
        if ((v & hls::version_t::v3) != hls::version_t::none) {
            f = f.then([&plyrs, pub, internal_url, args, t, v, this] {
                return _schedule_hls_player(pub, internal_url, args, t, hls::version_t::v3)
                    .then([&plyrs](auto plyr) {
                        plyrs.push_back(plyr);
                    });
            });
        }
        return f.then([&plyrs] {
            return make_ready_future<std::vector<hls_player_ptr>>(std::move(plyrs));
        });
    });
}

const std::vector<player_ptr> &
transmition::all_players() const {
    return _players.get();
}

bool
transmition::add_player(player_ptr plyr) {
    assert(plyr);

    bool success = _players.add(plyr);
    if (success) {
        // plyr->set_settings(settings_for(plyr->app(), plyr->stream()));
        plyr->set_settings();
        plyr->set_lifecycle(this);

        l.info("add {}", plyr->to_string());
        l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

        on_add_player(plyr);
    }
    return success;
}

void
transmition::remove_player(player_ptr plyr) {
    assert(plyr);

    _players.remove(plyr);

    l.info("remove {}", plyr->to_string());
    l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

    on_remove_player(plyr);
}

void
transmition::remove_player(session::play_session *plyr) {
    assert(plyr);

    auto ptr = _players.find_any_when<session_ns::play_session>([plyr](auto s) {
        return s.get() == plyr;
    });
    if (!ptr) return;

    _players.remove(ptr);

    l.info("remove {}", ptr->to_string());
    l.debug("publishers: {} players: {}", _publishers.size(), _players.size());

    on_remove_player(ptr);
}

void
transmition::remove_players(const sstring &app, const sstring &stream) {
    assert(!app.empty() || !stream.empty());

    auto plyrs = find_players(app, stream);
    if (plyrs.empty()) return;

    _players.remove_all(app, stream);

    for (auto plyr : plyrs) {
        l.info("remove {} ", plyr->to_string());
        on_remove_player(plyr);
    }

    l.debug("publishers: {} players: {}", _publishers.size(), _players.size());
}

bool
transmition::is_exist_player(player_ptr session) const {
    return _players.is_exist(session);
}

bool
transmition::is_exist_player(const sstring &app, const sstring &stream) const {
    return _players.is_exist(app, stream);
}

publisher_ptr
transmition::find_publisher(const sstring &id) const {
    return _publishers.find_any_when<session_ns::publish_session>([id](auto s) {
        return s->id() == id;
    });
}

player_ptr
transmition::find_player(const sstring &id) const {
    return _players.find_any_when<session_ns::play_session>([id](auto s) {
        return s->id() == id;
    });
}

std::vector<session_ptr>
transmition::all_sessions() const {
    auto const &plyrs = _players.get();
    auto const &pubs = _publishers.get();

    std::vector<session_ptr> ss;
    ss.insert(ss.end(), plyrs.begin(), plyrs.end());
    ss.insert(ss.end(), pubs.begin(), pubs.end());

    return ss;
}

std::vector<session_ptr>
transmition::find_sessions(
    const sstring &app,
    const sstring &stream,
    ownership_t os,
    format_t fmt,
    protocol_t prot,
    std::function<bool(session_ptr)> condition) const {
    std::vector<session_ptr> sessions =
        _publishers.find_all<session_ns::session_impl>(app, stream, os, fmt, prot, condition);
    std::vector<session_ptr> plyrs = _players.find_all<session_ns::session_impl>(app, stream, os, fmt, prot, condition);
    std::move(plyrs.begin(), plyrs.end(), std::back_inserter(sessions));

    return sessions;
}

session_ptr
transmition::find_session(const sstring &id) const {
    auto pub = find_publisher(id);
    if (pub != nullptr) return pub;

    auto plyr = find_player(id);
    if (plyr != nullptr) return plyr;

    return nullptr;
}

void
transmition::for_each_session(
    const sstring &app, const sstring &stream, std::function<void(session_ptr session)> func) const {
    _publishers.for_each<session_ns::session_impl>(
        app,
        stream,
        ownership_t::ignored,
        format_t::ignored,
        protocol_t::none,
        nullptr,
        [func = std::move(func), this](auto s) {
            func(s);
        });

    _players.for_each<session_ns::session_impl>(
        app,
        stream,
        ownership_t::ignored,
        format_t::ignored,
        protocol_t::none,
        nullptr,
        [func = std::move(func), this](auto s) {
            func(s);
        });
}

size_t
transmition::total_count() const {
    return _publishers.size() + _players.size();
}

publisher_ptr
transmition::make_publisher(
    protocol_t prot,
    format_t fmt,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address) {
    assert(app.size() && stream.size() && address.size());
    if (app.empty() || stream.empty() || address.empty()) return nullptr;

    if (fmt == format_t::ignored) fmt = protocol_to_default_format(prot);

    assert(fmt != format_t::UNKNOWN);
    if (fmt == format_t::UNKNOWN) return nullptr;

    publisher_ptr pub = nullptr;
    switch (prot) {
        // case protocol_t::TCP:
        //     pub = std::make_shared<tcp::session::svr::publish_session>(
        //         app, stream, internal_url, args, address, ownership_t::user, fmt);
        //     break;
        // case protocol_t::HTTP1:
        //     pub = std::make_shared<http1::session::svr::publish_session>(
        //         app, stream, internal_url, args, address, ownership_t::user, fmt);
        //     break;
        // case protocol_t::HTTP2:
        // case protocol_t::HTTP3:
        //     pub = std::make_shared<http1::session::svr::publish_session>(
        //         app, stream, internal_url, args, address, ownership_t::user, fmt);
        //     break;
        case protocol_t::RTMP:
            pub = std::make_shared<rtmp::session::svr::publish_session>(
                app, stream, internal_url, args, address, ownership_t::user, fmt);
            break;
        default: return nullptr;
    }

    // if (pub) pub->set_settings(settings_for(app, stream));
    pub->set_settings();
    return pub;
}

player_ptr
transmition::make_player(
    publisher_ptr pub,
    protocol_t prot,
    format_t fmt,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    media_type_t media_type) {
    assert(app.size() && stream.size() && address.size());
    if (app.empty() || stream.empty() || address.empty()) return nullptr;

    return make_player(pub, prot, fmt, util::generate_uuid(), app, stream, internal_url, args, address, media_type);
}

player_ptr
transmition::make_player(
    publisher_ptr pub,
    protocol_t prot,
    format_t fmt,
    const sstring &id,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    media_type_t media_type) {
    assert(id.size() && app.size() && stream.size() && address.size());
    if (id.empty() || app.empty() || stream.empty() || address.empty()) return nullptr;

    if (fmt == format_t::ignored) fmt = protocol_to_default_format(prot);

    assert(fmt != format_t::UNKNOWN);
    if (fmt == format_t::UNKNOWN) return nullptr;

    player_ptr plyr = nullptr;
    switch (prot) {
        // case protocol_t::TCP:
        //     plyr = std::make_shared<tcp::session::svr::play_session>(
        //         pub, id, app, stream, internal_url, args, address, ownership_t::user, fmt, media_type);
        //     break;
        case protocol_t::HTTP1:
            plyr = std::make_shared<http1::session::svr::play_session>(
                pub, id, app, stream, internal_url, args, address, ownership_t::user, fmt, media_type);
            break;
        // case protocol_t::HTTP2:
        // case protocol_t::HTTP3:
        //     plyr = std::make_shared<http1::session::svr::play_session>(
        //         pub, id, app, stream, internal_url, args, address, ownership_t::user, fmt, media_type);
        //     break;
        case protocol_t::RTMP:
            plyr = std::make_shared<rtmp::session::svr::play_session>(
                pub, id, app, stream, internal_url, args, address, ownership_t::user, fmt, media_type);
            break;
        default: return nullptr;
    }

    // if (plyr) plyr->set_settings(settings_for(app, stream));
    if (plyr) plyr->set_settings();
    return plyr;
}

void
transmition::on_add_publisher(publisher_ptr pub) {
    (void)seastar::async([pub, this] {
        auto f = schedule_hls_players_if_needs(pub, pub->internal_url(), pub->args(), media_type_t::all);
        f.get();
    });
}

void
transmition::on_remove_publisher(publisher_ptr pub) {
    stop_all_players_after_publisher_shutdown(pub);
}

void
transmition::on_add_player(player_ptr plyr) {}

void
transmition::on_remove_player(player_ptr plyr) {
    if (plyr->owner() != ownership_t::user) return;

    sstring mode = global_settings::global.auto_stop_publish_mode();
    if (mode == "none") return;

    stop_publisher_after_player_shutdown(plyr, mode);
}

void
transmition::stop_all_players_after_publisher_shutdown(publisher_ptr pub) {
    auto app = pub->app();
    auto stream = pub->stream();

    auto plyrs = find_players(app, stream);

    (void)seastar::async([plyrs = std::move(plyrs), this] {
        auto f = do_with(plyrs, [](auto &plyrs) {
            return do_for_each(plyrs, [](player_ptr plyr) {
                l.info("cancel {} for none publish session", plyr->to_string());
                return plyr->invoke<future<> (session_ns::session_impl::*)()>(&session_ns::session_impl::cancel);
            });
        });
        f.get();
    });
}

void
transmition::stop_publisher_after_player_shutdown(player_ptr plyr, const sstring &mode) {
    auto app = plyr->app();
    auto stream = plyr->stream();

    auto owner =
        mode == "user" ? ownership_t::user : (mode == "internal" ? ownership_t::internal : ownership_t::ignored);

    auto pub = find_publisher(app, stream, owner);
    if (!pub) return;

    (void)seastar::async([pub] {
        l.info("cancel {} for none play session", pub->to_string());
        auto f = pub->invoke<future<> (session_ns::session_impl::*)()>(&session_ns::session_impl::cancel);
        f.get();
    });
}

/*** session lifecycle ***/

void
transmition::on_session_end(session_ptr s) {
    if (s->type() == type_t::play) {
        remove_player(dynamic_pointer_cast<session_ns::play_session>(s));
    } else if (s->type() == type_t::publish) {
        remove_publisher(dynamic_pointer_cast<session_ns::publish_session>(s));
    } else {
        assert(0);
    }
}

void
transmition::on_fail(session_ptr s, status_t status, const sstring &message) {
    on_session_end(s);
}

void
transmition::on_cancel(session_ptr s) {
    on_session_end(s);
}

void
transmition::on_launch(session_ptr s) {}

void
transmition::on_begin(session_ptr s) {}

void
transmition::on_done(session_ptr s) {
    on_session_end(s);
}
} // namespace server
} // namespace amadeus