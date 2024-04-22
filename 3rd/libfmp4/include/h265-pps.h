#ifndef _h265_pps_h_
#define _h265_pps_h_

#include <stdint.h>
#include "bitstream.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct h265_pps_t {
    uint32_t pps_pic_parameter_set_id;  // ue
    uint32_t pps_seq_parameter_set_id;  // ue
};

int h265_pps(bitstream_t* stream, struct h265_pps_t* pps);

#if defined(__cplusplus)
}
#endif
#endif /* !_h265_pps_h_ */
