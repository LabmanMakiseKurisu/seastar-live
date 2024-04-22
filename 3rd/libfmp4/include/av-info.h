#ifndef _video_size_h_
#define _video_size_h_

#include <stddef.h>
#include <stdint.h>

#include "mov-atom.h"

#if defined(__cplusplus)
extern "C" {
#endif

size_t video_filter_nal(const void *data, size_t bytes, size_t nalu);
int get_hevc_mpeg4_decorder_configration(const void *data, size_t bytes, uint8_t *buf, size_t len);
int get_avc_mpeg4_decorder_configration(const void *data, size_t bytes, uint8_t *buf, size_t len);

int h264_annex_keyframe(const void *data, size_t bytes);
int h265_annex_keyframe(const void *data, size_t bytes);

int parse_dv_config_descriptor(const uint8_t *data, size_t bytes, struct dovi_config_t *config);
int parse_ac3_config_descriptpr(const uint8_t *data, size_t bytes, struct eac3_config_t *config);
int parse_eac3_config_descriptpr(const uint8_t *data, size_t bytes, struct eac3_config_t *config);

#if defined(__cplusplus)
}
#endif
#endif /* !_video_size_h_ */