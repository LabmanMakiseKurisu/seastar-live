#include "mov-internal.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

size_t mov_write_dac3(const struct mov_t* mov) {
    size_t size = 0;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io); /* size */
    mov_buffer_w32(&mov->io, 0);
    mov_buffer_write(&mov->io, "dac3", 4);

    // fscod:2
    // bsid:5
    // bsmod:3
    // acmod:3
    // lfeon:1
    // bit_rate_code:5
    // reserved:5
    mov_buffer_w8(
        &mov->io,
        ((mov->da_config.fscod & 0x03) << 6) |
        ((mov->da_config.bsid & 0x1F) << 1) |
        (mov->da_config.bsmod & 0x04));
    mov_buffer_w8(
        &mov->io,
        ((mov->da_config.bsmod & 0x03) << 6) |
        ((mov->da_config.acmod & 0x07) << 3) |
        ((mov->da_config.lfeon & 0x01) << 2) |
        ((mov->da_config.bit_rate_code & 0x018) << 3));
    mov_buffer_w8(
        &mov->io,
        ((mov->da_config.bit_rate_code & 0x07) << 5) | 0);

    size += 3;

    mov_write_size(mov, offset, size); /* update size */

    return size;
}

size_t mov_write_dec3(const struct mov_t* mov) {
    size_t size = 0;
    uint64_t offset;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0);
    mov_buffer_write(&mov->io, "dec3", 4);

    mov_buffer_w16(
        &mov->io, (mov->da_config.data_rate << 3) | 0);
    size += 8 + 2;
    // for (int i = 0; i < mov->da_config.num_ind_sub; i++) {
    // fscod:2
    // bsid:5
    // reserved:1
    mov_buffer_w8(
        &mov->io, (mov->da_config.fscod << 6) | (mov->da_config.bsid << 1) | 0);
    mov_buffer_w8(
        &mov->io, (mov->da_config.asvc << 7) | (mov->da_config.bsmod << 4) |
                      (mov->da_config.acmod << 1) | mov->da_config.lfeon);
    size += 2;
    if (mov->da_config.num_dep_sub > 0) {
        mov_buffer_w16(
            &mov->io,
            (mov->da_config.num_dep_sub << 9) | mov->da_config.chan_loc);
        size += 2;
    } else {
        mov_buffer_w8(&mov->io, mov->da_config.num_dep_sub << 1);
        size += 1;
    }
    // }
    mov_write_size(mov, offset, size);
    return size;
}