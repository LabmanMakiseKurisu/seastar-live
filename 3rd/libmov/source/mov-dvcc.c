#include "mov-internal.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
align(8) class DOVIDecoderConfigurationRecord
{
 unsigned int (8) dv_version_major;
 unsigned int (8) dv_version_minor;
 unsigned int (7) dv_profile;
 unsigned int (6) dv_level;
 bit (1) rpu_present_flag;
 bit (1) el_present_flag;
 bit (1) bl_present_flag;
 unsigned int (4) dv_bl_signal_compatibility_id;
const unsigned int (28) reserved = 0;
 const unsigned int (32)[4] reserved = 0;
}
*/

// dv_profile <= 7 dvcC
int mov_read_dvcc(struct mov_t* mov, const struct mov_box_t* box) {
    struct mov_sample_entry_t* entry = mov->track->stsd.current;
    int32_t size = box->size;
    if (mov->v_extra_size + box->size > mov->v_extra_cap) {
        // need to recalloc
        mov->v_extra_cap *= 2;
        char* tmp = malloc(mov->v_extra_cap);
        memcpy(tmp, mov->v_extra_data, mov->v_extra_size);
        free(mov->v_extra_data);
        mov->v_extra_data = tmp;
    }
    memcpy(mov->v_extra_data, &box->size, 4);
    memcpy(mov->v_extra_data, &box->type, 4);
    mov_buffer_read(&mov->io, mov->v_extra_data, box->size);

    // memcpy(mov->video_extra_data, &size, 4);
    // memcpy(mov->video_extra_data, &box->type, 4);
    // mov_buffer_read(&mov->io, mov->video_extra_data, box->size);
    mov->v_extra_size += box->size + 8;
    return mov_buffer_error(&mov->io);
}

size_t mov_write_dvcc(const struct mov_t* mov) {
    const struct mov_track_t* track = mov->track;
    const struct mov_sample_entry_t* entry = track->stsd.current;
    assert(mov->dv_config.dv_profile <= 7);
    mov_buffer_w32(&mov->io, 24 + 8);
    mov_buffer_write(&mov->io, "dvcC", 4);
    // 4bytes
    mov_buffer_w8(&mov->io, mov->dv_config.dv_version_major);
    mov_buffer_w8(&mov->io, mov->dv_config.dv_version_minor);
    mov_buffer_w16(
        &mov->io, (mov->dv_config.dv_profile << 9) |
                      (mov->dv_config.dv_level << 3) |
                      (mov->dv_config.rpu_present_flag << 2) |
                      (mov->dv_config.el_present_flag << 1) |
                      (mov->dv_config.bl_present_flag));
    // 4bytes
    mov_buffer_w16(
        &mov->io, (mov->dv_config.dv_bl_signal_compatibility_id << 12) | 0);
    mov_buffer_w16(&mov->io, 0);
    // 16byte
    mov_buffer_w32(&mov->io, 0);
    mov_buffer_w32(&mov->io, 0);
    mov_buffer_w32(&mov->io, 0);
    mov_buffer_w32(&mov->io, 0);
    return 32;
}