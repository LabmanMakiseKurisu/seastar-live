#include "server/http1/route_handler.hh"

#include <boost/algorithm/string/replace.hpp>
#include <nlohmann/json.hpp>
#include <seastar/core/fstream.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/thread.hh>
#include <seastar/http/exception.hh>
#include <seastar/http/httpd.hh>
#include <seastar/util/short_streams.hh>
#include <seastar/util/tmp_file.hh>

#include "http1/http.hh"
#include "http1/log.hh"
#include "http1/status.hh"
#include "http1/util.hh"
#include "session/http1/play_session.hh"
#include "util/CxxUrl.hh"
#include "util/util.hh"

namespace amadeus {
namespace http1 {
namespace route {

static inline future<std::unique_ptr<http::reply>>
finally(
    const sstring &method,
    const sstring &url,
    std::unique_ptr<http::reply> rep,
    http::reply::status_type status,
    const sstring &content = "") {
    l.info("http request end {}: {} {} {}", method, url, status, status == http::reply::status_type::ok ? "" : content);

    rep->set_status(status);

    rep->_headers["Access-Control-Allow-Origin"] = "*";
    rep->_headers["Access-Control-Allow-Methods"] = "*";
    rep->_headers["Access-Control-Allow-Headers"] = "*";
    return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
}

static inline future<std::unique_ptr<http::reply>>
finally(
    std::unique_ptr<http::request> req,
    std::unique_ptr<http::reply> rep,
    http::reply::status_type status,
    const sstring &content = "") {
    return finally(req->_method, req->_url, std::move(rep), status, content);
}

static inline future<std::unique_ptr<http::reply>>
finally(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep, const sstring &content) {
    auto st = rep->_status;
    rep->_content = content;

    return finally(std::move(req), std::move(rep), st, content);
}

static inline future<std::unique_ptr<http::reply>>
finally(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep, response rsp = resp::success) {
    rep->write_body(rsp.content_type, rsp.content);

    return finally(std::move(req), std::move(rep), rsp.status, rsp.content);
}

static inline future<std::unique_ptr<http::reply>>
finally(const sstring &method, const sstring &url, std::unique_ptr<http::reply> rep, response rsp = resp::success) {
    rep->write_body(rsp.content_type, rsp.content);

    return finally(method, url, std::move(rep), rsp.status, rsp.content);
}

template <typename T, std::enable_if_t<std::is_base_of<session_ns::session_impl, T>::value, int> = 0>
static inline future<std::unique_ptr<http::reply>>
finally_session_info(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep, std::shared_ptr<T> session) {
    return do_with(std::move(req), std::move(rep), [session](auto &req, auto &rep) {
        return session
            ->invoke_in([](auto s) {
                return s->information()->to_json();
            })
            .then([&req, &rep](auto js) {
                return finally(std::move(req), std::move(rep), resp::make_json_string(js));
            });
    });
}

static inline future<std::unique_ptr<http::reply>>
no_session_id_provided(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    l.warn("no session id provided");

    return finally(
        std::move(req), std::move(rep), resp::make(http::reply::status_type::not_acceptable, "no session id provided"));
}

static inline future<std::unique_ptr<http::reply>>
no_app_name_provided(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    l.warn("no stream app provided");

    return finally(
        std::move(req), std::move(rep), resp::make(http::reply::status_type::not_acceptable, "no stream app provided"));
}

static inline future<std::unique_ptr<http::reply>>
no_stream_name_provided(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    l.warn("no stream name provided");

    return finally(
        std::move(req),
        std::move(rep),
        resp::make(http::reply::status_type::not_acceptable, "no session name provided"));
}

static inline future<std::unique_ptr<http::reply>>
no_file_name_provided(std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    l.warn("no file name provided");

    return finally(
        std::move(req), std::move(rep), resp::make(http::reply::status_type::not_acceptable, "no file name provided"));
}

static inline future<std::unique_ptr<http::reply>>
unsupported_content_type(
    std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep, const sstring &mime_type) {
    l.warn("unsupported content-type: {}", mime_type);

    return finally(std::move(req), std::move(rep), resp::make(http::reply::status_type::unsupported_media_type));
}

static inline future<std::unique_ptr<http::reply>>
read_file(sstring filepath, sstring mime_type, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    struct stat fst;
    if (stat(filepath.c_str(), &fst) != 0) {
        return finally(std::move(req), std::move(rep), resp::make_not_found("file not found"));
    }

    uint64_t lmt = 0;
#if (defined __USE_MISC || defined __USE_XOPEN2K8)
    lmt = fst.st_mtim.tv_sec * 1000LL + fst.st_mtim.tv_nsec / 1000000;
#else
    lmt = fst.st_mtime * 1000LL;
#endif

    rep->add_header("Last_Modified_Timestamp", std::to_string(lmt));
    rep->add_header("Content-Length", to_sstring(fst.st_size));

    rep->write_body(sstring(), [filepath](output_stream<char> &&out)-> future<> {
        return do_with(std::move(out), [filepath](auto &out) {
            return with_file_input_stream(
                       fs::path(filepath),
                       open_flags::ro,
                       [&out](auto &in) {
                           return seastar::copy(in, out);
                       })
                .then([&out] {
                    return out.flush();
                })
                .finally([&out] {
                    return out.close();
                });
        });
    });
    rep->set_mime_type(mime_type);
    return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
}

static future<std::unique_ptr<http::reply>>
read_file(std::filesystem::path filepath, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    auto extension = filepath.extension().string();
    if (extension.size() <= 1)
        return finally(std::move(req), std::move(rep), resp::make_not_found("no extension is provided"));

    auto mime_type = extension_to_mime_type(extension);
    if (mime_type.empty()) {
        return finally(
            std::move(req),
            std::move(rep),
            resp::make_not_found(fmt::format("file extension: {} is unsupported", extension)));
    }
    return read_file(filepath.native(), mime_type, std::move(req), std::move(rep));
}

static inline future<std::unique_ptr<http::reply>>
write_file(seastar::file f, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    return do_with(
        std::move(req),
        std::move(rep),
        [f = std::move(f)](std::unique_ptr<http::request> &req, std::unique_ptr<http::reply> &rep) {
            auto &request = *req;

            return make_file_output_stream(std::move(f))
                .then([&request](output_stream<char> &&out) {
                    return do_with(std::move(out), [&request](output_stream<char> &out) {
                        return seastar::copy(*request.content_stream, out).then([&out] {
                            return out.close();
                        });
                    });
                })
                .then([&rep] {
                    rep->done();
                    return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
                })
                .finally([req = std::move(req)] {});
        });
}

static inline future<std::unique_ptr<http::reply>>
write_file(sstring filepath, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    return do_with(
        std::move(req),
        std::move(rep),
        [filepath](std::unique_ptr<http::request> &req, std::unique_ptr<http::reply> &rep) {
            return open_file_dma(filepath, open_flags::rw | open_flags::create).then([&req, &rep](seastar::file f) {
                return write_file(std::move(f), std::move(req), std::move(rep));
            });
        });
}

static inline future<nlohmann::json>
parse_json(const sstring &content) {
    try {
        auto json = nlohmann::json::parse(content);
        return make_ready_future<nlohmann::json>(std::move(json));
    } catch (...) { return make_exception_future<nlohmann::json>(std::current_exception()); }
}

static inline future<nlohmann::json>
parse_json(std::unique_ptr<http::request> &req) {
    if (!req->content.empty()) return parse_json(req->content);

    return seastar::util::read_entire_stream_contiguous(*req->content_stream)
        .then([req = std::move(req)](sstring content) mutable {
            return parse_json(content);
        });
}

future<std::unique_ptr<http::reply>>
default_route_handler::handle(
    const sstring &path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    auto status = req->_method == "OPTIONS" ? http::reply::status_type::ok : http::reply::status_type::not_found;
    return finally(std::move(req), std::move(rep), status);
}

future<std::unique_ptr<http::reply>>
route_handler::play_stream(
    std::unique_ptr<http::request> req,
    std::unique_ptr<http::reply> rep,
    const sstring &app,
    const sstring &stream,
    format_t fmt,
    media_type_t media) {
    return do_with(
        std::move(req),
        std::move(rep),
        [app, stream, fmt, media, this](std::unique_ptr<http::request> &req, std::unique_ptr<http::reply> &rep) {
            auto args = parse_request_argumets(req);
            auto internal_url = req->get_header("");
            return _trans
                ->make_valid_player(
                    protocol_t::HTTP1, fmt, app, stream, internal_url, args, to_sstring(req->_client_address), media)
                .then_wrapped([&req, &rep, this](auto f) {
                    if (f.failed()) {
                        try {
                            f.get();
                        } catch (httpd::base_exception &e) {
                            return finally(std::move(req), std::move(rep), e.status());
                        } catch (...) {
                            return finally(std::move(req), std::move(rep), http::reply::status_type::unauthorized);
                        }
                        return finally(std::move(req), std::move(rep), http::reply::status_type::internal_server_error);
                    } else {
                        auto plyr = f.get();
                        if (plyr) {
                            _trans->add_player(plyr);

                            rep->write_body("", [plyr, this](output_stream<char> &&out) {
                                return do_with(std::move(out), [plyr, this](auto &out) {
                                    return plyr->start_with(out).finally([&out, plyr] {});
                                });
                            });
                            rep->set_mime_type(format_to_mime_type(plyr->format()));
                            return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
                        } else {
                            return finally(std::move(req), std::move(rep), http::reply::status_type::not_found);
                        }
                    }
                });
        });
}

future<std::unique_ptr<http::reply>>
play_stream_route_handler::handle(
    const sstring &path, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    l.info("http request begin {}: {} {}", req->_method, req->_url, req->_client_address);

    auto app = req->param["app_name"];
    if (app.empty()) return no_app_name_provided(std::move(req), std::move(rep));

    auto stream = req->param["stream_name"];
    if (stream.empty()) return no_stream_name_provided(std::move(req), std::move(rep));

    auto fmt = req->get_query_param("format") != ""
                 ? str2format(req->get_query_param("format"))
                 : format_t::FLV;

    auto media = req->get_query_param("media") != ""
                   ? str2media(req->get_query_param("media"))
                   : media_type_t::all;

    return play_stream(std::move(req), std::move(rep), app, stream, fmt, media);
}

} // namespace route
} // namespace http1
} // namespace amadeus
