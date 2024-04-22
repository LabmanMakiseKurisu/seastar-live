#include "flv/metadata.hh"

#include <amf0.h>

#include "flv/flv.hh"
#include "flv/log.hh"
#include "flv/media.hh"
#include "util/codec.hh"
#include "metadata.hh"

namespace amadeus {
namespace flv {

using namespace seastar;

#define H265_NAL_VPS 32
#define H265_NAL_SPS 33
#define H265_NAL_PPS 34
#define H265_NAL_AUD 35

/// 音频采样频率: 0-5.5 kHz, 1-11 kHz, 2-22 kHz, 3-44 kHz
static uint32_t flv_rates[4] = {5500, 11025, 22050, 44100};
/// 音频采样位深: 0-8 bit samples, 1-16-bit samples
static uint32_t flv_bits[2] = {8, 16};

int32_t
samplerate_from_flv_rate_index(size_t index) {
    size_t size = sizeof(flv_rates) / sizeof(uint32_t);
    if (index > size) return -1;

    return flv_rates[index];
}

int
flv_rate_index_from_samplerate(uint32_t samplerate) {
    size_t size = sizeof(flv_rates) / sizeof(uint32_t);
    for (size_t i = 0; i < size; i++) {
        if (flv_rates[i] == samplerate) return i;
    }

    if (samplerate > flv_rates[size - 1]) return size - 1;
    return -1;
}

int32_t
samplesize_from_flv_bits_index(size_t index) {
    size_t size = sizeof(flv_bits) / sizeof(uint32_t);
    if (index > size) return -1;

    return flv_bits[index];
}

int
flv_bits_index_from_samplesize(uint32_t samplesize) {
    for (size_t i = 0; i < sizeof(flv_bits) / sizeof(uint32_t); i++) {
        if (flv_bits[i] == samplesize) return i;
    }
    return -1;
}

meta_t::meta_t(int32_t _codecid, double _bitrate, double _datarate)
: codecid(_codecid)
, bitrate(_bitrate)
, datarate(_datarate) {}

sstring
meta_t::to_string() const {
    return fmt::format(
        "{} valid: {} codecid: {} bitrate: {} datarate: {}",
        seastar::pretty_type_name(typeid(flv::media_t)),
        _valid,
        codecid,
        bitrate,
        datarate);
}

sstring
meta_t::codec_name() const {
    return codec_name_by_id(codecid);
}

audio_meta_t::audio_meta_t() {
    memset(&aac, 0, sizeof(mpeg4_aac_t));
}

audio_meta_t::audio_meta_t(const audio_meta_t &x)
: meta_t(x.codecid, x.bitrate, x.datarate)
, samplerate(x.samplerate)
, samplesize(x.samplesize)
, channel_count(x.channel_count)
, stereo(x.stereo) {
    _valid = x._valid;
    memset(&aac, 0, sizeof(mpeg4_aac_t));
    memcpy(&aac, &x.aac, sizeof(mpeg4_aac_t));
}

audio_meta_t &
audio_meta_t::operator=(const audio_meta_t &x) {
    codecid = x.codecid;
    bitrate = x.bitrate;
    datarate = x.datarate;

    samplerate = x.samplerate;
    samplesize = x.samplesize;
    stereo = x.stereo;
    channel_count = x.channel_count;

    _valid = x._valid;

    memcpy(&aac, &x.aac, sizeof(mpeg4_aac_t));

    return *this;
}

int
audio_meta_t::_load(int32_t cid, const uint8_t *data, int bytes) {
    codecid = cid;

    if (bytes <= 0) return -1;
    switch (cid) {
        case FLV_AUDIO_AAC:
        case FLV_AUDIO_ASC: {
            int rt = ::mpeg4_aac_audio_specific_config_load(data, bytes, &aac);
            if (rt < 0) return rt;

            channel_count = aac.channels;
            samplerate = aac.sampling_frequency;
            codec = codec::mpeg4_aac_codecs(&aac);
            return 0;
        }
        case FLV_AUDIO_MP3: return -1;
        case FLV_AUDIO_OPUS: return -1;
        default: return -1;
    }
}

bool
audio_meta_t::operator==(const audio_meta_t &x) const {
    return codecid == x.codecid && bitrate == x.bitrate && datarate == x.datarate && samplerate == x.samplerate
        && samplesize == x.samplesize && channel_count == x.channel_count && stereo == x.stereo
        && memcmp(&aac, &x.aac, sizeof(mpeg4_aac_t)) == 0;
}

bool
audio_meta_t::operator!=(const audio_meta_t &x) const {
    return !(*this == x);
}

int
audio_meta_t::load(int32_t cid, const uint8_t *data, int bytes) {
    int rt = _load(cid, data, bytes);
    if (rt >= 0) _valid = true;

    return rt;
}

int
audio_meta_t::save(uint8_t *data, int bytes) const {
    if (!is_enabled()) return -1;
    switch (codecid) {
        case FLV_AUDIO_AAC:
        case FLV_AUDIO_ASC: return ::mpeg4_aac_audio_specific_config_save(&aac, data, bytes);
        case FLV_AUDIO_MP3: return -1;
        case FLV_AUDIO_OPUS: return -1;
        default: return -1;
    }
}

temporary_buffer<uint8_t>
audio_meta_t::to_tag_data() const {
    if (!is_enabled()) return temporary_buffer<uint8_t>();

    // audio tag
    flv_audio_tag_header_t ath;
    ath.codecid = codecid;
    ath.avpacket = FLV_SEQUENCE_HEADER;
    ath.rate = flv_rate_index_from_samplerate(samplerate);
    ath.bits = flv_bits_index_from_samplesize(samplesize);
    ath.channels = stereo ? 1 : 0;

    std::vector<uint8_t> athb(FLV_AUDIO_TAG_HEADER_SIZE);
    int aths = ::flv_audio_tag_header_write(&ath, athb.data(), FLV_AUDIO_TAG_HEADER_SIZE);
    assert(aths > 0);

    uint8_t header[4096];
    int header_len = save(header, 4096);
    if (header_len < 0) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> result(aths + header_len);

    std::copy_n(athb.data(), aths, result.get_write());
    std::copy_n(header, header_len, result.get_write() + aths);

    return result;
}

sstring
audio_meta_t::to_string() const {
    return fmt::format(
        "{} valid: {} codecid: {} bitrate: {} datarate: {} samplerate: {} samplesize: {} channel_count: {} stereo: {} "
        "aac: {}",
        seastar::pretty_type_name(typeid(flv::audio_meta_t)),
        _valid,
        codecid,
        bitrate,
        datarate,
        samplerate,
        samplesize,
        channel_count,
        stereo,
        aac);
}

video_meta_t::video_meta_t() {
    memset(&v, 0, sizeof(v));
}

video_meta_t::video_meta_t(const video_meta_t &x)
: meta_t(x.codecid, x.bitrate, x.datarate)
, framerate(x.framerate)
, width(x.width)
, height(x.height)
, encoder(x.encoder) {
    memset(&v, 0, sizeof(v));

    _valid = x._valid;

    switch (codecid) {
        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: codec::copy(&x.v.avc, &v.avc); break;
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: codec::copy(&x.v.hevc, &v.hevc); break;
        case FLV_VIDEO_AV1:
        case FLV_VIDEO_AV1C: codec::copy(&x.v.av1, &v.av1); break;
        default: break;
    }
}

video_meta_t &
video_meta_t::operator=(const video_meta_t &x) {
    codecid = x.codecid;
    bitrate = x.bitrate;
    datarate = x.datarate;

    framerate = x.framerate;
    width = x.width;
    height = x.height;
    encoder = x.encoder;

    _valid = x._valid;

    switch (codecid) {
        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: codec::copy(&x.v.avc, &v.avc); break;
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: codec::copy(&x.v.hevc, &v.hevc); break;
        case FLV_VIDEO_AV1:
        case FLV_VIDEO_AV1C: codec::copy(&x.v.av1, &v.av1); break;
        default: break;
    }

    return *this;
}

bool
video_meta_t::operator==(const video_meta_t &x) const {
    return codecid == x.codecid && bitrate == x.bitrate && datarate == x.datarate && framerate == x.framerate
        && width == x.width && height == x.height && encoder == x.encoder && memcmp(&v, &x.v, sizeof(v)) == 0;
}

bool
video_meta_t::operator!=(const video_meta_t &x) const {
    return !(*this == x);
}

int
video_meta_t::_load(int32_t cid, const uint8_t *data, int bytes) {
    codecid = cid;
    if (bytes <= 0) return -1;

    switch (cid) {
        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: {
            auto rt = ::mpeg4_avc_decoder_configuration_record_load(data, bytes, &v.avc);
            if (rt < 0) return rt;

            rt = codec::mpeg4_avc_rect_load(&v.avc, nullptr, nullptr, &width, &height);
            if (rt < 0) return rt;

            codec = codec::mpeg4_avc_codecs(&v.avc);
            return 0;
        }
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: {
            auto rt = ::mpeg4_hevc_decoder_configuration_record_load(data, bytes, &v.hevc);
            if (rt < 0) return rt;

            rt = codec::mpeg4_hevc_rect_load(&v.hevc, nullptr, nullptr, &width, &height);
            if (rt < 0) return rt;

            codec = codec::mpeg4_hevc_codecs(&v.hevc);
            return 0;
        }
        case FLV_VIDEO_AV1:
        case FLV_VIDEO_AV1C: {
            int rt = ::aom_av1_codec_configuration_record_load(data, bytes, &v.av1);
            if (rt < 0) return rt;

            width = v.av1.width;
            height = v.av1.height;
            codec = codec::aom_av1_codecs(&v.av1);
            return 0;
        }
        default: return -1;
    }
}

int
video_meta_t::load(int32_t cid, const uint8_t *data, int bytes) {
    int rt = _load(cid, data, bytes);
    if (rt >= 0) _valid = true;

    return rt;
}

int
video_meta_t::save(uint8_t *data, int bytes) const {
    if (!is_enabled()) return -1;

    switch (codecid) {
        case FLV_VIDEO_H264:
        case FLV_VIDEO_AVCC: {
            assert(bytes >= 7);
            if (bytes < 7) return -1;
            return ::mpeg4_avc_decoder_configuration_record_save(&v.avc, data, bytes);
        }
        case FLV_VIDEO_H265:
        case FLV_VIDEO_HVCC: {
            assert(bytes >= 23);
            if (bytes < 4) return -1;
            return ::mpeg4_hevc_decoder_configuration_record_save(&v.hevc, data, bytes);
        }
        case FLV_VIDEO_AV1:
        case FLV_VIDEO_AV1C: {
            assert(bytes >= 4);
            if (bytes <= 0) return -1;
            return ::aom_av1_codec_configuration_record_save(&v.av1, data, bytes);
        }
        default: return -1;
    }
}

temporary_buffer<uint8_t>
video_meta_t::to_tag_data() const {
    if (!is_enabled()) return temporary_buffer<uint8_t>();

    // video tag
    flv_video_tag_header_t vth;
    vth.codecid = codecid;
    vth.avpacket = FLV_SEQUENCE_HEADER;
    vth.keyframe = 1;
    vth.cts = 0;

    std::vector<uint8_t> vthb(FLV_VIDEO_TAG_HEADER_SIZE);
    int vths = ::flv_video_tag_header_write(&vth, vthb.data(), FLV_VIDEO_TAG_HEADER_SIZE);
    assert(vths > 0);

    uint8_t header[4096];
    int header_len = save(header, 4096);
    if (header_len < 0) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> result(vths + header_len);

    std::copy_n(vthb.data(), vths, result.get_write());
    std::copy_n(header, header_len, result.get_write() + vths);

    return result;
}

sstring
video_meta_t::to_string() const {
    auto v_to_string = [this] {
        if (FLV_VIDEO_H264 == codecid) {
            return sstring(fmt::format("{}", v.avc));
        } else if (FLV_VIDEO_H265 == codecid) {
            return sstring(fmt::format("{}", v.hevc));
        } else if (FLV_VIDEO_AV1 == codecid) {
            return sstring(fmt::format("{}", v.av1));
        } else {
            return sstring("");
        }
    };

    return fmt::format(
        "{} valid: {} codecid: {} bitrate: {} datarate: {} framerate: {} width: {} height: {} encoder: {} v: {}",
        seastar::pretty_type_name(typeid(flv::video_meta_t)),
        _valid,
        codecid,
        bitrate,
        datarate,
        framerate,
        width,
        height,
        encoder,
        v_to_string());
}

metadata_t::metadata_t(const metadata_t &x)
: video(x.video)
, audio(x.audio) {}

metadata_t::metadata_t(const uint8_t *data, size_t len) {
    load(data, len);
}


metadata_t::metadata_t(const metadata_t &x, media_type_t media_type) {
    if ((media_type & media_type_t::video) != media_type_t::none) video = x.video;
    if ((media_type & media_type_t::audio) != media_type_t::none) audio = x.audio;
}

metadata_t &
metadata_t::operator=(const metadata_t &x) {
    video = x.video;
    audio = x.audio;

    return *this;
}

bool
metadata_t::operator==(const metadata_t &x) const {
    return video == x.video && audio == x.audio;
}

bool
metadata_t::operator!=(const metadata_t &x) const {
    return !(*this == x);
}

int
metadata_t::load(const uint8_t *data, size_t len) {
    const uint8_t *end = data + len;

    char encoder[256] = {0};
    char creationdate[64] = {0};
    double audiocodecid = 0;
    double audiodatarate = 0; // bitrate / 1024
    double audiodelay = 0;
    double audiosamplerate = 0;
    double audiosamplesize = 0;
    double videocodecid = 0;
    double videodatarate = 0; // bitrate / 1024
    double framerate = 0;
    double height = 0;
    double width = 0;
    double duration = 0;
    double filesize = 0;
    double fps = 0;
    int canSeekToEnd = 0;
    int stereo = 0;
    struct amf_object_item_t keyframes[2];
    struct amf_object_item_t prop[18];
    struct amf_object_item_t items[1];

#define AMF_OBJECT_ITEM_VALUE(v, amf_type, amf_name, amf_value, amf_size) \
    {                                                                     \
        v.type = amf_type;                                                \
        v.name = amf_name;                                                \
        v.value = amf_value;                                              \
        v.size = amf_size;                                                \
    }
    AMF_OBJECT_ITEM_VALUE(keyframes[0], AMF_STRICT_ARRAY, "filepositions", NULL,
                          0); // ignore keyframes
    AMF_OBJECT_ITEM_VALUE(keyframes[1], AMF_STRICT_ARRAY, "times", NULL, 0);

    AMF_OBJECT_ITEM_VALUE(prop[0], AMF_NUMBER, "audiocodecid", &audiocodecid, sizeof(audiocodecid));
    AMF_OBJECT_ITEM_VALUE(prop[1], AMF_NUMBER, "audiodatarate", &audiodatarate, sizeof(audiodatarate));
    AMF_OBJECT_ITEM_VALUE(prop[2], AMF_NUMBER, "audiodelay", &audiodelay, sizeof(audiodelay));
    AMF_OBJECT_ITEM_VALUE(prop[3], AMF_NUMBER, "audiosamplerate", &audiosamplerate, sizeof(audiosamplerate));
    AMF_OBJECT_ITEM_VALUE(prop[4], AMF_NUMBER, "audiosamplesize", &audiosamplesize, sizeof(audiosamplesize));
    AMF_OBJECT_ITEM_VALUE(prop[5], AMF_BOOLEAN, "stereo", &stereo, sizeof(stereo));

    AMF_OBJECT_ITEM_VALUE(prop[6], AMF_BOOLEAN, "canSeekToEnd", &canSeekToEnd, sizeof(canSeekToEnd));
    AMF_OBJECT_ITEM_VALUE(prop[7], AMF_STRING, "creationdate", creationdate, sizeof(creationdate));
    AMF_OBJECT_ITEM_VALUE(prop[8], AMF_NUMBER, "duration", &duration, sizeof(duration));
    AMF_OBJECT_ITEM_VALUE(prop[9], AMF_NUMBER, "filesize", &filesize, sizeof(filesize));

    AMF_OBJECT_ITEM_VALUE(prop[10], AMF_NUMBER, "videocodecid", &videocodecid, sizeof(videocodecid));
    AMF_OBJECT_ITEM_VALUE(prop[11], AMF_NUMBER, "videodatarate", &videodatarate, sizeof(videodatarate));
    AMF_OBJECT_ITEM_VALUE(prop[12], AMF_NUMBER, "framerate", &framerate, sizeof(framerate));
    AMF_OBJECT_ITEM_VALUE(prop[13], AMF_NUMBER, "height", &height, sizeof(height));
    AMF_OBJECT_ITEM_VALUE(prop[14], AMF_NUMBER, "width", &width, sizeof(width));

    AMF_OBJECT_ITEM_VALUE(prop[15], AMF_OBJECT, "keyframes", keyframes, 2); // FLV I-index
    AMF_OBJECT_ITEM_VALUE(prop[16], AMF_STRING, "encoder", encoder, sizeof(encoder));
    AMF_OBJECT_ITEM_VALUE(prop[17], AMF_NUMBER, "fps", &fps, sizeof(fps));

    AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "onMetaData", prop, sizeof(prop) / sizeof(prop[0]));
#undef AMF_OBJECT_ITEM_VALUE

