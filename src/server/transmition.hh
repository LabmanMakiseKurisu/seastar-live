/*
 * @Author: Amadeus
 * @Date: 2024-04-23 14:51:50
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-10 18:21:30
 * @FilePath: /Amadeus/src/server/transmition.hh
 * @Description:
 */
#pragma once

#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>
#include <seastar/core/seastar.hh>
#include <seastar/net/api.hh>

#include <type_traits>

#include "app/global_setting.hh"
#include "session/play_session.hh"
#include "session/publish_session.hh"
#include "session/rtmp/play_session.hh"
#include "session/rtmp/publish_session.hh"
#include "session/http1/play_session.hh"

namespace amadeus {
namespace server {
using namespace seastar;
using options_map = boost::program_options::variables_map;

namespace session_ns = amadeus::session;

// 管理多个session的类，session专用的容器
template <typename T>
class session_list {
 public:
    template <typename S>
    using element_to = std::shared_ptr<S>;

    using element_t = std::shared_ptr<T>;
    using default_condition_type = std::function<bool(element_t)>;

 protected:
    mutable std::mutex _lock;
    std::vector<element_t> _sessions;

 public:
    template <typename S = T>
    bool remove_when(std::function<bool(element_to<S>)> condition);

    template <typename S = T>
    size_t size_when(std::function<bool(element_to<S>)> condition) const;

    template <typename S = T>
    element_to<S> find_any_when(std::function<bool(element_to<S>)> condition) const;

    template <typename S = T>
    std::vector<element_to<S>> find_all_when(std::function<bool(element_to<S>)> condition) const;

    template <typename S = T>
    bool is_exist_when(std::function<bool(element_to<S>)> condition) const noexcept;

    template <typename S = T>
    void for_each_when(
        std::function<bool(element_to<S>)> condition, std::function<void(element_to<S>)> action) const noexcept;

    template <typename S = T>
    void for_each(std::function<void(element_to<S>)> action) const noexcept;

    template <typename S = T>
    void for_each(
        const sstring &app,
        const sstring &stream,
        ownership_t os,
        format_t fmt,
        protocol_t prot,
        std::function<bool(element_to<S>)> condition,
        std::function<void(element_to<S>)> action) const;

    template <typename S = T>
    element_to<S> find_any(
        const sstring &app = "",
        const sstring &stream = "",
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(element_to<S>)> condition = nullptr) const;

    template <typename S = T>
    std::vector<element_to<S>> find_all(
        const sstring &app = "",
        const sstring &stream = "",
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(element_to<S>)> condition = nullptr) const;

    template <typename S = T>
    size_t size(
        const sstring &app = "",
        const sstring &stream = "",
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(element_to<S>)> condition = nullptr) const;

    template <typename S = T>
    void remove_all(const sstring &app, const sstring &stream);

    template <typename S = T>
    bool is_exist(
        const sstring &app = "",
        const sstring &stream = "",
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(element_to<S>)> condition = nullptr) const;

    bool add(element_t session);
    void remove(element_t session);
    bool is_exist(element_t session) const;

    const std::vector<element_t> &get() const;
};

class transmition : public session::lifecycle {
 protected:
    session_list<session::play_session> _players;
    session_list<session::publish_session> _publishers;

    mutable std::mutex _lock;
 public:
    static const sstring version;

    transmition();
    virtual ~transmition() = default;

    void initialize(const global_settings &settings);

    //检查要创建的publisher session是否存在，不存在则创建
    future<publisher_ptr> make_valid_publisher(
        protocol_t prot,
        format_t fmt,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "");

    // 检查其对应的publisher session是否存在，不存在则创建
    future<player_ptr> make_valid_player(
        protocol_t prot,
        format_t fmt,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        media_type_t media_type = media_type_t::all);

    // 查询_publishers数组的元素个数
    size_t get_publishers_size(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(publisher_ptr)> condition = nullptr) const {
        return _publishers.size<session::publish_session>(app, stream, os, fmt, prot, std::move(condition));
    }

    //找到特定的publisher
    publisher_ptr find_publisher(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(publisher_ptr)> condition = nullptr) const;

