#ifndef _h265_vps_h_
#define _h265_vps_h_

#include <stdint.h>

#include "bitstream.h"

#if defined(__cplusplus)
extern "C" {
#endif
struct h265_sub_layer_t {
    uint8_t sub_layer_profile_present_flag;        // u(1)
    uint8_t sub_layer_level_present_flag;          // u(1)
    uint8_t sub_layer_profile_space;               // u(2)
    uint8_t sub_layer_tier_flag;                   // u(1)
    uint8_t sub_layer_profile_idc;                 // u(5)
    uint32_t sub_layer_profile_compatibility_flag; // u(32)
    uint64_t sub_layer_constraint_flags;           // u(48)
    uint8_t sub_layer_level_idc;                   // u(8)
};

struct h265_profile_tier_level_t {
    uint8_t general_profile_space;               // u(2)
    uint8_t general_tier_flag;                   // u(1)
    uint8_t general_profile_idc;                 // u(5)
    uint32_t general_profile_compatibility_flag; // u(32)
    uint64_t general_constraint_flags;           // u(48)
    uint8_t general_level_idc;                   // u(8)

    struct h265_sub_layer_t sub_layer[1 << 6]; // maxNumSubLayersMinus1
};

struct h265_vps_t {
    uint8_t vps_video_parameter_set_id;    // u(4)
    uint8_t vps_base_layer_internal_flag;  // u(1)
    uint8_t vps_base_layer_available_flag; // u(1)
    uint8_t vps_max_layers_minus1;         // u(6)
    uint8_t vps_max_sub_layers_minus1;     // u(3)
    uint8_t vps_temporal_id_nesting_flag;  // u(1)

    struct h265_profile_tier_level_t profile;
};

int h265_vps(bitstream_t *stream, struct h265_vps_t *vps);

int h265_profile_tier_level(
    bitstream_t *stream, struct h265_profile_tier_level_t *profile, int profilePresentFlag, int maxNumSubLayersMinus1);

#if defined(__cplusplus)
}
#endif
#endif /* !_h265_vps_h_ */
