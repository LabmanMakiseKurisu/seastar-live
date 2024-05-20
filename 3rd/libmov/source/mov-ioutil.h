#ifndef _mov_ioutil_h_
#define _mov_ioutil_h_

#include "mov-buffer.h"

struct mov_ioutil_t {
    struct mov_buffer_t io;
    void* param;
    int error;
};

static inline int mov_buffer_error(const struct mov_ioutil_t* io) {
    return io->error;
}

static inline uint64_t mov_buffer_tell(const struct mov_ioutil_t* io) {
    return io->io.tell(io->param);
}

static inline void mov_buffer_seek(
    const struct mov_ioutil_t* io, uint64_t offset) {
    //	if (0 == io->error)
    ((struct mov_ioutil_t*)io)->error = io->io.seek(io->param, offset);
}

static inline void mov_buffer_skip(struct mov_ioutil_t* io, uint64_t bytes) {
    uint64_t offset;
    if (0 == io->error) {
        offset = io->io.tell(io->param);
        io->error = io->io.seek(io->param, offset + bytes);
    }
}

static inline void mov_buffer_read(
    struct mov_ioutil_t* io, void* data, uint64_t bytes) {
    if (0 == io->error)
        io->error = io->io.read(io->param, data, bytes);
}

static inline void mov_buffer_write(
    const struct mov_ioutil_t* io, const void* data, uint64_t bytes) {
    if (0 == io->error)
        ((struct mov_ioutil_t*)io)->error =
            io->io.write(io->param, data, bytes);
}

static inline uint8_t mov_buffer_r8(struct mov_ioutil_t* io) {
    uint8_t v = 0;
    mov_buffer_read(io, &v, 1);
    return v;
}

#ifndef _BILI_FIXED_
#define _BILI_FIXED_
#endif
#ifdef _BILI_FIXED_
#define _SWAP_U16_(v) ((((v)&0x00FF) << 8) | (((v)&0xFF00) >> 8))
#define _SWAP_U32_(v) ((_SWAP_U16_(v) << 16) | (_SWAP_U16_(v >> 16)))
#define _SWAP_U64_(v) ((_SWAP_U32_(v) << 32) | (_SWAP_U32_(v >> 32)))

static inline uint16_t mov_buffer_r16(struct mov_ioutil_t* io) {
    uint16_t v = 0;
    mov_buffer_read(io, &v, 2);
    return _SWAP_U16_(v);
}

static inline uint32_t mov_buffer_r24(struct mov_ioutil_t* io) {
    uint32_t v = 0;
    mov_buffer_read(io, &v, 3);
    return _SWAP_U32_(v) >> 8;
}

static inline uint32_t mov_buffer_r32(struct mov_ioutil_t* io) {
    uint32_t v = 0;
    mov_buffer_read(io, &v, 4);
    return _SWAP_U32_(v);
}

static inline uint64_t mov_buffer_r64(struct mov_ioutil_t* io) {
    uint64_t v;
    mov_buffer_read(io, &v, 8);
    return _SWAP_U64_(v);
}

static inline void mov_buffer_w8(const struct mov_ioutil_t* io, uint8_t v) {
    mov_buffer_write(io, &v, 1);
}

static inline void mov_buffer_w16(const struct mov_ioutil_t* io, uint16_t v) {
    v = _SWAP_U16_(v);
    mov_buffer_write(io, &v, 2);
}

static inline void mov_buffer_w24(const struct mov_ioutil_t* io, uint32_t v) {
    v = _SWAP_U32_(v) >> 8;
    mov_buffer_write(io, &v, 3);
}

static inline void mov_buffer_w32(const struct mov_ioutil_t* io, uint32_t v) {
    v = _SWAP_U32_(v);
    mov_buffer_write(io, &v, 4);
}

static inline void mov_buffer_w64(const struct mov_ioutil_t* io, uint64_t v) {
    v = _SWAP_U64_(v);
    mov_buffer_write(io, &v, 8);
}
#else
static inline uint16_t mov_buffer_r16(struct mov_ioutil_t* io) {
    uint16_t v;
    v = mov_buffer_r8(io) << 8;
    v |= mov_buffer_r8(io);
    return v;
}

static inline uint32_t mov_buffer_r24(struct mov_ioutil_t* io) {
    uint32_t v;
    v = mov_buffer_r8(io) << 16;
    v |= mov_buffer_r16(io);
    return v;
}

static inline uint32_t mov_buffer_r32(struct mov_ioutil_t* io) {
    uint32_t v;
    v = mov_buffer_r16(io) << 16;
    v |= mov_buffer_r16(io);
    return v;
}

static inline uint64_t mov_buffer_r64(struct mov_ioutil_t* io) {
    uint64_t v;
    v = ((uint64_t)mov_buffer_r32(io)) << 32;
    v |= mov_buffer_r32(io);
    return v;
}

static inline void mov_buffer_w8(const struct mov_ioutil_t* io, uint8_t v) {
    mov_buffer_write(io, &v, 1);
}

static inline void mov_buffer_w16(const struct mov_ioutil_t* io, uint16_t v) {
    mov_buffer_w8(io, (uint8_t)(v >> 8));
    mov_buffer_w8(io, (uint8_t)v);
}

static inline void mov_buffer_w24(const struct mov_ioutil_t* io, uint32_t v) {
    mov_buffer_w16(io, (uint16_t)(v >> 8));
    mov_buffer_w8(io, (uint8_t)v);
}

static inline void mov_buffer_w32(const struct mov_ioutil_t* io, uint32_t v) {
    mov_buffer_w16(io, (uint16_t)(v >> 16));
    mov_buffer_w16(io, (uint16_t)v);
}

static inline void mov_buffer_w64(const struct mov_ioutil_t* io, uint64_t v) {
    mov_buffer_w32(io, (uint32_t)(v >> 32));
    mov_buffer_w32(io, (uint32_t)v);
}
#endif

#endif /* !_mov_ioutil_h_ */