    //找到满足condition的所有publisher
    std::vector<publisher_ptr> find_publishers(
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(publisher_ptr)> condition = nullptr) const;

    // 查询_players数组的元素个数
    size_t get_players_size(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(player_ptr)> condition = nullptr) const {
        return _players.size<session::play_session>(app, stream, os, fmt, prot, std::move(condition));
    }

    //找到任意一个满足条件的player
    player_ptr find_any_player(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(player_ptr)> condition = nullptr) const;

    //找到所有满足条件的player
    std::vector<player_ptr> find_players(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(player_ptr)> condition = nullptr) const;

    const std::vector<publisher_ptr> &all_publishers() const;

    bool add_publisher(publisher_ptr pub);
    void remove_publisher(publisher_ptr pub);
    void remove_publisher(session::publish_session *pub);
    void remove_publishers(const sstring &app, const sstring &stream);
    bool is_exist_publisher(const sstring &app, const sstring &stream) const;
    bool is_exist_publisher(publisher_ptr pub) const;

    const std::vector<player_ptr> &all_players() const;
    bool add_player(player_ptr plyr);
    void remove_player(player_ptr plyr);
    void remove_player(session::play_session *plyr);
    void remove_players(const sstring &app, const sstring &stream);
    bool is_exist_player(const sstring &app, const sstring &stream) const;
    bool is_exist_player(player_ptr plyr) const;

    std::vector<session_ptr> all_sessions() const;
    std::vector<session_ptr> find_sessions(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(session_ptr)> condition = nullptr) const;

    session_ptr find_session(const sstring &id) const;
    publisher_ptr find_publisher(const sstring &id) const;
    player_ptr find_player(const sstring &id) const;

    size_t get_rtmp_publishers_size(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_publisher_ptr)> condition = nullptr) const;
    rtmp_publisher_ptr find_rtmp_publisher(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_publisher_ptr)> condition = nullptr) const;
    std::vector<rtmp_publisher_ptr> find_rtmp_publishers(
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_publisher_ptr)> condition = nullptr) const;

    size_t get_rtmp_players_size(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_player_ptr)> condition = nullptr) const;
    rtmp_player_ptr find_any_rtmp_player(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_player_ptr)> condition = nullptr) const;
    std::vector<rtmp_player_ptr> find_rtmp_players(
        const sstring &app,
        const sstring &stream,
        ownership_t os = ownership_t::ignored,
        format_t fmt = format_t::ignored,
        protocol_t prot = protocol_t::none,
        std::function<bool(rtmp_player_ptr)> condition = nullptr) const;

 protected:
    future<> validate(
        type_t type,
        protocol_t prot,
        format_t fmt,
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url,
        const arguments_t &args,
        const sstring &address);

    publisher_ptr make_publisher(
        protocol_t prot,
        format_t fmt,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "");
    player_ptr make_player(
        publisher_ptr pub,
        protocol_t prot,
        format_t fmt,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        media_type_t media_type = media_type_t::all);
    player_ptr make_player(
        publisher_ptr pub,
        protocol_t prot,
        format_t fmt,
        const sstring &id,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        const sstring &address = "",
        media_type_t media_type = media_type_t::all);

    void
    for_each_session(const sstring &app, const sstring &stream, std::function<void(session_ptr session)> func) const;

    //返回总的session数量
    size_t total_count() const;

    void on_add_publisher(publisher_ptr pub);
    void on_remove_publisher(publisher_ptr pub);

    void on_add_player(player_ptr plyr);
    void on_remove_player(player_ptr plyr);

    void stop_all_players_after_publisher_shutdown(publisher_ptr pub);
    void stop_publisher_after_player_shutdown(player_ptr plyr, const sstring &mode);

 protected:
    /*** session lifecycle ***/
    virtual void on_fail(session_ptr s, status_t status, const sstring &message) override;
    virtual void on_cancel(session_ptr s) override;
    virtual void on_launch(session_ptr s) override;
    virtual void on_begin(session_ptr s) override;
    virtual void on_done(session_ptr s) override;

    void on_session_end(session_ptr s);
};

} // namespace server

using transmition_ptr = std::shared_ptr<server::transmition>;
} // namespace amadeus