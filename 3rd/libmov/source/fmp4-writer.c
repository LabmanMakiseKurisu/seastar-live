#include "fmp4-writer.h"
#include "mov-atom.h"
#include "mov-internal.h"
#include "mov-reader.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define _SWAP_U16_(v) ((((v)&0x00FF) << 8) | (((v)&0xFF00) >> 8))
#define _SWAP_U32_(v) ((_SWAP_U16_(v) << 16) | (_SWAP_U16_(v >> 16)))

#define _BILI_FIXED_

struct fmp4_writer_t {
    struct mov_t mov;
    size_t mdat_size;
    int has_moov;

    uint32_t frag_interleave;
    uint32_t fragment_id;  // start from 1
    uint32_t sn;           // sample sn

#ifdef _BILI_FIXED_
    int aac_profile;
#endif
};

static size_t fmp4_write_mvex(struct mov_t* mov) {
    int i;
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "mvex", 4);

    // size += fmp4_write_mehd(mov);
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;
        size += mov_write_trex(mov);
    }
    // size += mov_write_leva(mov);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

// pssh box
static size_t fmp4_write_pssh(struct mov_t* mov) {
    int i;
    size_t size;
    uint64_t offset;

    size = 8;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "pssh", 4);

    // full box
    mov_buffer_w32(&mov->io, 0);
    size += 4;

    // pssh data
    mov_buffer_write(&mov->io, mov->pssh_data + 12, mov->pssh_data_len - 12);
    size += (mov->pssh_data_len - 12);

    mov_write_size(mov, offset, size); /* update size */

    return size;
};

static size_t fmp4_write_traf(struct mov_t* mov, uint32_t moof) {
    uint32_t i, start;
    size_t size;
    uint64_t offset;
    struct mov_track_t* track;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "traf", 4);

    track = mov->track;
    track->tfhd.flags = MOV_TFHD_FLAG_DEFAULT_FLAGS /*| MOV_TFHD_FLAG_BASE_DATA_OFFSET*/;
    track->tfhd.flags |= MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX;
    // ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
    // The 'moof' boxes shall use movie-fragment relative addressing for media
    // data that does not use external data references, the flag
    // 'default-base-is-moof' shall be set, and data-offset shall be used, i.e.
    // base-data-offset-present shall not be used.
    // if (mov->flags & MOV_FLAG_SEGMENT)
    {
        // track->tfhd.flags &= ~MOV_TFHD_FLAG_BASE_DATA_OFFSET;
        track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF;
    }
    track->tfhd.base_data_offset = mov->moof_offset;
    track->tfhd.sample_description_index = 1;
    track->tfhd.default_sample_flags =
        MOV_AUDIO == track->handler_type
            ? MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_I_PICTURE
            : (MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE | MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_NOT_I_PICTURE);
    if (track->sample_count > 0) {
#ifdef _BILI_FIXED_
        if (MOV_AUDIO == track->handler_type) {
            track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION;
            track->tfhd.default_sample_duration = track->default_duration;
        }
#else
        track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION | MOV_TFHD_FLAG_DEFAULT_SIZE;
        track->tfhd.default_sample_duration =
            track->sample_count > 1 ? (uint32_t)(track->samples[1].dts - track->samples[0].dts) : 0;
        track->tfhd.default_sample_size = track->samples[0].bytes;
#endif
    } else {
        track->tfhd.flags |= MOV_TFHD_FLAG_DURATION_IS_EMPTY;
        track->tfhd.default_sample_duration = 0;  // not set
        track->tfhd.default_sample_size = 0;      // not set
    }

    size += mov_write_tfhd(mov);
    // ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
    // Each 'traf' box shall contain a 'tfdt' box.
    size += mov_write_tfdt(mov);

#ifdef _BILI_FIXED_
    // write only one trun, same as in fmp4_write_moof()
    size += mov_write_trun(mov, 0, track->sample_count, moof);
    // senc
    if (mov->is_drm) {
        size += mov_write_senc(mov, 0, track->sample_count);
    }
#else
    for (start = 0, i = 1; i < track->sample_count; i++) {
        if (track->samples[i - 1].offset + track->samples[i - 1].bytes != track->samples[i].offset) {
            size += mov_write_trun(mov, start, i - start, moof);
            start = i;
        }
    }
    size += mov_write_trun(mov, start, i - start, moof);
#endif

    // write udta
    if (track->udta.size > 0) {
        size_t udta_size = 4 + 4 + track->udta.size;
        mov_buffer_w32(&mov->io, udta_size); /* size */
        mov_buffer_write(&mov->io, "udta", 4);
        mov_buffer_write(&mov->io, track->udta.data, track->udta.size);
        size += udta_size;
        // clear
        track->udta.size = 0;
    }

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

#ifdef _BILI_FIXED_
static size_t fmp4_get_moof_size(struct mov_t* mov) {
    int i;
    size_t size = 8;  // box
    size += 16;       // mfhd
    struct mov_sample_t* sample;
    for (i = 0; i < mov->track_count; i++) {
        const struct mov_track_t* track = &mov->tracks[i];
        if (track->sample_count > 0) {
            int default_duration = (MOV_TFHD_FLAG_DEFAULT_DURATION & track->tfhd.flags) != 0;
            size += 8;                                              // traf box
            size += 12 + 4 + (4 + 4 + (default_duration ? 4 : 0));  // tfhd: MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX &
                                                                    // MOV_TFHD_FLAG_DEFAULT_FLAGS
            size += 20;                                             // tfdt
            size += 12 + 4 + 4 + 4 + track->sample_count * ((default_duration) ? 4 : 12);  // trun
            if (mov->is_drm) {                                                             // senc
                if (track->handler_type == MOV_VIDEO) {
                    size += 12 + 4;
                    for (int i = 0; i < track->sample_count; i++) {
                        size += 2;
                        sample = track->samples + i;
                        size += sample->subsample_count * 6;
                    }
                } else {
                    size += 12 + 4;
                }
            }
        }
    }
    return size;
}
#endif

