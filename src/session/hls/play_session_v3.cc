#include "session/hls/play_session_v3.hh"

#include <seastar/core/fstream.hh>
#include <seastar/core/thread.hh>
#include <seastar/util/file.hh>
#include <seastar/util/log.hh>

#include "hls/file_clean.hh"
#include "hls/m3u8.hh"
#include "play_session_v3.hh"
#include "session/log.hh"
#include "session/publish_session.hh"
#include "util/util.hh"

namespace amadeus {
namespace hls {
namespace session {

using namespace session_ns;

static void *
on_alloc(void *param, size_t bytes) {
    return malloc(bytes);
}

static void
on_free(void *param, void *buffer) {
    free(buffer);
}

play_session_v3::play_session_v3(
    publisher_ptr pub, const sstring &internal_url, const arguments_t &args, media_type_t media_type)
: play_session_v3(pub, pub->app(), pub->stream(), internal_url, args, media_type) {}

play_session_v3::play_session_v3(
    publisher_ptr pub,
    const sstring &app,
    const sstring &stream,
    const sstring &internal_url,
    const arguments_t &args,
    media_type_t media_type)
: play_session(pub, app, stream, internal_url, args, media_type, version_t::v3) {
    ::mpeg_ts_func_t handler;
    handler.alloc = on_alloc;
    handler.free = on_free;
    handler.write = on_data;
    _muxer = ::mpeg_ts_create(&handler, this);

    using namespace std::chrono;
    // unix timestamp - (2022-01-01 00:00:00)
    _next_fragment_id = duration_cast<seconds>(system_clock::now().time_since_epoch()).count() - 1640966400;
}

play_session_v3::~play_session_v3() {
    ::mpeg_ts_destroy(_muxer);
}

static int
flv_to_mpeg_ts_codecid(int codec_id) {
    switch (codec_id) {
        case FLV_VIDEO_H264: return PSI_STREAM_H264;
        case FLV_VIDEO_H265: return PSI_STREAM_H265;
        case FLV_AUDIO_AAC: return PSI_STREAM_AAC;
        case FLV_AUDIO_MP3: return PSI_STREAM_MP3;
        case FLV_AUDIO_OPUS: return PSI_STREAM_AUDIO_OPUS;
        case FLV_AUDIO_G711A: return PSI_STREAM_AUDIO_G711A;
        case FLV_AUDIO_G711U: return PSI_STREAM_AUDIO_G711U;
        case FLV_VIDEO_AV1: return PSI_STREAM_AV1;
        default: return -1;
    }
}

void
play_session_v3::create_tracers() {
    auto path = fmt::format("{}/{}/{}", global_settings::global.hls_ts_base_directory(), _app, _stream);

    _directory = path;

    auto save_timestamp_playlist = g_settings().hls_ts_save_timestamp_playlist();
    auto delete_delay = g_settings().hls_ts_fragment_file_delete_delay();

    _tracers.clear();

    if (delete_delay > 0 && (_tracers.empty() || _tracers.find(path) == _tracers.end())) {
        _tracers[path] = seastar::make_shared<m3u8_writer_v3>(path, save_timestamp_playlist, delete_delay);
    }

    if (delete_delay > 0 && (_cleaners.empty() || _cleaners.find(path) == _cleaners.end())) {
        _cleaners[path] = seastar::make_shared<file_cleaner>(delete_delay);
    }
    for (auto e : _cleaners) { e.second->update_delay(delete_delay); }
}

future<>
play_session_v3::do_write_frame(frame_ptr frame) {
    if (is_complete()) return make_ready_future<>();

    if (frame->is_media) {
        return handle_frame(frame->media);
    } else if (frame->is_metadata) {
        return handle_meta(frame->metadata);
    } else {
        // unknown framme
        return make_ready_future();
    }
}

std::shared_ptr<play_session_v3::ts_stream_info>
play_session_v3::add_stream(flv::video_meta_t &video_meta) {
    auto codec_id = video_meta.codecid;
    auto ret = ::mpeg_ts_add_program_stream(_muxer, _program_number, flv_to_mpeg_ts_codecid(codec_id), nullptr, 0);
    if (ret < 0) throw std::runtime_error(fmt::format("failed to add ts stream: {}", ret));
    auto info = std::make_shared<ts_stream_info>();
    info->stream_id = ret;
    info->codec_id = codec_id;
    switch (codec_id) {
        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: {
            deep_copy_mpeg4_avc(&info->config.avc, &video_meta.v.avc);
            break;
        }
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: {
            deep_copy_mpeg4_hevc(&info->config.hevc, &video_meta.v.hevc);
            break;
        }
        default: throw std::runtime_error(fmt::format("unsupported video codec id: {}", codec_id));
    }
    return info;
}

std::shared_ptr<play_session_v3::ts_stream_info>
play_session_v3::add_stream(flv::audio_meta_t &audio_meta) {
    auto codec_id = audio_meta.codecid;
    auto ret = ::mpeg_ts_add_program_stream(_muxer, _program_number, flv_to_mpeg_ts_codecid(codec_id), nullptr, 0);
    if (ret < 0) throw std::runtime_error(fmt::format("failed to add ts stream: {}", ret));
    auto info = std::make_shared<ts_stream_info>();
    info->stream_id = ret;
    info->codec_id = codec_id;
    switch (codec_id) {
        case FLV_AUDIO_AAC:
        case FLV_AUDIO_ASC: {
            deep_copy_mpeg4_aac(&info->config.aac, &audio_meta.aac);
            break;
        }
        default: throw std::runtime_error(fmt::format("unsupported audio codec id: {}", codec_id));
    }
    return info;
}

future<>
play_session_v3::handle_meta(metadata_ptr metadata) {
    // 分别创建视频和音频轨道
    //aac.profile >0 avc.profile >0
    if (!_program_enabled) {
        mpeg_ts_add_program(_muxer, 1, "BVC-LIVE", sizeof("BVC-LIVE"));
        _program_enabled = true;
    } 

    if(metadata->video.v.avc.profile>0&&!_video_stream)
        _video_stream = add_stream(metadata->video);
    if(metadata->audio.aac.profile>0&&!_audio_stream)
        _audio_stream = add_stream(metadata->audio);

    auto header = std::make_shared<fragment_info>();
    header->is_header = true;

    return on_update_header(header, metadata);
}

future<>
play_session_v3::handle_frame(media_ptr frame) {
    if (!_fragment) _fragment = make_fragment(frame);

    if (is_fragment_header_frame(_fragment, frame)) {
        ::mpeg_ts_reset(_muxer);

        return dump_fragment(_fragment, frame).then([this, frame] {
            _fragment = nullptr;
            return write_ts_frame(frame);
        });
    } else {
        return write_ts_frame(frame);
    }
}

future<>
play_session_v3::write_ts_frame(media_ptr frame) {
    if (!_fragment) { _fragment = make_fragment(frame); }

    auto mpeg_frame = make_ts_frame(frame);
    auto ret = ::mpeg_ts_write(
        _muxer,
        mpeg_frame->stream_id,
        frame->is_keyframe() ? 0 : 1,
        (frame->dts() + frame->cts()) * 90,
        frame->dts() * 90,
        mpeg_frame->data.get_write(),
        mpeg_frame->data.size());
    if (ret != 0) {
        return make_exception_future(std::runtime_error(fmt::format("failed to add mpegts frame: {}", ret)));
    }
    return make_ready_future();
}

future<>
play_session_v3::dump_fragment(fragment_ptr frag, media_ptr frame) {
    auto info = make_fragment_info(frag, frame);
    if (!info) return make_ready_future<>();

    return add_fragment_file(info, frag);
}

fragment_info_ptr
play_session_v3::make_fragment_info(fragment_ptr frag, media_ptr frame) {
    if (!frag || frag->size == 0) return nullptr;
    auto info = std::make_shared<fragment_info>();

    auto filename = to_sstring(frag->id) + ".ts";
    auto filepath = _directory / fs::path(filename);

    info->id = frag->id;
    info->pts = frag->first_pts;
    info->start_by_keyframe = frag->start_by_keyframe;
    info->size = frag->size;
    info->duration = frame->dts() - frag->first_dts;
    info->filepath = filepath;
    using namespace std::chrono;
    info->ctime = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    return info;
}

future<>
play_session_v3::add_fragment_file(fragment_info_ptr info, fragment_ptr frag) {
    assert(info && frag);

    if (is_complete()) return make_ready_future<>();

    return with_file_output_stream(
               info->filepath.native(),
               open_flags::rw | open_flags::create,
               [frag](output_stream<char> &os) {
                   return do_for_each(frag->bufs, [frag, &os](auto &buf) {
                       auto ptr = reinterpret_cast<char *>(buf.get_write());
                       return os.write(ptr, buf.size());
                   });
               })
        .then([info, this] {
            l.info("wrote fragment file {} size: {}", info->filepath, info->size);
            return add_play_item(info, g_settings().hls_ts_playlist_min_duration());
        })
        .handle_exception([info, this](auto e) {
            l.warn("failed to write fragment file {} {}", info->filepath, e);
            return make_exception_future<>(e);
        });
}

play_session_v3::fragment_ptr
play_session_v3::make_fragment(media_ptr frame) {
    auto frag = std::make_shared<fragment_t>();
    frag->id = ++_next_fragment_id;
    frag->first_dts = frame->dts();
    frag->first_pts = frame->dts() + frame->cts();
    frag->start_by_keyframe = frame->is_keyframe();

    return frag;
}

bool
play_session_v3::is_timeout_fragment(fragment_ptr frag, media_ptr frame, float duration) const {
    auto timeoffset = frame->dts() - frag->first_dts;
    auto timeout = timeoffset >= (duration * 1000);

    return timeout;
}

bool
play_session_v3::is_fragment_header_frame(fragment_ptr frag, media_ptr frame) const {
    auto video_keyframe = frame->is_video() && frame->is_keyframe();
    auto duration = g_settings().hls_ts_fragment_duration();
    auto fragment_timeout = is_timeout_fragment(frag, frame, duration);

    return video_keyframe || fragment_timeout;
}


future<>
play_session_v3::add_play_item(fragment_info_ptr item, float min_duration) {
    return play_session::add_play_item(item, min_duration);
}


int
play_session_v3::on_data(void *param, const void *data, size_t bytes) {
    auto session = reinterpret_cast<play_session_v3 *>(param);
    session->_fragment->write(temporary_buffer<uint8_t>(reinterpret_cast<const uint8_t *>(data), bytes));

    return 0;
}

int
play_session_v3::deep_copy_mpeg4_avc(mpeg4_avc_t *dst, const mpeg4_avc_t *src) {
    if (src == NULL || dst == NULL) return -1;

    // 复制基本数据类型
    dst->profile = src->profile;
    dst->compatibility = src->compatibility;
    dst->level = src->level;
    dst->nalu = src->nalu;
    dst->nb_sps = src->nb_sps;
    dst->nb_pps = src->nb_pps;
    dst->chroma_format_idc = src->chroma_format_idc;
    dst->bit_depth_luma_minus8 = src->bit_depth_luma_minus8;
    dst->bit_depth_chroma_minus8 = src->bit_depth_chroma_minus8;
    dst->off = src->off;

    // 复制data数组内容
    memcpy(dst->data, src->data, sizeof(src->data));

    // 更新SPS中的指针
    for (int i = 0; i < src->nb_sps; ++i) {
        dst->sps[i].bytes = src->sps[i].bytes;
        dst->sps[i].data = dst->data + (src->sps[i].data - src->data);
    }

    // 更新PPS中的指针
    for (int i = 0; i < src->nb_pps; ++i) {
        dst->pps[i].bytes = src->pps[i].bytes;
        dst->pps[i].data = dst->data + (src->pps[i].data - src->data);
    }

    return 0;
}

int
play_session_v3::deep_copy_mpeg4_aac(mpeg4_aac_t *dst, const mpeg4_aac_t *src)  {
    if (src == NULL || dst == NULL)
        return -1;

    // 直接拷贝整个结构体内容
    memcpy(dst, src, sizeof(mpeg4_aac_t));

    return 0;
}

int
play_session_v3::deep_copy_mpeg4_hevc(mpeg4_hevc_t *dst, const mpeg4_hevc_t *src) {
    if (src == NULL || dst == NULL) return -1;

    // 复制基本数据类型
    dst->configurationVersion = src->configurationVersion;
    dst->general_profile_space = src->general_profile_space;
    dst->general_tier_flag = src->general_tier_flag;
    dst->general_profile_idc = src->general_profile_idc;
    dst->general_profile_compatibility_flags = src->general_profile_compatibility_flags;
    dst->general_constraint_indicator_flags = src->general_constraint_indicator_flags;
    dst->general_level_idc = src->general_level_idc;
    dst->min_spatial_segmentation_idc = src->min_spatial_segmentation_idc;
    dst->parallelismType = src->parallelismType;
    dst->chromaFormat = src->chromaFormat;
    dst->bitDepthLumaMinus8 = src->bitDepthLumaMinus8;
    dst->bitDepthChromaMinus8 = src->bitDepthChromaMinus8;
    dst->avgFrameRate = src->avgFrameRate;
    dst->constantFrameRate = src->constantFrameRate;
    dst->numTemporalLayers = src->numTemporalLayers;
    dst->temporalIdNested = src->temporalIdNested;
    dst->lengthSizeMinusOne = src->lengthSizeMinusOne;
    dst->numOfArrays = src->numOfArrays;
    dst->array_completeness = src->array_completeness;
    dst->off = src->off;

    // 复制data数组内容
    memcpy(dst->data, src->data, sizeof(src->data));

    // 更新NALU中的指针
    for (int i = 0; i < src->numOfArrays; ++i) {
        dst->nalu[i].array_completeness = src->nalu[i].array_completeness;
        dst->nalu[i].type = src->nalu[i].type;
        dst->nalu[i].bytes = src->nalu[i].bytes;
        dst->nalu[i].data = dst->data + (src->nalu[i].data - src->data);
    }

    return 0;
}


play_session_v3::mpeg_frame_ptr
play_session_v3::make_ts_frame(media_ptr frame) {
    auto stream = frame->is_video() ? _video_stream : _audio_stream;
    if (!stream) throw std::runtime_error(fmt::format("no {} stream", frame->is_video() ? "video" : "audio"));

    auto ts_frame = std::make_shared<mpeg_frame>();
    ts_frame->stream_id = stream->stream_id;
    ts_frame->raw_frame = frame;
    ts_frame->data = make_frame_data(stream->codec_id,frame->data().get_write(),frame->data().size());

    return ts_frame;
}

temporary_buffer<uint8_t>
play_session_v3::make_frame_data(int codec_id, uint8_t *data, size_t bytes) {
    switch (codec_id) {
        case FLV_VIDEO_H264: return to_h264_annexb(&_video_stream->config.avc, data, bytes);
        case FLV_VIDEO_H265: return to_hevc_annexb(&_video_stream->config.hevc, data, bytes);
        case FLV_AUDIO_AAC: return to_aac_adts(&_audio_stream->config.aac, data, bytes);
        default: throw std::runtime_error(fmt::format("unsupported audio codec id: {}", codec_id));
    }
}

temporary_buffer<uint8_t>
play_session_v3::to_aac_adts(const mpeg4_aac_t *config, uint8_t *data, size_t len) {
    int size = 1024 + len;
    temporary_buffer<uint8_t> buf(size);

    auto ret = mpeg4_aac_adts_save(config, len, buf.get_write(), size);
    if (ret < 0) { throw std::runtime_error("failed to make AAC ADTS"); }

    std::copy_n(data, len, buf.get_write() + ret);
    buf.trim(ret + len);

    return buf;
}

temporary_buffer<uint8_t>
play_session_v3::to_h264_annexb(const mpeg4_avc_t *config, uint8_t *data, size_t len) {
    int size = 1024 + len;
    temporary_buffer<uint8_t> buf(size);
    mpeg4_avc_t cfg;
    cfg = *config;
    auto ret = h264_mp4toannexb(&cfg, data, len, buf.get_write(), size);
    if (ret < 0) { throw std::runtime_error("failed to make h264 annex-B"); }

    buf.trim(ret);

    return buf;
}

temporary_buffer<uint8_t>
play_session_v3::to_hevc_annexb(const mpeg4_hevc_t *config, uint8_t *data, size_t len) {
    int size = 1024 + len;
    temporary_buffer<uint8_t> buf(size);

    auto ret = h265_mp4toannexb(config, data, len, buf.get_write(), size);
    if (ret < 0) { throw std::runtime_error("failed to make h264 annex-B"); }

    buf.trim(ret);

    return buf;
}

} // namespace session
} // namespace hls
} // namespace amadeus
