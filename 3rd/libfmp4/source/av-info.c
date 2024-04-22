#include "av-info.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "bitstream.h"
#include "eac3-header.h"
#include "h264-sps.h"
#include "h265-nal.h"
#include "h265-sps.h"
#include "mp3-header.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"

#define H264_NAL_RASL_R 9
#define H265_NAL_BLA_W_LP 16
#define H265_NAL_BLA_W_RADL 17
#define H265_NAL_BLA_N_LP 18
#define H265_NAL_IDR 19
#define H265_NAL_IDR_N_LP 20
#define H265_NAL_CRA_NUT 21
#define H265_NAL_VPS 32
#define H265_NAL_SPS 33
#define H265_NAL_PPS 34
#define H265_NAL_AUD 35

int
h265_annex_keyframe(const void *data, size_t bytes) {
    char *p = (char *)data;
    int remain = bytes;
    int nal_type;

    while (remain > 0) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            p += 4;
            nal_type = (*p & 0x7E) >> 1;
            // printf("1nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            if (nal_type == H265_NAL_IDR || nal_type == H265_NAL_IDR_N_LP || nal_type == H265_NAL_CRA_NUT
                || nal_type == H265_NAL_BLA_W_LP || nal_type == H265_NAL_BLA_W_RADL || nal_type == H265_NAL_BLA_N_LP) {
                return 0;
            } else if (nal_type >= 0 && nal_type <= H264_NAL_RASL_R) {
                return -1;
            }
        } else if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
            p += 3;
            nal_type = (*p & 0x7E) >> 1;
            // printf("2nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            if (nal_type == H265_NAL_IDR || nal_type == H265_NAL_IDR_N_LP || nal_type == H265_NAL_CRA_NUT
                || nal_type == H265_NAL_BLA_W_LP || nal_type == H265_NAL_BLA_W_RADL || nal_type == H265_NAL_BLA_N_LP) {
                return 0;
            } else if (nal_type >= 0 && nal_type <= H264_NAL_RASL_R) {
                return -1;
            }
        } else {
            p += 1;
        }
        remain = ((int)bytes - (p - (char *)data));
    }
    return -1;
}

#define H264_NAL_SLIACE_NONE 1
#define H264_NAL_SLIACE_A    2
#define H264_NAL_SLIACE_B    3
#define H264_NAL_SLIACE_C    4
#define H264_NAL_IDR         5 // Coded slice of an IDR picture
#define H264_NAL_SPS         7 // Sequence parameter set
#define H264_NAL_PPS         8 // Picture parameter set
#define H264_NAL_AUD         9 // Access unit delimiter

int
get_avc_mpeg4_decorder_configration(const void *data, size_t bytes, uint8_t *buf, size_t len) {
    char *p = (char *)data;
    if (bytes < 5) { return -1; }
    int nal_type = 0;
    int res = -1;

    int remain = bytes;
    char *pps_start = NULL;
    char *pps_end = NULL;

    while (remain > 0) {
        if (pps_start != NULL && remain == 1) {
            pps_end = (char *)data + bytes;
            res = 0;
            break;
        }
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            p += 4;
            nal_type = *p & 0x1F;
            // printf("1nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            if (nal_type == H264_NAL_PPS) {
                pps_start = p;
            } else if (pps_start != NULL) {
                // printf("P=%X\n", *p);
                pps_end = p - 4;
                res = 0;
                break;
            }
        } else if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
            // printf("2nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            p += 3;
            nal_type = *p & 0x1F;
            if (nal_type == H264_NAL_PPS) {
                pps_start = p;
            } else if (pps_start != NULL) {
                // printf("P=%X\n", *p);
                pps_end = p - 3;
                res = 0;
                break;
            }
        } else {
            p += 1;
        }

        remain = ((int)bytes - (p - (char *)data));
    }

    if (res != 0) { return res; }

    struct mpeg4_avc_t avc;
    int vcl, update;
    memset(&avc, 0, sizeof(avc));
    int size = h264_annexbtomp4(&avc, data, pps_end - (char *)data, buf, len, &vcl, &update);
    assert(size > 0);
    return mpeg4_avc_decoder_configuration_record_save(&avc, buf, len);
}

