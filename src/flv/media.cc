#include "flv/media.hh"

#include <flv-proto.h>
#include <seastar/util/log.hh>

#include "flv/flv.hh"
#include "flv/log.hh"
#include "flv/metadata.hh"

namespace amadeus {
namespace flv {

using namespace seastar;
using media_ptr = std::shared_ptr<flv::media_t>;

sstring
media_t::to_string() const {
    return fmt::format(
        "{} is_video: {} is_keyframe: {} dts: {} media_data: {}",
        seastar::pretty_type_name(typeid(flv::media_t)),
        _is_video,
        _is_keyframe,
        _dts,
        _data.size());
}

audio_media_t::audio_media_t(uint64_t dts, temporary_buffer<uint8_t> data) {
    _is_video = false;
    _dts = dts;
    _data = std::move(data);
}

temporary_buffer<uint8_t>
audio_media_t::tag_header(metadata_t *metadata) const {
    assert(metadata);
    assert(metadata->audio.is_enabled());

    if (!metadata->audio.is_enabled()) return temporary_buffer<uint8_t>();

    // video tag
    ::flv_audio_tag_header_t ath;
    ath.codecid = metadata->audio.codecid;
    ath.avpacket = FLV_AVPACKET;
    ath.rate = flv_rate_index_from_samplerate(metadata->audio.samplerate);
    ath.bits = flv_bits_index_from_samplesize(metadata->audio.samplesize);
    ath.channels = metadata->audio.stereo ? 1 : 0;

    temporary_buffer<uint8_t> buf(FLV_AUDIO_TAG_HEADER_SIZE);
    int len =
        ::flv_audio_tag_header_write(&ath, reinterpret_cast<uint8_t *>(buf.get_write()), FLV_AUDIO_TAG_HEADER_SIZE);
    if (len < 0) return temporary_buffer<uint8_t>();

    buf.trim(len);

    return buf;
}

temporary_buffer<uint8_t>
audio_media_t::to_tag_data(metadata_t *metadata) const {
    assert(metadata);
    temporary_buffer<uint8_t> athb = tag_header(metadata);
    if (athb.empty()) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> buf(athb.size() + _data.size());

    std::copy_n(athb.get(), athb.size(), buf.get_write());
    std::copy_n(_data.get(), _data.size(), buf.get_write() + athb.size());

    return buf;
}

sstring
audio_media_t::to_string() const {
    return fmt::format(
        "{} is_video: {} is_keyframe: {} dts: {} media_data: {}",
        seastar::pretty_type_name(typeid(flv::audio_media_t)),
        _is_video,
        _is_keyframe,
        _dts,
        _data.size());
}

video_media_t::video_media_t(int keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data)
: video_media_t(keyframe == 1, cts, dts, std::move(data)) {}

video_media_t::video_media_t(bool is_keyframe, uint64_t cts, uint64_t dts, temporary_buffer<uint8_t> data) {
    _is_video = true;
    _is_keyframe = is_keyframe;
    _data = std::move(data);
    _dts = dts;
    _cts = cts;
}

temporary_buffer<uint8_t>
video_media_t::tag_header(metadata_t *metadata) const {
    assert(metadata);
    assert(metadata->video.is_enabled());

    if (!metadata->video.is_enabled()) return temporary_buffer<uint8_t>();

    // video tag
    ::flv_video_tag_header_t vth;
    vth.codecid = metadata->video.codecid;
    vth.avpacket = FLV_AVPACKET;
    vth.keyframe = _is_keyframe ? 1 : 2; // 1-key media_t, 2-inter media_t
    vth.cts = _cts;

    temporary_buffer<uint8_t> buf(FLV_VIDEO_TAG_HEADER_SIZE);
    int len = ::flv_video_tag_header_write(&vth, buf.get_write(), FLV_VIDEO_TAG_HEADER_SIZE);
    if (len < 0) return temporary_buffer<uint8_t>();

    buf.trim(len);

    return buf;
}

temporary_buffer<uint8_t>
video_media_t::to_tag_data(metadata_t *metadata) const {
    assert(metadata);

    temporary_buffer<uint8_t> vthb = tag_header(metadata);
    if (vthb.empty()) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> buf(vthb.size() + _data.size());

    std::copy_n(vthb.get(), vthb.size(), buf.get_write());
    std::copy_n(_data.get(), _data.size(), buf.get_write() + vthb.size());

    return buf;
}

sstring
video_media_t::to_string() const {
    return fmt::format(
        "{} is_video: {} is_keyframe: {} dts: {} cts: {} media_data: {}",
        seastar::pretty_type_name(typeid(flv::video_media_t)),
        _is_video,
        _is_keyframe,
        _dts,
        _cts,
        _data.size());
}

} // namespace flv
} // namespace amadeus
