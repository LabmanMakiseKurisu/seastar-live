#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

size_t mov_write_frma(const struct mov_t* mov) {
    const struct mov_track_t* track = mov->track;
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);

    mov_buffer_w32(&mov->io, 0); /* size */

    mov_buffer_write(&mov->io, "frma", 4);

    if (track->handler_type == MOV_VIDEO) {
        if (track->tag == MOV_H264) {
            mov_buffer_write(&mov->io, "avc1", 4);
        } else if (track->tag == MOV_HEVC) {
            mov_buffer_write(&mov->io, "hvc1", 4);
        }
    } else {
        switch (track->tag) {
        case MOV_EAC3:
            mov_buffer_write(&mov->io, "ec-3", 4);
            break;
        case MOV_AC3:
            mov_buffer_write(&mov->io, "ac-3", 4);
            break;
        case MOV_MP4A:
            mov_buffer_write(&mov->io, "mp4a", 4);
            break;
        case MOV_OPUS:
            mov_buffer_write(&mov->io, "Opus", 4);
        default:
            mov_buffer_write(&mov->io, "mp4a", 4);
            break;
        }
    }

    size += 4;

    mov_write_size(mov, offset, size); /* update size */

    return size;
}

size_t mov_write_schm(const struct mov_t* mov) {
    size_t size;
    uint64_t offset;

    size = 8 + 4/* Box */;
    offset = mov_buffer_tell(&mov->io);

    mov_buffer_w32(&mov->io, 0); /* size */

    mov_buffer_write(&mov->io, "schm", 4);

    mov_buffer_w32(&mov->io, 0); /* version + flag */

    /* scheme type */
    switch (mov->scheme_type)
    {
    case DRM_ENC_CENC/* constant-expression */:
        mov_buffer_write(&mov->io, "cenc", 4);
        break;
    case DRM_ENC_CBC1/* constant-expression */:
        mov_buffer_write(&mov->io, "cbc1", 4);
        break;
    case DRM_ENC_CENS/* constant-expression */:
        mov_buffer_write(&mov->io, "cens", 4);
        break;
    case DRM_ENC_CBCS/* constant-expression */:
        mov_buffer_write(&mov->io, "cbcs", 4);
        break;

    default:
        break;
    }

    size += 4;

    /* scheme version */
    mov_buffer_w32(&mov->io, 0x10000); /* size */

    size += 4;

    mov_write_size(mov, offset, size); /* update size */

    return size;
}


size_t mov_write_tenc(const struct mov_t* mov) {
    struct mov_track_t* track = mov->track;
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);

    mov_buffer_w32(&mov->io, 0); /* size */

    mov_buffer_write(&mov->io, "tenc", 4);

    track->default_iv_len = 16;
    //memset(track->default_iv, 0, track->default_iv_len);

    //track->default_kid_len = 0;
    //memset(track->default_kid, 0, 16);

    /* version & flags */
    if (track->default_iv_len != 0) {
        mov_buffer_w32(&mov->io, 0x01000000);      // version + flags
    } else {
        mov_buffer_w32(&mov->io, 0);      // version + flags
    }
    size += 4;

    /* is encrypted */
    switch (mov->scheme_type) {
    case DRM_ENC_CENC:
        mov_buffer_w32(&mov->io, 0x108);      // default is encrypted, iv size = 8
        break;

    case DRM_ENC_CBCS:
        switch (track->handler_type) {
        case MOV_VIDEO:
            mov_buffer_w32(&mov->io, 0x190100); // default is encrypted, 1/9 crypt/skip ratio
            break;

        case MOV_AUDIO:
            mov_buffer_w32(&mov->io, 0x000100); // default is encrypted
            break;

        default:
            break;
        }

        break;
    }
    size += 4;

    if (track->default_kid_len != 0) {
        mov_buffer_write(&mov->io, track->default_kid, 16);  // default key id
    } else {
        // zero filling
        unsigned char zero_kid[16] = {0};
        mov_buffer_write(&mov->io, zero_kid, 16);
    }

    size += 16;

    if (track->default_iv_len != 0) {
        mov_buffer_w8(&mov->io, 16);                         // default constant iv size
        mov_buffer_write(&mov->io, track->default_iv, 16);   // default constant iv
    }

    size += 1 + 16;


    mov_write_size(mov, offset, size); /* update size */

    return size;
}


size_t mov_write_schi(const struct mov_t* mov) {
    size_t size;
    uint64_t offset;

    size = 8/* Box */;
    offset = mov_buffer_tell(&mov->io);

    mov_buffer_w32(&mov->io, 0); /* size */

    mov_buffer_write(&mov->io, "schi", 4);

    size += mov_write_tenc(mov);

    mov_write_size(mov, offset, size); /* update size */

    return size;
}

size_t mov_write_sinf(const struct mov_t* mov) {
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "sinf", 4);

    size += mov_write_frma(mov);
    size += mov_write_schm(mov);
    size += mov_write_schi(mov);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}