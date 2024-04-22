#include "mov-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/*
aligned(8) class SampleEncryptionBox
extends FullBox(‘senc’, version=0, flags)
{
    unsigned int(32) sample_count;
    {
        unsigned int(Per_Sample_IV_Size*8) InitializationVector;
        if (flags & 0x000002)
        {
            unsigned int(16) subsample_count;
            {
                unsigned int(16) bytes_clear_data;
                unsigned int(32) bytes_protected_data;
            } [ subsample_count ]
        }
    }[ sample_count ]
}
*/
size_t mov_write_senc(
    const struct mov_t* mov,
    uint32_t from,
    uint32_t count)
{
    const struct mov_sample_t* sample;
    const struct mov_track_t* track = mov->track;
    size_t size;
    uint64_t offset;

    size = 8   /* Box */;
    offset = mov_buffer_tell(&mov->io);

    mov_buffer_w32(&mov->io, 0); /* size */

    mov_buffer_write(&mov->io, "senc", 4);

    if (track->handler_type == MOV_VIDEO) {
        // video
        mov_buffer_w32(&mov->io, 2);              /* version + flags */
        size += 4;

        mov_buffer_w32(&mov->io, count);          /* sample info count */
        size += 4;

        // sample
        for (int i = from; i < from + count; i++) {
            // subsample_count
            sample = track->samples + i;
            mov_buffer_w16(&mov->io, sample->subsample_count);
            size += 2;
            for (int j = 0; j < sample->subsample_count; j++) {
                // bytes_clear_data
                mov_buffer_w16(&mov->io, sample->senc_info[j].bytes_clear_data);
                size += 2;
                // bytes_protected_data
                mov_buffer_w32(&mov->io, sample->senc_info[j].bytes_protected_data);
                size += 4;
            }
        }
    } else {
        // audio
        mov_buffer_w32(&mov->io, 0);     // version + flags
        size += 4;

        mov_buffer_w32(&mov->io, count); /* sample info count */
        size += 4;
    }

    mov_write_size(mov, offset, size); /* update size */

    return size;

};