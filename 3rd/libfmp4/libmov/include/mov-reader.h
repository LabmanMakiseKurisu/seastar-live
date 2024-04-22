#ifndef _mov_reader_h_
#define _mov_reader_h_

#include <stddef.h>
#include <stdint.h>
#include "mov-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mov_reader_t mov_reader_t;

mov_reader_t* mov_reader_create(const struct mov_buffer_t* buffer, void* param);
void mov_reader_destroy(mov_reader_t* mov);

struct mov_reader_trackinfo_t {
    /// @param[in] object: MOV_OBJECT_H264/MOV_OBJECT_AAC, see more
    /// @mov-format.h
    void (*onvideo)(
        void* param, uint32_t track, uint8_t object, int width, int height,
        const void* extra, size_t bytes, int64_t bitrate);
    void (*onaudio)(
        void* param, uint32_t track, uint8_t object, int channel_count,
        int bit_per_sample, int sample_rate, const void* extra, size_t bytes,
        int64_t bitrate);
    void (*onsubtitle)(
        void* param, uint32_t track, uint8_t object, const void* extra,
        size_t bytes);
};

/// @return 0-OK, other-error
int mov_reader_getinfo(
    mov_reader_t* mov, struct mov_reader_trackinfo_t* ontrack, void* param);

uint64_t mov_reader_getduration(mov_reader_t* mov);

/// audio: AAC raw data, don't include ADTS/AudioSpecificConfig
/// video: 4-byte data length(don't include self length) + H.264 NALU(don't
/// include 0x00000001)
/// @param[in] flags MOV_AV_FLAG_xxx, such as: MOV_AV_FLAG_KEYFREAME
typedef void (*mov_reader_onread)(
    void* param, uint32_t track, uint8_t object, const void* buffer,
    size_t bytes, int64_t pts, int64_t dts, int flags);
/// @return 1-read one frame, 0-EOF, <0-error
int mov_reader_read(
    mov_reader_t* mov, void* buffer, size_t bytes, mov_reader_onread onread,
    void* param);

/// @param[in,out] timestamp input seek timestamp, output seek location
/// timestamp
/// @return 0-ok, other-error
int mov_reader_seek(mov_reader_t* mov, int64_t* timestamp);

/// get extra data
int64_t mov_video_extra_data(mov_reader_t* mov, char* data, int64_t size);
int64_t mov_audio_extra_data(mov_reader_t* mov, char* data, int64_t size);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_reader_h_*/
