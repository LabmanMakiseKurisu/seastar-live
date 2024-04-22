/*
 * @Author: kay
 * @Date: 2021-11-02 11:00:40
 * @Last Modified by: likai
 * @Last Modified time: 2022-02-18 20:40:35
 */

#include "eac3-header.h"
#include "bitstream.h"
#include <assert.h>

#define _SWAP_U16_(v) ((((v)&0x00FF) << 8) | (((v)&0xFF00) >> 8))
#define _SWAP_U32_(v) ((_SWAP_U16_(v) << 16) | (_SWAP_U16_(v >> 16)))
#define _SWAP_U64_(v) ((_SWAP_U32_(v) << 32) | (_SWAP_U32_(v >> 32)))

int eac3_header_load(struct eac3_header_t* eac3, const void* data, int bytes) {
    bitstream_t stream;
    bitstream_init(&stream, data, bytes);

    int16_t syncword = bitstream_read_bits(&stream, 16);
    if (syncword != 0x0B77) {
        return -1;
    }
    int32_t bsid = bitstream_next_bits(&stream, 29) & 0x1F;
    eac3->numblk = 6;
    if (bsid <= 10) {
        // AC-3
        int16_t crc1 = bitstream_read_bits(&stream, 16);
        eac3->fscod = bitstream_read_bits(&stream, 2);
        int8_t frmsizecod = bitstream_read_bits(&stream, 6);
        // skip bsid
        bitstream_read_bits(&stream, 5);

        if (frmsizecod > 37) {
            return -1;
        }
        eac3->bsmod = bitstream_read_bits(&stream, 3);
        eac3->acmod = bitstream_read_bits(&stream, 3);
        // skip dobly surround mode
        bitstream_read_bits(&stream, 6);
        eac3->lfeon = bitstream_read_bit(&stream);
        int sr_shift = (bsid > 8 ? bsid : 8) - 8;
        eac3->sample_rate = eac3_sample_rate[eac3->fscod] >> sr_shift;
        eac3->data_rate = eac3_data_rate[frmsizecod >> 1] * 1000 >> sr_shift;
        eac3->channels = eac3_nfchans[eac3->acmod] + eac3->lfeon;
        eac3->frame_size = eac3_frame_size_tab[frmsizecod][eac3->fscod] * 2;
    } else {
        // EAC-3
        bitstream_read_bits(&stream, 5);
        int sr_shift = 0;
        eac3->frame_size = (bitstream_read_bits(&stream, 11) + 1) << 1;
        if (eac3->frame_size < 7) {
            return -1;
        }
        eac3->fscod = bitstream_read_bits(&stream, 2);
        if (eac3->fscod == 3) {
            int fscod2 = bitstream_read_bits(&stream, 2);
            if (fscod2 == 3) {
                return -1;
            }
            eac3->sample_rate = eac3_sample_rate[fscod2] / 2;
            sr_shift = 1;
        } else {
            eac3->numblk = eac3_blocks[bitstream_read_bits(&stream, 2)];
            eac3->sample_rate = eac3_sample_rate[eac3->fscod];
            sr_shift = 0;
        }
        eac3->acmod = bitstream_read_bits(&stream, 3);
        eac3->lfeon = bitstream_read_bit(&stream);
        eac3->data_rate =
            8 * eac3->frame_size * eac3->sample_rate / (eac3->numblk * 256);
        eac3->channels = eac3_nfchans[eac3->acmod] + eac3->lfeon;
    }
    eac3->bsid = bsid;

    // FIXME: Dolby Atmos or Dolby Digital according to the documents, but I can't get correct result:(
    /*
    int dialnorm = bitstream_read_bits(&stream, 5);
    int compre = bitstream_read_bit(&stream);
    if (compre) {
        // compr
        bitstream_read_bits(&stream, 8);
    }
    int langcode = bitstream_read_bit(&stream);
    if (langcode) {
        bitstream_read_bits(&stream, 8);
    }
    int audprodie = bitstream_read_bit(&stream);
    if (audprodie) {
        bitstream_read_bits(&stream, 7);
    }
    else {
        bitstream_read_bits(&stream, 5);
        int compr2e = bitstream_read_bit(&stream);
        if (compr2e) {
            bitstream_read_bits(&stream, 8);
        }
        int langcod2e = bitstream_read_bit(&stream);
        if (langcod2e) {
            bitstream_read_bits(&stream, 8);
        }
        int audprodi2e = bitstream_read_bit(&stream);
        if (audprodi2e) {
            bitstream_read_bits(&stream, 7);
        }
    }
    bitstream_read_bits(&stream, 2);
    int timecod1e = bitstream_read_bit(&stream);
    if (timecod1e) {
        bitstream_read_bits(&stream, 14);
    }
    int timecod2e = bitstream_read_bit(&stream);
    if (timecod2e) {
        bitstream_read_bits(&stream, 14);
    }
    int addbsie = bitstream_read_bit(&stream);
    if (addbsie) {
        int addbsil = bitstream_read_bits(&stream, 6);
        //  retrieve first byte
        int addbsi = 0;
        for (int i = 0; i < addbsil + 1; i++) {
            addbsi = bitstream_read_bits(&stream, 8);
        }
        eac3->is_atmos = addbsi & 0x01;
    } else {
        eac3->is_atmos = 0;
    }
    */
    eac3->is_atmos = 1;
    return 0;
}
