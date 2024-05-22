#pragma once

#include <mpeg-muxer.h>
#include <mpeg4-aac.h>
#include <mpeg4-avc.h>
#include <mpeg4-hevc.h>
#include <seastar/core/sstring.hh>
#include <seastar/core/temporary_buffer.hh>

#include "session/hls/play_session.hh"

namespace amadeus {
namespace hls {
namespace session {

using namespace seastar;

class play_session_v3 : public play_session {
 public:
    play_session_v3(
        publisher_ptr pub,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        media_type_t media_type = media_type_t::all);
    play_session_v3(
        publisher_ptr pub,
        const sstring &app,
        const sstring &stream,
        const sstring &internal_url = "",
        const arguments_t &args = {},
        media_type_t media_type = media_type_t::all);
    virtual ~play_session_v3();

 protected:
    struct ts_stream_info {
        int stream_id;
        int codec_id;
        int pid;

        union {
            struct ::mpeg4_avc_t avc;
            struct ::mpeg4_hevc_t hevc;
            struct ::mpeg4_aac_t aac;
        } config;
    };

    struct mpeg_frame {
        int stream_id;
        media_ptr raw_frame;
        temporary_buffer<uint8_t> data;
    };

    using mpeg_frame_ptr = std::shared_ptr<mpeg_frame>;

    struct fragment_t {
        int64_t id = 1;

        int64_t last_dts = -1;
        int64_t first_dts = -1;
        int64_t first_pts = -1;

        bool start_by_keyframe = false;

        size_t size = 0;

        void write(temporary_buffer<uint8_t> data) {
            if (data.size() == 0) return;

            size += data.size();
            bufs.push_back(std::move(data));
        }

        std::vector<temporary_buffer<uint8_t>> bufs;
    };

    using fragment_ptr = std::shared_ptr<fragment_t>;

    virtual void create_tracers() override;

    virtual future<> do_write_frame(frame_ptr pkt) override;
    virtual future<> add_play_item(fragment_info_ptr item, float min_duration) override;

    future<> handle_meta(metadata_ptr meta);
    future<> handle_frame(media_ptr bmif);
    future<> write_ts_frame(media_ptr bmif);

    fragment_info_ptr make_fragment_info(fragment_ptr frag, media_ptr frame);
    future<> dump_fragment(fragment_ptr fragment, media_ptr frame);
    future<> add_fragment_file(fragment_info_ptr info, fragment_ptr frag);
    fragment_ptr make_fragment(media_ptr frame);
    bool is_timeout_fragment(fragment_ptr frag, media_ptr frame, float duration) const;
    bool is_fragment_header_frame(fragment_ptr frag, media_ptr frame) const;

    std::shared_ptr<ts_stream_info> add_stream(flv::video_meta_t& video_meta);
    std::shared_ptr<ts_stream_info> add_stream(flv::audio_meta_t& audio_meta);

    mpeg_frame_ptr make_ts_frame(media_ptr frame);
    temporary_buffer<uint8_t> make_frame_data(int codec_id, uint8_t *data, size_t bytes);
    temporary_buffer<uint8_t> to_aac_adts(const mpeg4_aac_t *config, uint8_t *data, size_t len);
    temporary_buffer<uint8_t> to_h264_annexb(const mpeg4_avc_t *config, uint8_t *data, size_t len);
    temporary_buffer<uint8_t> to_hevc_annexb(const mpeg4_hevc_t *config, uint8_t *data, size_t len);
    temporary_buffer<uint8_t> avcc_to_annexb(const uint8_t *data, size_t len);
 private:
    static int on_data(void *param, const void *data, size_t bytes);
    static int deep_copy_mpeg4_avc(mpeg4_avc_t *dst, const mpeg4_avc_t *src);
    static int deep_copy_mpeg4_aac(mpeg4_aac_t* dst, const mpeg4_aac_t* src);
    static int deep_copy_mpeg4_hevc(mpeg4_hevc_t *dst, const mpeg4_hevc_t *src);

    bool _program_enabled = false;
    int _program_number = 1;

    std::shared_ptr<ts_stream_info> _audio_stream;
    std::shared_ptr<ts_stream_info> _video_stream;

    fragment_ptr _fragment;

    int64_t _next_fragment_id = 0;

    void *_muxer = nullptr;
};

} // namespace session
} // namespace hls

using play_session_v3_ptr = std::shared_ptr<hls::session::play_session_v3>;

} // namespace amadeus