    if (amf_read_items(data, end, items, sizeof(items) / sizeof(items[0])) == nullptr) {
        assert(0);
        return EINVAL;
    }

    // compress encoder into 32 bytes
    char *name = encoder;
    char *psz;

    if (strncmp(encoder, "obs-output module ", 18) == 0) {
        name += 18;
    } else if (strncmp(encoder, "Open Broadcaster Software", 25) == 0) {
        name += 25 - 3;
        memcpy(name, "OBS", 3);
    } else if ((psz = strstr(encoder, " (Windows)")) != nullptr) {
        if (psz != name) *psz = '\0';
    }

    if (audiocodecid > 0) {
        audio.codecid = ((int32_t)audiocodecid << 4);
        audio.datarate = audiodatarate;
        audio.bitrate = audiodatarate * 1000;
        audio.samplesize = audiosamplesize;
        audio.samplerate = audiosamplerate;
        audio.stereo = stereo > 0;
    } else {
        audio = audio_meta_t();
    }

    if (videocodecid > 0) {
        video.framerate = framerate;
        if (fps > 0) video.framerate = fps;

        video.codecid = videocodecid;
        video.datarate = videodatarate;
        video.bitrate = videodatarate * 1000;
        video.width = width;
        video.height = height;
        video.encoder = name;
    } else {
        video = video_meta_t();
    }