static size_t fmp4_write_moof(struct mov_t* mov, uint32_t fragment, uint32_t moof) {
    int i;
    size_t size, j;
    uint64_t offset;
    uint64_t n;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "moof", 4);

    size += mov_write_mfhd(mov, fragment);

    n = 0;
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;

        // rewrite offset, write only one trun
        // 2017/10/17 Dale Curtis SHA-1:
        // a5fd8aa45b11c10613e6e576033a6b5a16b9cbb9 (libavformat/mov.c)
        for (j = 0; j < mov->track->sample_count; j++) {
            mov->track->samples[j].offset = n;
            n += mov->track->samples[j].bytes;
        }

        if (mov->track->sample_count > 0)
            size += fmp4_write_traf(mov, moof);
    }

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

static size_t fmp4_write_moov(struct mov_t* mov) {
    int i;
    size_t size;
    uint32_t count;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "moov", 4);
    size += mov_write_mvhd(mov);
    //	size += fmp4_write_iods(mov);
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;
        count = mov->track->sample_count;
        mov->track->sample_count = 0;
        size += mov_write_trak(mov);
        mov->track->sample_count = count;  // restore sample count
    }

    size += fmp4_write_mvex(mov);
    //  size += fmp4_write_udta(mov);

    // drm
    if (mov->is_drm) {
        size += fmp4_write_pssh(mov);
    }

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

static size_t fmp4_write_sidx(struct mov_t* mov) {
    int i;
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;
        mov_write_sidx(mov, 52 * (uint64_t)(mov->track_count - i - 1)); /* first_offset */
    }

    return 52 * mov->track_count;
}

static int fmp4_write_mfra(struct mov_t* mov) {
    int i;
    uint64_t mfra_offset;
    uint64_t mfro_offset;

    // mfra
    mfra_offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "mfra", 4);

    // tfra
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;
        mov_write_tfra(mov);
    }

    // mfro
    mfro_offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 16); /* size */
    mov_buffer_write(&mov->io, "mfro", 4);
    mov_buffer_w32(&mov->io, 0); /* version & flags */
    mov_buffer_w32(&mov->io, (uint32_t)(mfro_offset - mfra_offset + 16));

    mov_write_size(mov, mfra_offset, (size_t)(mfro_offset - mfra_offset + 16));
    return (int)(mfro_offset - mfra_offset + 16);
}

static int fmp4_add_fragment_entry(struct mov_track_t* track, uint64_t time, uint64_t offset) {
    if (track->frag_count >= track->frag_capacity) {
        void* p = realloc(track->frags, sizeof(struct mov_fragment_t) * (track->frag_capacity + 64));
        if (!p)
            return ENOMEM;
        track->frags = p;
        track->frag_capacity += 64;
    }

    track->frags[track->frag_count].time = time;
    track->frags[track->frag_count].offset = offset;
    ++track->frag_count;
    return 0;
}

#ifdef _BILI_FIXED_
static int fmp4_write_fragment(struct fmp4_writer_t* writer, int64_t next_video_dts, uint32_t fragment_id)
#else
static int fmp4_write_fragment(struct fmp4_writer_t* writer, uint32_t fragment_id)
#endif
{
    int i;
    size_t n;
    size_t refsize, moof_size;
    struct mov_t* mov;
    mov = &writer->mov;

    if (writer->mdat_size < 1)
        return 0;  // empty

#ifdef _BILI_FIXED_
    for (i = 0; i < mov->track_count; i++) {
        struct mov_track_t* track = mov->tracks + i;
        if (track->sample_count > 0 && track->handler_type == MOV_VIDEO) {
            // fix last sample's duration which not set in fmp4_writer_write()
            int64_t duration = 0;
            int64_t last_dts = track->samples[track->sample_count - 1].dts;
            if (next_video_dts != 0) {
                duration = next_video_dts * track->mdhd.timescale / 1000 - last_dts;
            } else if (track->sample_count > 1)
                duration = (last_dts - track->samples[0].dts) / (track->sample_count - 1);
            else
                duration = track->mdhd.timescale / 30;  // assume 30 FPS
            track->samples[track->sample_count - 1].duration = (uint32_t)duration;
            track->total_duration += duration;
        }
    }
#endif
    // write moov
    if (!writer->has_moov) {
        // write ftyp/stype
        if (mov->flags & MOV_FLAG_SEGMENT) {
            mov_write_styp(mov);
        }
#ifndef _BILI_FIXED_  // don't need moov every segment, we do it ourselves
        else {
            mov_write_ftyp(mov);
            fmp4_write_moov(mov);
        }
#endif
        writer->has_moov = 1;
    }

    if (mov->flags & MOV_FLAG_SEGMENT) {
        // ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
        // Each Media Segment may contain one or more 'sidx' boxes.
        // If present, the first 'sidx' box shall be placed before any 'moof'
        // box and the first Segment Index box shall document the entire
        // Segment.
        fmp4_write_sidx(mov);
    }

    // moof
    mov->moof_offset = mov_buffer_tell(&mov->io);
#ifdef _BILI_FIXED_
    refsize = fmp4_get_moof_size(mov);
    moof_size = fmp4_write_moof(mov, fragment_id, (uint32_t)refsize + 8);  // start from 1
    assert(moof_size == refsize);
#else
    refsize = fmp4_write_moof(mov, fragment_id, 0);  // start from 1
    // rewrite moof with trun data offset
    mov_buffer_seek(&mov->io, mov->moof_offset);
    fmp4_write_moof(mov, fragment_id, (uint32_t)refsize + 8);
#endif
    refsize += writer->mdat_size + 8 /*mdat box*/;

    // add mfra entry
    for (i = 0; i < mov->track_count; i++) {
        mov->track = mov->tracks + i;
#ifndef _BILI_FIXED_
        if (mov->track->sample_count > 0)
            fmp4_add_fragment_entry(mov->track, mov->track->samples[0].dts, mov->moof_offset);
#endif

        // hack: write sidx referenced_size
        if (mov->flags & MOV_FLAG_SEGMENT)
            mov_write_size(
                mov, mov->moof_offset - 52 * (uint64_t)(mov->track_count - i) + 40, (0 << 31) | (refsize & 0x7fffffff));

        mov->track->offset = 0;  // reset
    }

    // mdat
    if (writer->mdat_size + 8 <= UINT32_MAX) {
        mov_buffer_w32(&mov->io, (uint32_t)writer->mdat_size + 8); /* size */
        mov_buffer_write(&mov->io, "mdat", 4);
    } else {
        mov_buffer_w32(&mov->io, 1);
        mov_buffer_write(&mov->io, "mdat", 4);
        mov_buffer_w64(&mov->io, writer->mdat_size + 16);
    }

    // interleave write samples
    n = 0;
    while (n < writer->mdat_size) {
        for (i = 0; i < mov->track_count; i++) {
            mov->track = mov->tracks + i;
            while (mov->track->offset < mov->track->sample_count &&
                   n == mov->track->samples[mov->track->offset].offset) {
#ifdef _BILI_FIXED_
                struct mov_sample_t* sample = &mov->track->samples[mov->track->offset];
                // TODO
                mov_buffer_write(&mov->io, sample->data, sample->bytes);
                n += sample->bytes;
#else
                mov_buffer_write(
                    &mov->io, mov->track->samples[mov->track->offset].data,
                    mov->track->samples[mov->track->offset].bytes);
                free(mov->track->samples[mov->track->offset].data);  // free av packet memory
                n += mov->track->samples[mov->track->offset].bytes;
#endif
                ++mov->track->offset;
            }
        }
    }

    // clear track samples(don't free samples memory)
    for (i = 0; i < mov->track_count; i++) {
        mov->tracks[i].sample_count = 0;
        mov->tracks[i].offset = 0;
#ifdef _BILI_FIXED_
        mov->tracks[i].samples_size = 0;
#endif
    }
    writer->mdat_size = 0;

    return mov_buffer_error(&mov->io);
}

