/*
 * @Author: Amadeus
 * @Date: 2024-04-22 13:38:13
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 13:38:16
 * @FilePath: /Amadeus/src/util/codec.hh
 * @Description: 
 */
#pragma once

#include <seastar/core/sstring.hh>

struct mpeg4_aac_t;
struct mpeg4_avc_t;
struct mpeg4_hevc_t;
struct aom_av1_t;

namespace amadeus {
namespace codec {

using namespace seastar;

sstring mpeg4_aac_codecs(const mpeg4_aac_t *aac);
sstring mpeg4_avc_codecs(const mpeg4_avc_t *avc);
sstring mpeg4_hevc_codecs(const mpeg4_hevc_t *hevc);
sstring aom_av1_codecs(const aom_av1_t *av1);

int mpeg4_avc_rect_load(struct mpeg4_avc_t *avc, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height);
int mpeg4_hevc_rect_load(struct mpeg4_hevc_t *hevc, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height);

void copy(const mpeg4_avc_t *from, mpeg4_avc_t *to);
void copy(const mpeg4_hevc_t *from, mpeg4_hevc_t *to);
void copy(const aom_av1_t *from, aom_av1_t *to);

} // namespace codec

} // namespace com

std::ostream &operator<<(std::ostream &os, const mpeg4_aac_t *v);
std::ostream &operator<<(std::ostream &os, const aom_av1_t *v);
std::ostream &operator<<(std::ostream &os, const mpeg4_avc_t *v);
std::ostream &operator<<(std::ostream &os, const mpeg4_hevc_t *v);

static inline std::ostream &
operator<<(std::ostream &os, const mpeg4_aac_t &v) {
    return os << &v;
}

static inline std::ostream &
operator<<(std::ostream &os, const aom_av1_t &v) {
    return os << &v;
}

static inline std::ostream &
operator<<(std::ostream &os, const mpeg4_avc_t &v) {
    return os << &v;
}

static inline std::ostream &
operator<<(std::ostream &os, const mpeg4_hevc_t &v) {
    return os << &v;
}
