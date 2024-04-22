#include <aom-av1.h>
#include <bitstream.h>
#include <flv-proto.h>
#include <h264-sps.h>
#include <h265-sps.h>
#include <h265-vps.h>
#include <mpeg4-aac.h>
#include <mpeg4-avc.h>
#include <mpeg4-hevc.h>

#include "util/codec.hh"

namespace amadeus {
namespace codec {

using namespace seastar;

#define H265_NAL_VPS 32
#define H265_NAL_SPS 33
#define H265_NAL_PPS 34
#define H265_NAL_AUD 35

sstring
mpeg4_aac_codecs(const mpeg4_aac_t *aac) {
    // https://tools.ietf.org/html/rfc6381#section-3.3
    // https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter
    return fmt::format("mp4a.40.{}", aac->profile);
}

sstring
mpeg4_avc_codecs(const mpeg4_avc_t *avc) {
    char buf[1024];
    auto rt = ::mpeg4_avc_codecs(avc, buf, 1024);
    if (rt < 0) return "";

    return sstring(buf, rt);
}

sstring
mpeg4_hevc_codecs(const mpeg4_hevc_t *hevc) {
    char buf[1024];
    auto rt = ::mpeg4_hevc_codecs(hevc, buf, 1024);
    if (rt < 0) return "";

    return sstring(buf, rt);
}

sstring
aom_av1_codecs(const aom_av1_t *av1) {
    char buf[1024];
    auto rt = ::aom_av1_codecs(av1, buf, 1024);
    if (rt < 0) return "";

    return sstring(buf, rt);
}

int
mpeg4_avc_rect_load(struct mpeg4_avc_t *avc, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height) {
    if (avc->nb_sps == 0) return -1;

    ::bitstream_t stream;
    ::bitstream_init(&stream, avc->sps[0].data + 1, avc->sps[0].bytes - 1); // skip 1 byte nal

    struct ::h264_sps_t sps;
    auto rt = ::h264_sps(&stream, &sps);
    if (rt < 0) return rt;

    int _x = 0, _y = 0, _w = 0, _h = 0;
    rt = ::h264_display_rect(&sps, &_x, &_y, &_w, &_h);
    if (rt == 0) {
        if (x) *x = (uint32_t)_x;
        if (y) *y = (uint32_t)_y;
        if (width) *width = (uint32_t)_w;
        if (height) *height = (uint32_t)_h;
    }

    return rt;
}

int
mpeg4_hevc_rect_load(struct mpeg4_hevc_t *hevc, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height) {
    for (int i = 0; i < static_cast<int>(hevc->numOfArrays); i++) {
        if (hevc->nalu[i].type != H265_NAL_SPS) continue;

        ::bitstream_t stream;
        ::bitstream_init(&stream, hevc->nalu[i].data, hevc->nalu[i].bytes);

        struct ::h265_sps_t sps;
        auto rt = ::h265_sps(&stream, &sps);
        if (rt < 0) continue;

        int _x = 0, _y = 0, _w = 0, _h = 0;
        rt = ::h265_display_rect(&sps, &_x, &_y, &_w, &_h);
        if (rt < 0) continue;

        if (x) *x = (uint32_t)_x;
        if (y) *y = (uint32_t)_y;
        if (width) *width = (uint32_t)_w;
        if (height) *height = (uint32_t)_h;

        break;
    }
    return 0;
}

void
copy(const mpeg4_avc_t *from, mpeg4_avc_t *to) {
    memcpy(to, from, sizeof(mpeg4_avc_t));

    for (size_t i = 0; i < from->nb_sps; i++) {
        size_t offset = from->sps[i].data - from->data;

        to->sps[i].data = to->data + offset;
    }

    for (size_t i = 0; i < from->nb_pps; i++) {
        size_t offset = from->pps[i].data - from->data;

        to->pps[i].data = to->data + offset;
    }
}

void
copy(const mpeg4_hevc_t *from, mpeg4_hevc_t *to) {
    memcpy(to, from, sizeof(mpeg4_hevc_t));

    for (size_t i = 0; i < from->numOfArrays; i++) {
        size_t offset = from->nalu[i].data - from->data;

        to->nalu[i].data = to->data + offset;
    }
}

void
copy(const aom_av1_t *from, aom_av1_t *to) {
    memcpy(to, from, sizeof(aom_av1_t));
}

} // namespace codec

} // namespace amadeus

