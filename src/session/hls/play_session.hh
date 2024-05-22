#pragma once

#include <fmp4-writer.h>
#include <mov-buffer.h>

#include "hls/file_clean.hh"
#include "hls/object.hh"
#include "frame/frame_queue.hh"
#include "flv/frame.hh"
#include "session/play_session.hh"


namespace amadeus {
namespace hls {
namespace session {

namespace fs = std::filesystem;
namespace session_ns = amadeus::session;

using namespace seastar;

using publisher_ptr = session_ns::publisher_ptr;

using media_ptr = std::shared_ptr<flv::media_t>;
using metadata_ptr = std::shared_ptr<flv::metadata_t>;
using frame_ptr = std::shared_ptr<flv::frame_t>;

using tracer_ptr = seastar::shared_ptr<tracer>;
using cleaner_ptr = seastar::shared_ptr<file_cleaner>;

class play_session : public session_ns::play_session {
 public:
    play_session(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        media_type_t media_type = media_type_t::all,
        version_t v = version_t::v3);
    play_session(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        media_type_t media_type = media_type_t::all,
        version_t v = version_t::v3);
    virtual ~play_session() = default;

    virtual void start();

    version_t version() const;

    virtual sstring to_string() const;

    virtual void subscribe() override;

 protected:
    future<> init_directory(const fs::path &directory);

    future<> do_start();
    future<> do_write();
    future<> write_once();

    virtual future<> do_write_frame(frame_ptr pkt) = 0;
    virtual void create_tracers() = 0;

    virtual void on_begin() override;
    virtual void on_end() override;
    virtual void on_settings_update() override;

    bool out_of_gop_duration(int64_t pts, float max_duration);
    future<> remove_first_gop_play_items();

    fragment_info_ptr find_second_key_frame_play_item();
    fragment_info_ptr find_last_key_frame_play_item();

    virtual future<> add_play_item(fragment_info_ptr item, float min_duration);

    future<> on_update_header(fragment_info_ptr header, metadata_ptr metadata);
    future<> on_add_playitem(fragment_info_ptr item);
    future<> on_remove_playitem(fragment_info_ptr item);
    future<> on_update_playlist(const playlist_t &playlist);

    future<> for_each_tracer(std::function<future<>(tracer_ptr &)> func);


    std::vector<tracer_ptr> all_tracers() {
        std::vector<tracer_ptr> tracers;
        for (auto e : _tracers) { tracers.push_back(e.second); }
        for (auto e : _cleaners) { tracers.push_back(e.second); }

        return tracers;
    }

    template <typename Func>
    future<> do_for_each_tracer(Func func) {
        return do_with(all_tracers(), [func = std::move(func)](auto &tracers) {
            return do_for_each(tracers.begin(), tracers.end(), [func = std::move(func)](auto &tracer) {
                return func(tracer);
            });
        });
    }

    version_t _version;

    fs::path _directory;

    playlist_t _playlist;

    std::unordered_map<sstring, tracer_ptr> _tracers;
    std::unordered_map<sstring, cleaner_ptr> _cleaners;
};

} // namespace session
} // namespace hls

using hls_player_ptr = std::shared_ptr<hls::session::play_session>;

} // namespace amadeus
