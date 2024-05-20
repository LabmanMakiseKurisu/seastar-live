/*
 * @Author: kay
 * @Date: 2021-11-02 11:00:37
 * @Last Modified by: likai
 * @Last Modified time: 2022-02-18 20:40:21
 */

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef _EAC3_HEADER_H_
#define _EAC3_HEADER_H_

#include "mov-atom.h"
#include <stdint.h>

struct eac3_header_t {
    int32_t data_rate;
    int32_t sample_rate;
    int32_t frame_size;
    int32_t numblkscod;
    int32_t fscod;
    int32_t bsid;
    int32_t bsmod;
    int32_t acmod;
    int32_t lfeon;
    int32_t channels;
    int32_t numblk;
    int32_t is_atmos;
};

static int eac3_data_rate[19] = { 32,  40,  48,  56,  64,  80,  96,
                                  112, 128, 160, 192, 224, 256, 320,
                                  384, 448, 512, 576, 640 };

static int eac3_blocks[] = { 1, 2, 3, 6 };

static const uint16_t eac3_frame_size_tab[38][3] = {
    { 64, 69, 96 },       { 64, 70, 96 },       { 80, 87, 120 },
    { 80, 88, 120 },      { 96, 104, 144 },     { 96, 105, 144 },
    { 112, 121, 168 },    { 112, 122, 168 },    { 128, 139, 192 },
    { 128, 140, 192 },    { 160, 174, 240 },    { 160, 175, 240 },
    { 192, 208, 288 },    { 192, 209, 288 },    { 224, 243, 336 },
    { 224, 244, 336 },    { 256, 278, 384 },    { 256, 279, 384 },
    { 320, 348, 480 },    { 320, 349, 480 },    { 384, 417, 576 },
    { 384, 418, 576 },    { 448, 487, 672 },    { 448, 488, 672 },
    { 512, 557, 768 },    { 512, 558, 768 },    { 640, 696, 960 },
    { 640, 697, 960 },    { 768, 835, 1152 },   { 768, 836, 1152 },
    { 896, 975, 1344 },   { 896, 976, 1344 },   { 1024, 1114, 1536 },
    { 1024, 1115, 1536 }, { 1152, 1253, 1728 }, { 1152, 1254, 1728 },
    { 1280, 1393, 1920 }, { 1280, 1394, 1920 },
};

static int eac3_sample_rate[] = { 48000, 44100, 32000, 0 };

// acmod
static int eac3_nfchans[] = {
    2,  // Ch1 Ch2
    1,  // C
    2,  // L,R
    3,  // L,C,R
    3,  // L,R,S
    4,  // L,C,R,S
    4,  // L,R,SL,SR
    5   // L,C,R,SL,SR
};

int eac3_header_load(struct eac3_header_t* eac3, const void* data, int bytes);

#endif

#if defined(__cplusplus)
}
#endif