#ifndef _fmp4_writer_h_
#define _fmp4_writer_h_

#include "mov-atom.h"
#include "mov-buffer.h"
#include "mov-reader.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmp4_writer_t fmp4_writer_t;

typedef struct dv_config_s {
    uint8_t dv_version_major;
    uint8_t dv_version_minor;
    uint8_t dv_profile;
    uint8_t dv_level;
    uint8_t rpu_present_flag;
    uint8_t el_present_flag;
    uint8_t bl_present_flag;
    uint16_t dependecy_pid;
    uint8_t dv_bl_signal_compatibility_id;
} dv_config_t;

typedef struct drm_info_s {
    int is_drm;
    int default_kid_len;
    int default_iv_len;
    unsigned char kid[16];
    unsigned char iv[16];
    int scheme_type;
    int drm_type;
    // widevine pssh box
    unsigned char pssh_data[256];
    size_t pssh_data_len;
} drm_info_t;

#ifndef _BILI_FIXED_
#define _BILI_FIXED_
#endif
/// @param[in] flags mov flags, such as: MOV_FLAG_SEGMENT, see more
/// @mov-format.h
#ifdef _BILI_FIXED_
fmp4_writer_t* fmp4_writer_create(int flags);
void fmp4_writer_set_compressor_name(fmp4_writer_t* fmp4, const char* name);
#else
fmp4_writer_t* fmp4_writer_create(const struct mov_buffer_t* buffer, void* param, int flags);
#endif
void fmp4_writer_destroy(fmp4_writer_t* fmp4);

/// @param[in] object MPEG-4 systems ObjectTypeIndication such as:
/// MOV_OBJECT_H264, see more @mov-format.h
/// @param[in] extra_data
/// AudioSpecificConfig/AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord
/// @return >=0-track, <0-error
#ifdef _BILI_FIXED_
int fmp4_writer_add_audio(
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size);
int fmp4_writer_add_audio2(  // for multi-track
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size);
int fmp4_writer_add_drm_audio(
    fmp4_writer_t* fmp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, int sample_duration,
    int bitrate, drm_info_t* drm, const void* extra_data, size_t extra_data_size);
int fmp4_writer_update_audio(
    struct fmp4_writer_t* writer, int track_index, uint8_t object, int channel_count, int bits_per_sample,
    int sample_rate, int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size);
#else
int fmp4_writer_add_audio(
    fmp4_writer_t* fmp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    const void* extra_data, size_t extra_data_size);
#endif
int fmp4_writer_add_video(
    fmp4_writer_t* fmp4, uint8_t object, int width, int height, int bitrate, double fps, const void* extra_data,
    size_t extra_data_size);
int fmp4_writer_add_drm_video(
    fmp4_writer_t* fmp4, uint8_t object, int width, int height, int bitrate, double fps, drm_info_t* drm,
    const void* extra_data, size_t extra_data_size);
int fmp4_writer_add_subtitle(fmp4_writer_t* fmp4, uint8_t object, const void* extra_data, size_t extra_data_size);

int fmp4_writer_add_udta( struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes);

#ifdef _BILI_FIXED_
int fmp4_writer_clear_tracks(fmp4_writer_t* fmp4);
#endif

/// Write audio/video stream
/// raw AAC data, don't include ADTS/AudioSpecificConfig
/// H.264/H.265 MP4 format, replace start code(0x00000001) with NALU size
/// @param[in] track return by mov_writer_add_audio/mov_writer_add_video
/// @param[in] data audio/video frame
/// @param[in] bytes buffer size
/// @param[in] pts timestamp in millisecond
/// @param[in] dts timestamp in millisecond
/// @param[in] flags MOV_AV_FLAG_XXX, such as: MOV_AV_FLAG_KEYFREAME, see more
/// @mov-format.h
/// @return 0-ok, other-error
int fmp4_writer_write(
    fmp4_writer_t* fmp4, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

/// Write drm audio/video stream
/// raw AAC data, don't include ADTS/AudioSpecificConfig
/// H.264/H.265 MP4 format, replace start code(0x00000001) with NALU size
/// @param[in] track return by mov_writer_add_audio/mov_writer_add_video
/// @param[in] data audio/video frame
/// @param[in] bytes buffer size
/// @param[in] pts timestamp in millisecond
/// @param[in] dts timestamp in millisecond
/// @param[in] flags MOV_AV_FLAG_XXX, such as: MOV_AV_FLAG_KEYFREAME, see more
/// @param[in] senc_info senc info for subsample
/// @param[in] subsample_count subsample for per frame, see more
/// @mov-format.h
/// @return 0-ok, other-error
int fmp4_writer_drm_write(
    fmp4_writer_t* fmp4, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags,
    void* senc_info, uint32_t subsample_count);

#ifdef _BILI_FIXED_
size_t fmp4_writer_samples_capacity(fmp4_writer_t* fmp4);
size_t fmp4_writer_segment_size(fmp4_writer_t* fmp4);
#endif

/// Save data and open next segment
/// @return 0-ok, other-error
#ifdef _BILI_FIXED_
int fmp4_writer_save_segment(
    fmp4_writer_t* fmp4, const struct mov_buffer_t* buffer, void* param, int64_t next_video_dts, uint32_t fragment_id);
#else
int fmp4_writer_save_segment(fmp4_writer_t* fmp4, uint32_t fragment_id);
#endif

/// Get init segment data(write FTYP, MOOV only)
/// WARNING: it caller duty to switch file/buffer context with fmp4_writer_write
/// @return 0-ok, other-error
#ifdef _BILI_FIXED_
int fmp4_writer_init_segment(fmp4_writer_t* fmp4, const struct mov_buffer_t* buffer, void* param);
#else
int fmp4_writer_init_segment(fmp4_writer_t* fmp4);
#endif

int fmp4_extra_data_copy(struct fmp4_writer_t* writer, struct mov_reader_t* reader, int flag);

int fmp4_writer_dv_config(fmp4_writer_t* fmp4, struct dovi_config_t config);
int fmp4_writer_eac3_config(fmp4_writer_t* fmp4, struct eac3_config_t config, int codecid);

void fmp4_writer_reset_track(fmp4_writer_t* fmp4, int idx);

#ifdef __cplusplus
}
#endif
#endif /* !_fmp4_writer_h_ */