static int fmp4_writer_init(struct mov_t* mov) {
    if (mov->flags & MOV_FLAG_SEGMENT) {
        mov->ftyp.major_brand = MOV_BRAND_MSDH;
        mov->ftyp.minor_version = 0;
        mov->ftyp.brands_count = 4;
        mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
        mov->ftyp.compatible_brands[1] = MOV_BRAND_MP42;
        mov->ftyp.compatible_brands[2] = MOV_BRAND_MSDH;
        mov->ftyp.compatible_brands[3] = MOV_BRAND_MSIX;
        mov->header = 0;
    } else {
        mov->ftyp.major_brand = MOV_BRAND_ISOM;
        mov->ftyp.minor_version = 1;
        mov->ftyp.brands_count = 4;
        mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
        mov->ftyp.compatible_brands[1] = MOV_BRAND_MP42;
        mov->ftyp.compatible_brands[2] = MOV_BRAND_AVC1;
        mov->ftyp.compatible_brands[3] = MOV_BRAND_DASH;
        mov->header = 0;
    }
    return 0;
}

#ifdef _BILI_FIXED_
struct fmp4_writer_t* fmp4_writer_create(int flags)
#else
struct fmp4_writer_t* fmp4_writer_create(const struct mov_buffer_t* buffer, void* param, int flags)
#endif
{
    struct mov_t* mov;
    struct fmp4_writer_t* writer;
    writer = (struct fmp4_writer_t*)calloc(1, sizeof(struct fmp4_writer_t));
    if (NULL == writer)
        return NULL;

    writer->frag_interleave = 5;

    mov = &writer->mov;
    mov->flags = flags;
    mov->mvhd.next_track_ID = 1;
    mov->mvhd.creation_time = time(NULL) + 0x7C25B080;  // 1970 based -> 1904 based;
    mov->mvhd.modification_time = mov->mvhd.creation_time;
#ifdef _BILI_FIXED_
    mov->mvhd.timescale = 90 * 1000;
#else
    mov->mvhd.timescale = 1000;
#endif
    mov->mvhd.duration = 0;  // placeholder

    memset(&mov->dv_config, 0, sizeof(mov->dv_config));
    memset(&mov->da_config, 0, sizeof(mov->da_config));
    mov->is_dovi = 0;

    mov->v_extra_size = 0;
    mov->a_extra_size = 0;

    mov->v_extra_cap = 1024;
    mov->a_extra_cap = 1024;

    mov->v_extra_data = malloc(1024);
    mov->a_extra_data = malloc(1024);
    if (mov->v_extra_data == NULL || mov->a_extra_data == NULL) {
        fprintf(stderr, "malloc failed");
        return NULL;
    }

    fmp4_writer_init(mov);

#ifndef _BILI_FIXED_
    mov->io.param = param;
    memcpy(&mov->io.io, buffer, sizeof(mov->io.io));
#endif
    return writer;
}

#ifdef _BILI_FIXED_
void fmp4_writer_set_compressor_name(struct fmp4_writer_t* writer, const char* name) {
    strncpy(writer->mov.compressorname, name, sizeof(writer->mov.compressorname));
}
#endif

void fmp4_writer_destroy(struct fmp4_writer_t* writer) {
    int i;
    struct mov_t* mov;
    mov = &writer->mov;

#ifndef _BILI_FIXED_
    fmp4_writer_save_segment(writer);
#endif

    for (i = 0; i < mov->track_count; i++)
        mov_free_track(mov->tracks + i);
    if (mov->tracks)
        free(mov->tracks);
    if (mov->v_extra_data)
        free(mov->v_extra_data);
    if (mov->a_extra_data)
        free(mov->a_extra_data);
    free(writer);
}

