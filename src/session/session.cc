/*
 * @Author: Amadeus
 * @Date: 2024-04-22 15:48:35
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-24 10:25:59
 * @FilePath: /Amadeus/src/session/session.cc
 * @Description:
 */

#include "session/session.hh"

#include <seastar/core/thread.hh>
#include <seastar/http/exception.hh>

#include <chrono>
#include <codecvt>

#include "util/util.hh"
#include "session.hh"

namespace amadeus {

namespace session {
using namespace seastar;
using namespace std::chrono_literals;

sstring
make_session_name(const sstring &app, const sstring &stream) {
    return app + "/" + stream;
}

session_impl::session_impl(
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    const sstring &address,
    ownership_t os,
    format_t fmt,
    media_type_t media_type,
    protocol_t prot)
: session_impl(util::generate_uuid(), app, stream, internal_url, args, address, os, fmt, media_type, prot) {}

session_impl::session_impl(
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
: _cpu(this_shard_id())
, _id(id)
, _index(util::generate_stream_index(app, stream))
, _sequence(util::generate_sequence())
, _app(app)
, _stream(stream)
, _internal_url(internal_url)
, _args(args)
, _address(util::make_valid_ip_address(prot, address))
, _name(make_session_name(app, stream))
, _ownership(os)
, _format(fmt)
, _media_type(media_type)
, _protocol(prot)
, _create_timestamp(std::chrono::system_clock::now()) {}

session_impl::~session_impl() {
    l.debug("{} destruct", to_string());
}

void
session_impl::set_lifecycle(lifecycle *lc) {
    _lifecycle = lc;
}

double
session_impl::duration() const {
    if (_start_dts == -1) return 0;
    if (_current_dts == -1) return 0;

    return _current_dts - _start_dts;
}

const metadata_ptr &
session_impl::meta() const {
    std::lock_guard<std::mutex> g(_meta_lock);

    return _meta;
}

metadata_ptr
session_impl::copy_meta() const {
    std::lock_guard<std::mutex> g(_meta_lock);
    metadata_ptr copy = std::make_shared<flv::metadata_t>(*_meta);
    return copy;
}

void
session_impl::set_io_bytes_func(noncopyable_function<size_t()> read_func, noncopyable_function<size_t()> write_func) {
    _io_read_bytes_func = std::move(read_func);
    _io_write_bytes_func = std::move(write_func);
}

status
session_impl::current_status() const {
    return _status;
}

void
session_impl::set_settings() {
    on_settings_update();
}

bool
session_impl::is_done() const {
    return _done;
}

bool
session_impl::is_canceled() const {
    return _canceled;
}

bool
session_impl::is_failed() const {
    return _failed;
}

bool
session_impl::is_timeout() const {
    return _timeout;
}

bool
session_impl::is_complete() const {
    return _done || _canceled || _failed || _timeout;
}

shard_id
session_impl::cpu() const {
    return _cpu;
}

const sstring &
session_impl::id() const {
    return _id;
}

const sstring &
session_impl::index() const {
    return _index;
}

const sstring &
session_impl::sequence() const {
    return _sequence;
}

const sstring &
session_impl::app() const {
    return _app;
}

const sstring &
session_impl::stream() const {
    return _stream;
}

const sstring &
session_impl::address() const {
    return _address;
}

const sstring &
session_impl::name() const {
    return _name;
}

const sstring &
session_impl::internal_url() const {
    return _internal_url;
}

const std::unordered_map<sstring, sstring> &
session_impl::args() const {
    return _args;
}

ownership_t
session_impl::owner() const {
    return _ownership;
}

type_t
session_impl::type() const {
    return _type;
}

format_t
session_impl::format() const {
    return _format;
}

protocol_t
session_impl::protocol() const {
    return _protocol;
}

media_type_t
session_impl::media_type() const {
    return _media_type;
}

sstring
session_impl::to_string() const {
    std::ostringstream os;

    os << _type;
    os << " " << _id;
    os << " " << _sequence;
    os << " " << _cpu;
    os << " " << _name;
    os << "-" << _ownership;
    os << "-" << media_type();
    os << "-" << _protocol;
    os << "-" << _format;
    os << "-" << _address;

    return os.str();
}

void
session_impl::_end() {
    if (is_complete()) return;

    _done = true;
    _status.code = status_t::ok;
    _status.content = "";

    on_done();
}

void
session_impl::_cancel() {
    if (is_complete()) return;

    _canceled = true;
    _status.code = status_t::cancel;
    _status.content = "";

    on_cancel();
}

void
session_impl::_fail(status st) {
    if (is_complete()) return;

    _failed = true;

    _set_status(std::move(st));

    on_fail();
}

void
session_impl::_set_status(status st) {
    _status = std::move(st);
}

void
session_impl::_set_status(status_t code) {
    _set_status(status(code));
}

void
session_impl::on_frame(flv_frame_ptr frame) {
    if (frame->is_metadata) {
        on_meta(frame->metadata);
    } else {
        on_media(frame->media);
    }
}

void
session_impl::on_media(media_ptr frame) {
    if (_start_dts == -1) _start_dts = frame->dts();
    _current_dts = frame->dts();
}

void
session_impl::on_meta(metadata_ptr frame) {
    std::lock_guard<std::mutex> g(_meta_lock);
    media_type_t update = media_type_t::none;
    if (!_meta || _meta->video_meta() != frame->video_meta()) update |= media_type_t::video;
    if (!_meta || _meta->audio_meta() != frame->audio_meta()) update |= media_type_t::audio;
    if (update != media_type_t::none) _meta = frame;
}

void
session_impl::on_done() {}

void
session_impl::on_fail() {}

void
session_impl::on_cancel() {}

void
session_impl::on_timeout() {
    _timeout = true;
    _set_status({status_t::timeout, "timed out"});
}

void
session_impl::on_codec(media_type_t update) {}

void
session_impl::on_settings_update() {}

void
session_impl::on_begin() {
    _done = false;
    _canceled = false;
    _failed = false;
    _timeout = false;

    _meta = metadata_ptr();
    if (_lifecycle) _lifecycle->on_begin(shared_from_this());
}

void
session_impl::on_end() {
    _access_log_timer.cancel();

    print_status(status_t::disconnect);
}

void
session_impl::on_launch() {
    _status = status();

    if (_lifecycle) _lifecycle->on_launch(shared_from_this());
}

void
session_impl::on_terminate() {
    if (!_lifecycle) return;

    if (_failed || _timeout) {
        auto st = current_status();
        _lifecycle->on_fail(shared_from_this(), st.code, st.content);
    } else if (_canceled) {
        _lifecycle->on_cancel(shared_from_this());
    } else if (_done) {
        _lifecycle->on_done(shared_from_this());
    } else {
        assert(0);
    }
    _lifecycle = nullptr;
}

/*
YY-MM-DD|HH:mm:ss|tsm|project_tag|shard_name|protocol|session_mode|format|app|stream_name|url|session_id|peer_address|total_io_bytes|create_timestamp|session_duration|status
*/
void
session_impl::print_status(status_t sc) {
    sstring trans_protocol = protocol_to_string(_protocol);
    sstring session_mode = session_type_to_string(_type, _ownership);
    sstring format = format_to_string(_format);
    int64_t create_timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(_create_timestamp.time_since_epoch()).count();
    int64_t session_duration =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        - create_timestamp;

    l.info(
        "{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
        trans_protocol,
        session_mode,
        format,
        _app,
        _stream,
        _internal_url,
        _id,
        _address,
        0,
        0,
        create_timestamp,
        session_duration,
        sc);
}

void
session_impl::print_access_log() {
    print_status(status_t::connect);
}

const global_settings &
session_impl::g_settings() const {
    return global_settings::global;
}

bool
session_impl::auto_complete() const {
    return _auto_complete;
}

future<>
session_impl::end() {
    _end();

    return make_ready_future<>();
}

future<>
session_impl::cancel() {
    _cancel();

    return make_ready_future<>();
}

future<>
session_impl::fail(status st) {
    _fail(st);

    return make_ready_future<>();
}

} // namespace session

std::ostream &
operator<<(std::ostream &os, session::session_impl *v) {
    return os << (const session::session_impl *)v;
}

std::ostream &
operator<<(std::ostream &os, const session::session_impl &v) {
    return os << &v;
}

std::ostream &
operator<<(std::ostream &os, const session::session_impl *v) {
    return os << v->to_string();
}

} // namespace amadeus