std::ostream &
operator<<(std::ostream &os, const mpeg4_aac_t *v) {
    os << "mpeg4_aac_t{";
    os << " profile: " << v->profile;
    os << " sampling_frequency_index: " << v->sampling_frequency_index;
    os << " channel_configuration: " << v->channel_configuration;
    os << " extension_frequency: " << v->extension_frequency;
    os << " sampling_frequency: " << v->sampling_frequency;
    os << " channels: " << v->channels;
    os << " sbr: " << v->sbr;
    os << " ps: " << v->ps;
    os << " npce: " << v->npce;
    os << "}";

    return os;
}

std::ostream &
operator<<(std::ostream &os, const aom_av1_t *v) {
    os << "aom_av1_t{";
    os << " marker: " << v->marker;
    os << " version: " << v->version;
    os << " seq_profile: " << v->seq_profile;
    os << " seq_level_idx_0: " << v->seq_level_idx_0;
    os << " seq_tier_0: " << v->seq_tier_0;
    os << " high_bitdepth: " << v->high_bitdepth;
    os << " twelve_bit: " << v->twelve_bit;
    os << " monochrome: " << v->monochrome;
    os << " chroma_subsampling_x: " << v->chroma_subsampling_x;
    os << " chroma_subsampling_y: " << v->chroma_subsampling_y;
    os << " chroma_sample_position: " << v->chroma_sample_position;
    os << " reserved: " << v->reserved;
    os << " initial_presentation_delay_present: " << v->initial_presentation_delay_present;
    os << " initial_presentation_delay_minus_one: " << v->initial_presentation_delay_minus_one;
    os << " buffer_delay_length_minus_1: " << v->buffer_delay_length_minus_1;
    os << " width: " << v->width;
    os << " height: " << v->height;
    os << " bytes: " << v->bytes;
    os << "}";

    return os;
}

std::ostream &
operator<<(std::ostream &os, const mpeg4_avc_t *v) {
    os << "mpeg4_avc_t{";
    os << " profile: " << v->profile;
    os << " compatibility: " << v->compatibility;
    os << " level: " << v->level;
    os << " nalu: " << v->nalu;
    os << " nb_sps: " << v->nb_sps;
    os << " nb_pps: " << v->nb_pps;
    os << " chroma_format_idc: " << v->chroma_format_idc;
    os << " bit_depth_luma_minus8: " << v->bit_depth_luma_minus8;
    os << " bit_depth_chroma_minus8: " << v->bit_depth_chroma_minus8;
    os << " off: " << v->off;
    os << "}";

    return os;
}

std::ostream &
operator<<(std::ostream &os, const mpeg4_hevc_t *v) {
    os << "mpeg4_hevc_t{";
    os << " configurationVersion: " << v->configurationVersion;
    os << " general_profile_space: " << v->general_profile_space;
    os << " general_tier_flag: " << v->general_tier_flag;
    os << " general_profile_idc: " << v->general_profile_idc;
    os << " general_profile_compatibility_flags: " << v->general_profile_compatibility_flags;
    os << " general_constraint_indicator_flags: " << v->general_constraint_indicator_flags;
    os << " general_level_idc: " << v->general_level_idc;
    os << " min_spatial_segmentation_idc: " << v->min_spatial_segmentation_idc;
    os << " parallelismType: " << v->parallelismType;
    os << " chromaFormat: " << v->chromaFormat;
    os << " bitDepthLumaMinus8: " << v->bitDepthLumaMinus8;
    os << " bitDepthChromaMinus8: " << v->bitDepthChromaMinus8;
    os << " avgFrameRate: " << v->avgFrameRate;
    os << " constantFrameRate: " << v->constantFrameRate;
    os << " numTemporalLayers: " << v->numTemporalLayers;
    os << " temporalIdNested: " << v->temporalIdNested;
    os << " lengthSizeMinusOne: " << v->lengthSizeMinusOne;
    os << " numOfArrays: " << v->numOfArrays;
    os << " array_completeness: " << v->array_completeness;
    os << " off: " << v->off;
    os << "}";

    return os;
}