    return 0;
}

temporary_buffer<uint8_t>
metadata_t::to_tag_data() const {
    if (!audio.is_enabled() && !video.is_enabled()) return temporary_buffer<uint8_t>();

    std::vector<uint8_t> buf(1024);

    uint8_t *begin = buf.data();
    uint8_t *end = begin + 1024;
    uint8_t *ptr = begin;
    uint32_t count = (audio.is_enabled() ? 5 : 0) + (video.is_enabled() ? 9 : 0);

    ptr = AMFWriteString(ptr, end, "onMetaData", 10);
    ptr[0] = AMF_ECMA_ARRAY;
    ptr[1] = (uint8_t)((count >> 24) & 0xFF);
    ptr[2] = (uint8_t)((count >> 16) & 0xFF);
    ptr[3] = (uint8_t)((count >> 8) & 0xFF);
    ptr[4] = (uint8_t)(count & 0xFF);
    ptr += 5;

    if (audio.is_enabled()) {
        int code_num = audio.codecid >> 4;

        ptr = AMFWriteNamedDouble(ptr, end, "audiocodecid", 12, code_num);
        ptr = AMFWriteNamedDouble(ptr, end, "audiodatarate", 13, audio.datarate);
        ptr = AMFWriteNamedDouble(ptr, end, "audiosamplerate", 15, audio.samplerate);
        ptr = AMFWriteNamedDouble(ptr, end, "audiosamplesize", 15, audio.samplesize);
        ptr = AMFWriteNamedBoolean(ptr, end, "stereo", 6, audio.stereo);
    }
    if (video.is_enabled()) {
        ptr = AMFWriteNamedDouble(ptr, end, "videocodecid", 12, video.codecid);
        ptr = AMFWriteNamedDouble(ptr, end, "videodatarate", 13, video.datarate);
        ptr = AMFWriteNamedDouble(ptr, end, "framerate", 9, video.framerate);
        ptr = AMFWriteNamedDouble(ptr, end, "height", 6, video.height);
        ptr = AMFWriteNamedDouble(ptr, end, "width", 5, video.width);
        ptr = AMFWriteNamedDouble(ptr, end, "displayWidth", 12, video.width);
        ptr = AMFWriteNamedDouble(ptr, end, "displayHeight", 13, video.height);
        ptr = AMFWriteNamedDouble(ptr, end, "fps", 3, video.framerate);
        ptr = AMFWriteNamedString(ptr, end, "encoder", 7, video.encoder.c_str(), video.encoder.size());
    }
    ptr = AMFWriteObjectEnd(ptr, end);

    int len = ptr - begin;
    buf.erase(buf.begin() + len, buf.end());

    temporary_buffer<uint8_t> result(len);
    std::copy_n(buf.data(), len, result.get_write());

    return result;
}

bool
metadata_t::is_enabled() const {
    return audio.is_enabled() || video.is_enabled();
}

bool
metadata_t::is_enabled(media_type_t type) const {
    auto is_video = (type & media_type_t::video) != media_type_t::none;
    if (is_video && !video.is_enabled()) return false;

    auto is_audio = (type & media_type_t::audio) != media_type_t::none;
    if (is_audio && !audio.is_enabled()) return false;

    return true;
}

bool
metadata_t::is_video_frame(std::shared_ptr<media_t> media) {
    return media->is_video();
}

media_type_t
metadata_t::media_options() const {
    media_type_t options = media_type_t::none;
    if (video.is_enabled()) options |= media_type_t::video;
    if (audio.is_enabled()) options |= media_type_t::audio;
    return options;
}

sstring
metadata_t::to_string() const {
    return fmt::format("{} video: {} audio: {}", seastar::pretty_type_name(typeid(flv::metadata_t)), video, audio);
}

int
rtmp_sample_access_t::decode(const uint8_t *data, size_t len) {
    const uint8_t *ptr = data;
    const uint8_t *end = data + len;

    uint8_t _arg1 = 0;
    uint8_t _arg2 = 0;

    ptr = AMFReadBoolean(ptr, end, &_arg1);
    if (!ptr) return -1;

    ptr = AMFReadBoolean(ptr, end, &_arg2);
    if (!ptr) return -1;

    arg1 = _arg1;
    arg2 = _arg2;

    return ptr - data;
}

sstring
parse_command_name(const uint8_t *data, size_t len) {
    sstring str(64, '\0');
    size_t str_len = 64;

    auto rt = parse_command_name(data, len, str.data(), &str_len);
    if (rt < 0) return "";

    str.resize(str_len);

    return str;
}

amf_command
command_from_name(const sstring &name) {
    if (name == "onMetaData") {
        return amf_command::meta_data;
    } else if (name == "onTextData") {
        return amf_command::text_data;
    } else if (name == "onCaption") {
        return amf_command::caption;
    } else if (name == "onCaptionInfo") {
        return amf_command::caption_info;
    } else if (name == "|RtmpSampleAccess") {
        return amf_command::rtmp_sample_access;
    } else {
        return amf_command::unknown;
    }
}

int
parse_command_name(const uint8_t *data, size_t len, char *out, size_t *out_len) {
    if (AMF_STRING != data[0]) return -1;

    auto tail = AMFReadString(data + 1, data + len, 0, out, *out_len);
    if (!tail) return -1;

    auto n = tail - data;
    *out_len = n - 2 - 1;

    return n;
}

extern int
parse_amf_command(const uint8_t *data, size_t len, amf_command *cmd) {
    sstring str(64, '\0');
    size_t str_len = 64;

    auto rt = parse_command_name(data, len, str.data(), &str_len);
    if (rt < 0) return rt;

    str.resize(str_len);

    *cmd = command_from_name(str);

    return rt;
}

script_t::script_t(temporary_buffer<uint8_t> buf)
: cmd_name(parse_command_name(buf.get(), buf.size()))
, data(std::move(buf)) {}

} // namespace flv
} // namespace amadeus
