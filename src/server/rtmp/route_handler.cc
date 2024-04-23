/*
 * @Author: Amadeus
 * @Date: 2024-04-23 14:49:19
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 15:14:31
 * @FilePath: /Amadeus/src/server/rtmp/route_handler.cc
 * @Description:
 */
#include "server/rtmp/route_handler.hh"

#include <seastar/http/exception.hh>

#include "rtmp/log.hh"
#include "rtmp/status.hh"
#include "rtmp/stream.hh"
#include "session/rtmp/play_session.hh"
#include "session/rtmp/publish_session.hh"
#include "util/CxxUrl.hh"
#include "util/util.hh"

namespace amadeus {
namespace rtmp {
namespace route {
future<std::unique_ptr<reply>>
finally(std::unique_ptr<request> &req, std::unique_ptr<reply> rep, reply::status_type status) {
    l.info(
        "request end {}/{} {} {} {} {} {}",
        req->app_name,
        req->stream(),
        req->_remote_address,
        req->_mode,
        req->_type,
        req->tcurl,
        status);

    rep->set_status(status).done();
    return make_ready_future<std::unique_ptr<reply>>(std::move(rep));
}

future<std::unique_ptr<reply>>
publish_stream_route_handler::handle(std::unique_ptr<request> &req, std::unique_ptr<reply> rep) {
    l.info(
        "request begin {}/{} {} {} {} {}",
        req->app_name,
        req->stream(),
        req->_remote_address,
        req->_mode,
        req->_type,
        req->tcurl);

    return do_with(std::move(rep), [&req, this](std::unique_ptr<reply> &rep) {
        auto app = req->app_name;
        auto stream = req->stream_name;
        if (app.empty() || stream.empty()) return finally(req, std::move(rep), reply::status_type::not_found);

        if (_trans->is_exist_publisher(app, stream)) {
            return finally(req, std::move(rep), reply::status_type::internal_error);
        }

        return _trans
            ->make_valid_publisher(
                protocol_t::RTMP, format_t::FLV, app, stream, req->tcurl, req->args, to_sstring(req->_remote_address))
            .then_wrapped([&req, &rep, this](auto f) {
                if (f.failed()) {
                    try {
                        f.get();
                    } catch (httpd::base_exception &e) {
                        return finally(req, std::move(rep), reply::status_type::internal_error);
                    } catch (...) { return finally(req, std::move(rep), reply::status_type::internal_error); }
                    return finally(req, std::move(rep), reply::status_type::internal_error);
                } else {
                    auto pub = dynamic_pointer_cast<rtmp::session::svr::publish_session>(f.get());
                    if (pub) {
                        _trans->add_publisher(pub);

                        req->read_body([pub](const request &req, input_stream &in) {
                            pub->set_io_bytes_func(
                                [&req]() {
                                    return req._read_bytes_provider();
                                },
                                [&req]() {
                                    return req._write_bytes_provider();
                                });
                            return pub->start_with(in).finally([pub] {});
                        });
                        return make_ready_future<std::unique_ptr<reply>>(std::move(rep));
                    } else {
                        return finally(req, std::move(rep), reply::status_type::not_found);
                    }
                }
            });
    });
}

future<std::unique_ptr<reply>>
play_stream_route_handler::handle(std::unique_ptr<request> &req, std::unique_ptr<reply> rep) {
    l.info(
        "request begin {}/{} {} {} {} {}",
        req->app_name,
        req->stream(),
        req->_remote_address,
        req->_mode,
        req->_type,
        req->tcurl);

    return do_with(std::move(rep), [&req, this](std::unique_ptr<reply> &rep) {
        auto app = req->app_name;
        auto stream = req->stream_name;
        if (app.empty() || stream.empty()) return finally(req, std::move(rep), reply::status_type::not_found);

        return _trans
            ->make_valid_player(
                protocol_t::RTMP, format_t::FLV, app, stream, req->tcurl, req->args, to_sstring(req->_remote_address))
            .then_wrapped([&req, &rep, this](auto f) {
                if (f.failed()) {
                    try {
                        f.get();
                    } catch (httpd::base_exception &e) {
                        return finally(req, std::move(rep), reply::status_type::internal_error);
                    } catch (...) { return finally(req, std::move(rep), reply::status_type::internal_error); }
                    return finally(req, std::move(rep), reply::status_type::internal_error);
                } else {
                    auto plyr = dynamic_pointer_cast<rtmp::session::svr::play_session>(f.get());
                    if (plyr) {
                        _trans->add_player(plyr);

                        rep->write_body([plyr](const reply &rep, output_stream &out) {
                            plyr->set_io_bytes_func(
                                [&rep]() {
                                    return rep._read_bytes_provider();
                                },
                                [&rep]() {
                                    return rep._write_bytes_provider();
                                });
                            return plyr->start_with(out).finally([plyr] {});
                        });
                        return make_ready_future<std::unique_ptr<reply>>(std::move(rep));
                    } else {
                        return finally(req, std::move(rep), reply::status_type::not_found);
                    }
                }
            });
    });
}

} // namespace route
} // namespace rtmp
} // namespace amadeus