#ifdef _BILI_FIXED_
int fmp4_writer_write_(
    struct fmp4_writer_t* writer, struct mov_track_t* track, const void* data, size_t bytes, int64_t pts, int64_t dts,
    int flags) {
    if (data == NULL || bytes == 0) {
        return -EINVAL;
    }
    struct mov_sample_t* sample;
#else
int fmp4_writer_write(
    struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags) {
    struct mov_track_t* track;
    struct mov_sample_t* sample;
    if (idx < 0 || idx >= (int)writer->mov.track_count)
        return -ENOENT;

    track = &writer->mov.tracks[idx];
    if (MOV_VIDEO == track->handler_type && (flags & MOV_AV_FLAG_KEYFREAME))
        fmp4_write_fragment(writer);  // fragment per video keyframe
#endif
    // 如果sample写完了 需要扩容
    if (track->sample_count + 1 >= track->sample_offset) {
        void* ptr = realloc(track->samples, sizeof(struct mov_sample_t) * (track->sample_offset + 1024));
        if (NULL == ptr)
            return -ENOMEM;
#ifdef _BILI_FIXED_
        memset(
            (uint8_t*)ptr + sizeof(struct mov_sample_t) * track->sample_offset, 0, sizeof(struct mov_sample_t) * 1024);
#endif
        track->samples = (struct mov_sample_t*)ptr;
        track->sample_offset += 1024;
    }

    pts = pts * track->mdhd.timescale / 1000;
    dts = dts * track->mdhd.timescale / 1000;

    sample = &track->samples[track->sample_count];
    sample->sample_description_index = 1;
    sample->bytes = (uint32_t)bytes;
    sample->flags = flags;
    sample->pts = pts;
    sample->dts = dts;
    sample->offset = writer->mdat_size;

#ifdef _BILI_FIXED_
    size_t new_size = track->samples_size + bytes;
    if (new_size > track->samples_capacity) {
        uint32_t i;
        size_t offset = 0;
        if (new_size < track->samples_capacity << 1)
            new_size = track->samples_capacity << 1;
        track->samples_buffer = realloc(track->samples_buffer, new_size);
        if (NULL == track->samples_buffer)
            return -ENOMEM;
        track->samples_capacity = new_size;
        for (i = 0; i < track->sample_count; i++) {
            track->samples[i].data = (char*)track->samples_buffer + offset;
            offset += track->samples[i].bytes;
        }
    }
    sample->data = (char*)track->samples_buffer + track->samples_size;
    track->samples_size += bytes;
#else
    sample->data = malloc(bytes);
    if (NULL == sample->data)
        return -ENOMEM;
#endif
    memcpy(sample->data, data, bytes);

    if (INT64_MIN == track->start_dts)
        track->start_dts = sample->dts;
    writer->mdat_size += bytes;  // update media data size
    track->sample_count += 1;
    return 0;
}

#ifdef _BILI_FIXED_
static struct mov_sample_t* aac_get_silent_frame(int aac_profile, int channel_count) {
    static uint8_t s_profile_2_channel_1[] = { 0x00, 0xc8, 0x00, 0x80, 0x23, 0x80 };
    static uint8_t s_profile_2_channel_2[] = { 0x21, 0x00, 0x49, 0x90, 0x02, 0x19, 0x00, 0x23, 0x80 };
    static uint8_t s_profile_2_channel_3[] = { 0x00, 0xc8, 0x00, 0x80, 0x20, 0x84, 0x01,
                                               0x26, 0x40, 0x08, 0x64, 0x00, 0x8e };
    static uint8_t s_profile_2_channel_4[] = { 0x00, 0xc8, 0x00, 0x80, 0x20, 0x84, 0x01, 0x26, 0x40,
                                               0x08, 0x64, 0x00, 0x80, 0x2c, 0x80, 0x08, 0x02, 0x38 };
    static uint8_t s_profile_2_channel_5[] = { 0x00, 0xc8, 0x00, 0x80, 0x20, 0x84, 0x01, 0x26, 0x40, 0x08, 0x64,
                                               0x00, 0x82, 0x30, 0x04, 0x99, 0x00, 0x21, 0x90, 0x02, 0x38 };
    static uint8_t s_profile_2_channel_6[] = { 0x00, 0xc8, 0x00, 0x80, 0x20, 0x84, 0x01, 0x26, 0x40,
                                               0x08, 0x64, 0x00, 0x82, 0x30, 0x04, 0x99, 0x00, 0x21,
                                               0x90, 0x02, 0x00, 0xb2, 0x00, 0x20, 0x08, 0xe0 };

    static uint8_t s_profile_x_channel_1[] = { 0x1,  0x40, 0x22, 0x80, 0xa3, 0x4e, 0xe6, 0x80, 0xba, 0x8,  0x0,  0x0,
                                               0x0,  0x1c, 0x6,  0xf1, 0xc1, 0xa,  0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5e };
    static uint8_t s_profile_x_channel_2[] = { 0x1,  0x40, 0x22, 0x80, 0xa3, 0x5e, 0xe6, 0x80, 0xba, 0x8,  0x0,  0x0,
                                               0x0,  0x0,  0x95, 0x0,  0x6,  0xf1, 0xa1, 0xa,  0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5e };
    static uint8_t s_profile_x_channel_3[] = { 0x1,  0x40, 0x22, 0x80, 0xa3, 0x5e, 0xe6, 0x80, 0xba, 0x8,  0x0,  0x0,
                                               0x0,  0x0,  0x95, 0x0,  0x6,  0xf1, 0xa1, 0xa,  0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                                               0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5e };

    static struct mov_sample_t s_profile_2[] = {
        { 0, 0, 0, s_profile_2_channel_1, 0, sizeof(s_profile_2_channel_1) },
        { 0, 0, 0, s_profile_2_channel_2, 0, sizeof(s_profile_2_channel_2) },
        { 0, 0, 0, s_profile_2_channel_3, 0, sizeof(s_profile_2_channel_3) },
        { 0, 0, 0, s_profile_2_channel_4, 0, sizeof(s_profile_2_channel_4) },
        { 0, 0, 0, s_profile_2_channel_5, 0, sizeof(s_profile_2_channel_5) },
        { 0, 0, 0, s_profile_2_channel_6, 0, sizeof(s_profile_2_channel_6) },
    };

    static struct mov_sample_t s_profile_x[] = {
        { 0, 0, 0, s_profile_x_channel_1, 0, sizeof(s_profile_x_channel_1) },
        { 0, 0, 0, s_profile_x_channel_2, 0, sizeof(s_profile_x_channel_2) },
        { 0, 0, 0, s_profile_x_channel_3, 0, sizeof(s_profile_x_channel_3) },
    };

    switch (aac_profile) {
        case 2:
            if (channel_count >= 1 && channel_count <= 6)
                return &s_profile_2[channel_count - 1];
            break;

        default:
            if (channel_count >= 1 && channel_count <= 3)
                return &s_profile_x[channel_count - 1];
            break;
    }
    return NULL;
}

int fmp4_writer_write(
    struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags) {
    struct mov_track_t* track;
    struct mov_sample_t* sample;
    int ret;
    if (idx < 0 || idx >= (int)writer->mov.track_count)
        return -ENOENT;
    track = &writer->mov.tracks[idx];
    ret = fmp4_writer_write_(writer, track, data, bytes, pts, dts, flags);
    if (ret != 0)
        return ret;

    sample = &track->samples[track->sample_count - 1];

    if (track->sample_count == 1) {
        // fix the first sample's dts and pts according to the caculated
        // total_duration, to fix baseMediaDecodeTime in tfdt
        // int64_t fixed_dts = track->start_dts + track->total_duration;

        // if (sample->dts != fixed_dts) {
        //     int64_t cts = sample->pts - sample->dts;
        //     sample->dts = fixed_dts;
        //     sample->pts = sample->dts + cts;
        // }
    }

    if (track->handler_type == MOV_AUDIO) {
        const int frame_duration = track->default_duration;
        const int max_gap_duration = frame_duration * 2;
        int64_t fixed_dts = track->start_dts + track->total_duration;
        int64_t duration_lost = sample->dts - fixed_dts;

        sample->duration = frame_duration;
        track->total_duration += sample->duration;

        // insert/drop frames only aac support
        if (MOV_OBJECT_AAC == track->stsd.entries[0].object_type_indication && duration_lost >= max_gap_duration) {
            int frame_count = (int)(duration_lost / frame_duration);
            if (frame_count >= 4096)
                return -E2BIG;
            int channel_count = track->stsd.entries[0].u.audio.channelcount;
            struct mov_sample_t* slient_frame = aac_get_silent_frame(writer->aac_profile, channel_count);
            struct mov_sample_t previous_sample_copy;
            previous_sample_copy.data = NULL;
            // sample->data can be freed by realloc() in fmp4_writer_write_(), the previous frame
            // should be copied before being used to inerting frames.
            if (slient_frame == NULL) {
                previous_sample_copy.data = malloc(sample->bytes);
                if (previous_sample_copy.data == NULL) {
                    return -ENOMEM;
                }
                previous_sample_copy.bytes = sample->bytes;
                memcpy(previous_sample_copy.data, sample->data, sample->bytes);
                slient_frame = &previous_sample_copy;
            }
            // printf("insert %d frames due to %.3f\n", frame_count,
            // duration_lost / (double)track->mdhd.timescale);
            while (frame_count-- > 0) {
                int ret = fmp4_writer_write_(
                    writer, track, slient_frame->data, slient_frame->bytes, fixed_dts, fixed_dts, flags);
                if (ret != 0) {
                    return ret;
                }
                sample = &track->samples[track->sample_count - 1];
                sample->duration = frame_duration;
                track->total_duration += sample->duration;
                fixed_dts += sample->duration;
            }
            if (previous_sample_copy.data != NULL) {
                free(previous_sample_copy.data);
            }
        }
        //
        else if (
            MOV_OBJECT_AAC == track->stsd.entries[0].object_type_indication && duration_lost <= -max_gap_duration) {
            int frame_count = -(int)(duration_lost / frame_duration);
            if (frame_count >= 4096)
                return -E2BIG;

            // printf("drop %d frames due to %.3f\n", frame_count,
            // -duration_lost / (double)track->mdhd.timescale);
            while (frame_count-- > 0 && track->sample_count > 0) {
                sample = &track->samples[--track->sample_count];
                writer->mdat_size -= sample->bytes;
                track->total_duration -= sample->duration;
            }
        }
    }

    else if (track->handler_type == MOV_VIDEO) {
        if (track->sample_count >= 2) {
            // last sample's duration will be set in fmp4_write_fragment()
            struct mov_sample_t* prev_sample = &track->samples[track->sample_count - 2];
            prev_sample->duration = sample->dts > prev_sample->dts ? (uint32_t)(sample->dts - prev_sample->dts) : 0;
            track->total_duration += prev_sample->duration;
        }
    }
    return ret;
}

int fmp4_writer_add_udta( struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes) {
    struct mov_track_t* track;

    if (idx < 0 || idx >= (int)writer->mov.track_count)
        return -ENOENT;
    track = &writer->mov.tracks[idx];

    // append data
    size_t new_size = track->samples_size + bytes;
    if (new_size > track->udta.capacity) {
        if (new_size < track->udta.capacity << 1) {
            new_size = track->udta.capacity << 1;
        }
        track->udta.data = realloc(track->udta.data, new_size);
        if (NULL == track->udta.data) {
            return -ENOMEM;
        }
        track->udta.capacity = new_size;
    }
    memcpy(track->udta.data + bytes, data, bytes);
    track->udta.size += bytes;

    return 0;
}

int fmp4_writer_drm_write(
    struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags,
    void* senc_info, uint32_t subsample_count) {
    struct mov_track_t* track;
    struct mov_sample_t* sample;
    mov_senc_info_t* senc_info_t = NULL;
    int ret;
    if (idx < 0 || idx >= (int)writer->mov.track_count)
        return -ENOENT;
    track = &writer->mov.tracks[idx];
    ret = fmp4_writer_write_(writer, track, data, bytes, pts, dts, flags);
    if (ret != 0)
        return ret;

    sample = &track->samples[track->sample_count - 1];

    if (track->sample_count == 1) {
        // fix the first sample's dts and pts according to the caculated
        // total_duration, to fix baseMediaDecodeTime in tfdt
        // int64_t fixed_dts = track->start_dts + track->total_duration;

        // if (sample->dts != fixed_dts) {
        //     int64_t cts = sample->pts - sample->dts;
        //     sample->dts = fixed_dts;
        //     sample->pts = sample->dts + cts;
        // }
    }

    // for senc
    if (senc_info != NULL && subsample_count != 0) {
        if (subsample_count + 1 >= sample->subsample_offset) {
            // realloc
            void* ptr = realloc(sample->senc_info, sizeof(mov_senc_info_t) * (sample->subsample_offset + 16));
            if (NULL == ptr) {
                return ENOMEM;
            }
            memset((uint8_t*)ptr + sizeof(mov_senc_info_t) * sample->subsample_offset, 0, sizeof(mov_senc_info_t) * 16);
            sample->subsample_offset += 16;
            sample->senc_info = ptr;
        }
        sample->subsample_count = subsample_count;
        senc_info_t = ((mov_senc_info_t*)senc_info);
        for (int i = 0; i < subsample_count; i++) {
            sample->senc_info[i].bytes_clear_data = senc_info_t[i].bytes_clear_data;
            sample->senc_info[i].bytes_protected_data = senc_info_t[i].bytes_protected_data;
        }
    } else {
        sample->subsample_count = 0;
        sample->senc_info = NULL;
    }

    if (track->handler_type == MOV_AUDIO) {
        const int frame_duration = track->default_duration;
        const int max_gap_duration = frame_duration * 2;
        int64_t fixed_dts = track->start_dts + track->total_duration;
        int64_t duration_lost = sample->dts - fixed_dts;

        sample->duration = frame_duration;
        track->total_duration += sample->duration;

        // insert/drop frames only aac support
        if (MOV_OBJECT_AAC == track->stsd.entries[0].object_type_indication && duration_lost >= max_gap_duration) {
            int frame_count = (int)(duration_lost / frame_duration);
            if (frame_count >= 4096)
                return -E2BIG;
            int channel_count = track->stsd.entries[0].u.audio.channelcount;
            struct mov_sample_t* slient_frame = aac_get_silent_frame(writer->aac_profile, channel_count);
            if (slient_frame == NULL)
                slient_frame = sample;
            // printf("insert %d frames due to %.3f\n", frame_count,
            // duration_lost / (double)track->mdhd.timescale);
            while (frame_count-- > 0) {
                int ret = fmp4_writer_write_(
                    writer, track, slient_frame->data, slient_frame->bytes, fixed_dts, fixed_dts, flags);
                if (ret != 0) {
                    return ret;
                }
                sample = &track->samples[track->sample_count - 1];
                sample->duration = frame_duration;
                track->total_duration += sample->duration;
                fixed_dts += sample->duration;
            }
        }
        //
        else if (
            MOV_OBJECT_AAC == track->stsd.entries[0].object_type_indication && duration_lost <= -max_gap_duration) {
            int frame_count = -(int)(duration_lost / frame_duration);
            if (frame_count >= 4096)
                return -E2BIG;

            // printf("drop %d frames due to %.3f\n", frame_count,
            // -duration_lost / (double)track->mdhd.timescale);
            while (frame_count-- > 0 && track->sample_count > 0) {
                sample = &track->samples[--track->sample_count];
                writer->mdat_size -= sample->bytes;
                track->total_duration -= sample->duration;
            }
        }
    }

    else if (track->handler_type == MOV_VIDEO) {
        if (track->sample_count >= 2) {
            // last sample's duration will be set in fmp4_write_fragment()
            struct mov_sample_t* prev_sample = &track->samples[track->sample_count - 2];
            prev_sample->duration = sample->dts > prev_sample->dts ? (uint32_t)(sample->dts - prev_sample->dts) : 0;
            track->total_duration += prev_sample->duration;
        }
    }
    return ret;
}

static int find_track_by_type(const struct mov_t* mov, uint32_t handler_type) {
    int i;
    for (i = 0; i < mov->track_count; i++) {
        if (mov->tracks[i].handler_type == handler_type)
            return i;
    }
    return -1;
}

// init drm mov & track
int fmp4_write_init_drm(struct mov_t* mov, struct mov_track_t* track, drm_info_t* drm_info) {
    if (mov == NULL || track == NULL || drm_info == NULL) {
        return -1;
    }

    // init mov
    mov->is_drm = drm_info->is_drm;
    mov->scheme_type = drm_info->scheme_type;
    mov->drm_type = drm_info->drm_type;
    // init pssh
    memset(mov->pssh_data, 0, 256);
    mov->pssh_data_len = drm_info->pssh_data_len;
    memcpy(mov->pssh_data, drm_info->pssh_data, mov->pssh_data_len);

    // init track
    track->is_drm = drm_info->is_drm;
    track->default_kid_len = drm_info->default_kid_len;
    memcpy(track->default_kid, drm_info->kid, track->default_kid_len);
    track->default_iv_len = drm_info->default_iv_len;
    memcpy(track->default_iv, drm_info->iv, track->default_iv_len);

    return 0;
}

int fmp4_writer_add_audio(
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;
    int track_index = find_track_by_type(mov, MOV_AUDIO);
    struct mov_track_t* track = track_index != -1 ? &mov->tracks[track_index] : NULL;
    uint32_t saved_track_ID = mov->mvhd.next_track_ID;
    if (track_index == -1) {
        track = mov_add_track(mov);
        if (NULL == track)
            return -ENOMEM;
        track_index = mov->track_count++;
    } else {
        uint32_t track_ID = track->tkhd.track_ID;
        mov_free_track(track);
        track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
        if (NULL == track->stsd.entries)
            return -ENOMEM;

        track->stsd.current = track->stsd.entries;
        mov->mvhd.next_track_ID = track_ID;
    }
    track->bitrate = bitrate;

    track->samples_buffer = realloc(track->samples_buffer, 256 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 256 * 1024;

    track->udta.data = realloc(track->udta.data, 256 * 1024);
    if (NULL == track->udta.data) {
        return -ENOMEM;
    }
    track->udta.size = 0;
    track->udta.capacity = 256 * 1024;

    track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION;
    track->tfhd.default_sample_duration = track->default_duration = sample_duration;
    if (MOV_OBJECT_AAC == object && extra_data_size > 0)
        writer->aac_profile = (((uint8_t*)extra_data)[0] >> 3) & 0x1F;
    if (0 != mov_add_audio(
                 track, &mov->mvhd, sample_rate, object, channel_count, bits_per_sample, sample_rate, extra_data,
                 extra_data_size))
        return -ENOMEM;

    if (saved_track_ID == mov->mvhd.next_track_ID)
        mov->mvhd.next_track_ID++;
    else
        mov->mvhd.next_track_ID = saved_track_ID;
    return track_index;
}

int fmp4_writer_add_audio2(
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;

    struct mov_track_t* track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;
    int track_index = mov->track_count++;
    track->bitrate = bitrate;

    track->samples_buffer = realloc(track->samples_buffer, 256 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 256 * 1024;

    track->udta.data = realloc(track->udta.data, 256 * 1024);
    if (NULL == track->udta.data) {
        return -ENOMEM;
    }
    track->udta.size = 0;
    track->udta.capacity = 256 * 1024;

    track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION;
    track->tfhd.default_sample_duration = track->default_duration = sample_duration;
    if (MOV_OBJECT_AAC == object && extra_data_size > 0)
        writer->aac_profile = (((uint8_t*)extra_data)[0] >> 3) & 0x1F;
    if (0 != mov_add_audio(
                 track, &mov->mvhd, sample_rate, object, channel_count, bits_per_sample, sample_rate, extra_data,
                 extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return track_index;
}

int fmp4_writer_update_audio(
    struct fmp4_writer_t* writer, int track_index, uint8_t object, int channel_count, int bits_per_sample,
    int sample_rate, int sample_duration, int bitrate, const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;

    if (track_index >= mov->track_count) {
        return -1;
    }
    struct mov_track_t* track = &mov->tracks[track_index];
    uint32_t saved_track_ID = mov->mvhd.next_track_ID;
    mov->mvhd.next_track_ID =
        track->tkhd.track_ID;  // mov_add_audio() will assign next_track_ID to track_ID of this track
    mov_free_track(track);
    track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
    if (NULL == track->stsd.entries)
        return -ENOMEM;

    track->stsd.current = track->stsd.entries;
    track->bitrate = bitrate;

    track->samples_buffer = realloc(track->samples_buffer, 256 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 256 * 1024;

    track->udta.data = realloc(track->udta.data, 256 * 1024);
    if (NULL == track->udta.data) {
        return -ENOMEM;
    }
    track->udta.size = 0;
    track->udta.capacity = 256 * 1024;

    track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION;
    track->tfhd.default_sample_duration = track->default_duration = sample_duration;
    if (MOV_OBJECT_AAC == object && extra_data_size > 0)
        writer->aac_profile = (((uint8_t*)extra_data)[0] >> 3) & 0x1F;
    if (0 != mov_add_audio(
                 track, &mov->mvhd, sample_rate, object, channel_count, bits_per_sample, sample_rate, extra_data,
                 extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID = saved_track_ID;
    return 0;
}

int fmp4_writer_add_drm_audio(
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    int sample_duration, int bitrate, drm_info_t* drm, const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;
    int track_index = find_track_by_type(mov, MOV_AUDIO);
    struct mov_track_t* track = track_index != -1 ? &mov->tracks[track_index] : NULL;
    uint32_t saved_track_ID = mov->mvhd.next_track_ID;
    if (track_index == -1) {
        track = mov_add_track(mov);
        if (NULL == track)
            return -ENOMEM;
        track_index = mov->track_count++;
    } else {
        uint32_t track_ID = track->tkhd.track_ID;
        mov_free_track(track);
        track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
        if (NULL == track->stsd.entries)
            return -ENOMEM;

        track->stsd.current = track->stsd.entries;
        mov->mvhd.next_track_ID = track_ID;
    }
    track->bitrate = bitrate;

    track->samples_buffer = realloc(track->samples_buffer, 256 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 256 * 1024;

    track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION;
    track->tfhd.default_sample_duration = track->default_duration = sample_duration;
    if (MOV_OBJECT_AAC == object && extra_data_size > 0)
        writer->aac_profile = (((uint8_t*)extra_data)[0] >> 3) & 0x1F;

    // init drm track & mov
    if (NULL != drm && drm->is_drm && fmp4_write_init_drm(mov, track, drm) != 0) {
        return -ENOMEM;
    }

    if (0 != mov_add_audio(
                 track, &mov->mvhd, sample_rate, object, channel_count, bits_per_sample, sample_rate, extra_data,
                 extra_data_size))
        return -ENOMEM;

    if (saved_track_ID == mov->mvhd.next_track_ID)
        mov->mvhd.next_track_ID++;
    else
        mov->mvhd.next_track_ID = saved_track_ID;
    return track_index;
}

int fmp4_extra_data_copy(struct fmp4_writer_t* writer, struct mov_reader_t* reader, int is_video) {
    if (is_video == 1) {
        writer->mov.v_extra_size = mov_video_extra_data(reader, writer->mov.v_extra_data, writer->mov.v_extra_cap);
        writer->mov.v_extra_cap =
            writer->mov.v_extra_size > writer->mov.v_extra_cap ? writer->mov.v_extra_size : writer->mov.v_extra_cap;
    } else {
        writer->mov.a_extra_size = mov_audio_extra_data(reader, writer->mov.a_extra_data, writer->mov.a_extra_cap);
        writer->mov.a_extra_cap =
            writer->mov.a_extra_size > writer->mov.v_extra_cap ? writer->mov.a_extra_size : writer->mov.a_extra_cap;
    }
    return 0;
}

int fmp4_writer_dv_config(fmp4_writer_t* fmp4, struct dovi_config_t config) {
    struct mov_t* mov = &fmp4->mov;
    mov->is_dovi = 1;
    mov->dv_config.dv_version_major = config.dv_version_major;
    mov->dv_config.dv_version_minor = config.dv_version_minor;
    mov->dv_config.dv_profile = config.dv_profile;
    mov->dv_config.dv_level = config.dv_level;
    mov->dv_config.rpu_present_flag = config.rpu_present_flag;
    mov->dv_config.el_present_flag = config.el_present_flag;
    mov->dv_config.bl_present_flag = config.bl_present_flag;
    mov->dv_config.dv_bl_signal_compatibility_id = config.dv_bl_signal_compatibility_id;
    return 0;
}

int fmp4_writer_eac3_config(fmp4_writer_t* fmp4, struct eac3_config_t config, int codecid) {
    struct mov_t* mov = &fmp4->mov;
    if (codecid == MOV_OBJECT_EAC3) {
        mov->da_config.data_rate = config.data_rate;
        mov->da_config.num_ind_sub = config.num_ind_sub;
        mov->da_config.fscod = config.fscod;
        mov->da_config.bsid = config.bsid;
        mov->da_config.bsmod = config.bsmod;
        mov->da_config.acmod = config.acmod;
        mov->da_config.lfeon = config.lfeon;
        mov->da_config.fscod = config.fscod;
        mov->da_config.bsid = config.bsid;
        mov->da_config.asvc = config.asvc;
        mov->da_config.bsmod = config.bsmod;
        mov->da_config.acmod = config.acmod;
        mov->da_config.lfeon = config.lfeon;
        mov->da_config.num_dep_sub = config.num_dep_sub;
        mov->da_config.chan_loc = config.chan_loc;
    } else if (codecid == MOV_OBJECT_AC3) {
        mov->da_config.fscod = config.fscod;
        mov->da_config.bsid = config.bsid;
        mov->da_config.bsmod = config.bsmod;
        mov->da_config.acmod = config.acmod;
        mov->da_config.lfeon = config.lfeon;
        mov->da_config.bit_rate_code = config.bit_rate_code;
    } else {
        return -1;
    }
    return 0;
}

int fmp4_writer_add_video(
    struct fmp4_writer_t* writer, uint8_t object, int width, int height, int bitrate, double fps,
    const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;
    int track_index = find_track_by_type(mov, MOV_VIDEO);
    struct mov_track_t* track = track_index != -1 ? &mov->tracks[track_index] : NULL;
    uint32_t saved_track_ID = mov->mvhd.next_track_ID;
    if (track_index == -1) {
        track = mov_add_track(mov);
        if (NULL == track)
            return -ENOMEM;
        track_index = mov->track_count++;
    } else {
        uint32_t track_ID = track->tkhd.track_ID;
        mov_free_track(track);
        track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
        if (NULL == track->stsd.entries)
            return -ENOMEM;

        track->stsd.current = track->stsd.entries;
        mov->mvhd.next_track_ID = track_ID;
    }
    track->bitrate = bitrate;
    track->fps = fps;

    track->samples_buffer = realloc(track->samples_buffer, 768 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 768 * 1024;

    track->udta.data = realloc(track->udta.data, 256 * 1024);
    if (NULL == track->udta.data) {
        return -ENOMEM;
    }
    track->udta.size = 0;
    track->udta.capacity = 256 * 1024;

    if (0 != mov_add_video(track, &mov->mvhd, mov->mvhd.timescale, object, width, height, extra_data, extra_data_size))
        return -ENOMEM;

    if (saved_track_ID == mov->mvhd.next_track_ID)
        mov->mvhd.next_track_ID++;
    else
        mov->mvhd.next_track_ID = saved_track_ID;
    return track_index;
}

int fmp4_writer_add_drm_video(
    struct fmp4_writer_t* writer, uint8_t object, int width, int height, int bitrate, double fps, drm_info_t* drm,
    const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov = &writer->mov;
    int track_index = find_track_by_type(mov, MOV_VIDEO);
    struct mov_track_t* track = track_index != -1 ? &mov->tracks[track_index] : NULL;
    uint32_t saved_track_ID = mov->mvhd.next_track_ID;
    if (track_index == -1) {
        track = mov_add_track(mov);
        if (NULL == track)
            return -ENOMEM;
        track_index = mov->track_count++;
    } else {
        uint32_t track_ID = track->tkhd.track_ID;
        mov_free_track(track);
        track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
        if (NULL == track->stsd.entries)
            return -ENOMEM;

        track->stsd.current = track->stsd.entries;
        mov->mvhd.next_track_ID = track_ID;
    }
    track->bitrate = bitrate;
    track->fps = fps;

    track->samples_buffer = realloc(track->samples_buffer, 768 * 1024);
    if (NULL == track->samples_buffer)
        return -ENOMEM;
    track->samples_capacity = 768 * 1024;

    // init drm track & mov
    if (NULL != drm && drm->is_drm && fmp4_write_init_drm(mov, track, drm) != 0) {
        return -ENOMEM;
    }

    if (0 != mov_add_video(track, &mov->mvhd, mov->mvhd.timescale, object, width, height, extra_data, extra_data_size))
        return -ENOMEM;

    if (saved_track_ID == mov->mvhd.next_track_ID)
        mov->mvhd.next_track_ID++;
    else
        mov->mvhd.next_track_ID = saved_track_ID;
    return track_index;
}

size_t fmp4_writer_samples_capacity(fmp4_writer_t* writer) {
    struct mov_t* mov = &writer->mov;
    size_t size = 0;
    int i;
    for (i = 0; i < mov->track_count; i++)
        size += mov->tracks[i].samples_capacity;
    return size;
}

size_t fmp4_writer_segment_size(fmp4_writer_t* writer) {
    return fmp4_get_moof_size(&writer->mov) + 8 + writer->mdat_size;
}
#else
int fmp4_writer_add_audio(
    struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate,
    const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 !=
        mov_add_audio(
            track, &mov->mvhd, 1000, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}

int fmp4_writer_add_video(
    struct fmp4_writer_t* writer, uint8_t object, int width, int height, const void* extra_data,
    size_t extra_data_size) {
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mov_add_video(track, &mov->mvhd, 1000, object, width, height, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}
#endif

int fmp4_writer_add_subtitle(
    struct fmp4_writer_t* writer, uint8_t object, const void* extra_data, size_t extra_data_size) {
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mov_add_subtitle(track, &mov->mvhd, 1000, object, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}

#ifdef _BILI_FIXED_
int fmp4_writer_clear_tracks(fmp4_writer_t* fmp4) {
    for (int i = 0; i < fmp4->mov.track_count; i++)
        mov_free_track(fmp4->mov.tracks + i);
    fmp4->mov.track_count = 0;
    return 0;
}
#endif

#ifdef _BILI_FIXED_
int fmp4_writer_save_segment(
    fmp4_writer_t* writer, const struct mov_buffer_t* buffer, void* param, int64_t next_video_dts, uint32_t fragment_id)
#else
int fmp4_writer_save_segment(fmp4_writer_t* writer, uint32_t fragment_id)
#endif
{
    int i;
    struct mov_t* mov;
    mov = &writer->mov;
#ifdef _BILI_FIXED_
    mov->io.param = param;
    memcpy(&mov->io.io, buffer, sizeof(mov->io.io));

    // flush fragment
    fmp4_write_fragment(writer, next_video_dts, fragment_id);
#else
    fmp4_write_fragment(writer);
#endif
    writer->has_moov = 0;  // clear moov flags

#ifndef _BILI_FIXED_
    // write mfra
    if (0 == (mov->flags & MOV_FLAG_SEGMENT)) {
        fmp4_write_mfra(mov);
        for (i = 0; i < mov->track_count; i++)
            mov->tracks[i].frag_count = 0;  // don't free frags memory
    }
#endif

    return mov_buffer_error(&mov->io);
}

#ifdef _BILI_FIXED_
int fmp4_writer_init_segment(fmp4_writer_t* writer, const struct mov_buffer_t* buffer, void* param)
#else
int fmp4_writer_init_segment(fmp4_writer_t* writer)
#endif
{
    struct mov_t* mov;
    mov = &writer->mov;
#ifdef _BILI_FIXED_
    mov->io.param = param;
    memcpy(&mov->io.io, buffer, sizeof(mov->io.io));
#endif

    mov_write_ftyp(mov);
    fmp4_write_moov(mov);
    return mov_buffer_error(&mov->io);
}

void fmp4_writer_reset_track(fmp4_writer_t* writer, int idx) {
    writer->mov.tracks[idx].start_dts = INT64_MIN;
    writer->mov.tracks[idx].total_duration = 0;
    writer->mov.tracks[idx].sample_count = 0;
    writer->mdat_size = 0;
}