int
h264_annex_keyframe(const void *data, size_t bytes) {
    char *p = (char *)data;
    int remain = bytes;
    int nal_type;

    while (remain > 0) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            p += 4;
            nal_type = *p & 0x1F;
            // printf("1nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            if (nal_type == H264_NAL_IDR) {
                return 0;
            } else if (
                nal_type == H264_NAL_SLIACE_NONE || nal_type == H264_NAL_SLIACE_A || nal_type == H264_NAL_SLIACE_B
                || nal_type == H264_NAL_SLIACE_C) {
                return -1;
            }
        } else if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
            p += 3;
            nal_type = *p & 0x1F;
            // printf("2nal_tpye=%d p=%p *p=%x\n", nal_type, p, *p);
            if (nal_type == H264_NAL_IDR) {
                return 0;
            } else if (
                nal_type == H264_NAL_SLIACE_NONE || nal_type == H264_NAL_SLIACE_A || nal_type == H264_NAL_SLIACE_B
                || nal_type == H264_NAL_SLIACE_C) {
                return -1;
            }
        } else {
            p += 1;
        }
        remain = ((int)bytes - (p - (char *)data));
    }
    return -1;
}

size_t
video_filter_nal(const void *data, size_t bytes, size_t nalu) {
    const uint8_t *begin = (const uint8_t *)data;
    const uint8_t *end = begin + bytes;
    const uint8_t *p = begin;
    while (p + nalu < end) {
        size_t nal_size = 0, i;
        for (i = 0; i < nalu; i++) nal_size = (nal_size << 8) | (*p++);

        const uint8_t *nal_end = p + nal_size;
        if (nal_end > end || nal_end <= p) break;

        uint8_t nal_type = *p & 0x1f;
        if (nal_type == 12 || nal_type == 9) {
            // T-REC-H.264-201704-I 7.4.2.7 (Page 84) Filler data RBSP
            // semantics The filler data RBSP contains zero or more bytes.
            // No normative decoding process is specified for a filler data
            // RBSP. drop the filler data
            memmove((void *)(p - nalu), nal_end, end - nal_end);
            end -= nalu + nal_size;
        }
        p = nal_end;
    }
    return end - begin;
}

int
parse_dv_config_descriptor(const uint8_t *data, size_t bytes, struct dovi_config_t *config) {
    if (bytes < 4) { return -1; }
    config->dv_version_major = data[0];
    config->dv_version_minor = data[1];
    config->dv_profile = data[2] >> 1;
    config->dv_level = ((data[2] & 0x01) << 5) | (data[3] >> 3);
    config->rpu_present_flag = data[3] >> 2 & 0x01;
    config->el_present_flag = data[3] >> 1 & 0x01;
    config->bl_present_flag = data[3] & 0x01;
    if (bytes > 4) {
        if (bytes == 5) {
            assert(config->bl_present_flag == 1);
            config->dv_bl_signal_compatibility_id = data[4] >> 4;
            return 0;
        } else if (bytes == 7) {
            assert(config->bl_present_flag == 0);
            if (!config->bl_present_flag) {
                config->dependecy_pid = (data[4] << 5) | (data[5] >> 3);
                config->dv_bl_signal_compatibility_id = data[6] >> 4;
                return 0;
            } else {
                return -1;
            }
        } else {
            assert(0);
            return -1;
        }
    }
    return 0;
}

// only support SYSTEM-A
int
parse_ac3_config_descriptpr(const uint8_t *data, size_t bytes, struct eac3_config_t *config) {
    if (bytes < 3) { return -1; }
    config->sample_rate_code = data[0] >> 5;
    config->bsid = data[0] & 0x1F;
    config->bit_rate_code = data[1] >> 2;
    config->surroud_mode = data[1] & 0x03;
    config->bsmod = data[2] >> 5;
    config->num_channels = (data[2] & 0x1E) >> 1;
    return 0;
}

int
parse_eac3_config_descriptpr(const uint8_t *data, size_t bytes, struct eac3_config_t *config) {
    // FIXME: Need to real parse
    config->asvc = (data[0] >> 4) & 0x01;
    config->num_ind_sub = 0;
    return 0;
}