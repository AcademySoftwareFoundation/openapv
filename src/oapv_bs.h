/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright owner, nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _OAPV_BSR_H_
#define _OAPV_BSR_H_

#include "oapv_port.h"

typedef struct oapv_bs oapv_bs_t;
typedef int (*oapv_bs_fn_flush_t)(oapv_bs_t *bs, int byte);

struct oapv_bs {
    u32                code;     // intermediate code buffer
    int                leftbits; // left bits count in code
    u8                *cur;      // address of current bitstream position
    u8                *end;      // address of bitstream end
    u8                *beg;      // address of bitstream begin
    u32                size;     // size of input bitstream in byte
    oapv_bs_fn_flush_t fn_flush; // function pointer for flush operation
    int                ndata[4]; // arbitrary data, if needs
    void              *pdata[4]; // arbitrary address, if needs
};

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

static inline bool bsw_is_align8(oapv_bs_t *bs)
{
    return (bool)(!((bs)->leftbits & 0x7));
}

static inline int bsw_get_write_byte(oapv_bs_t *bs)
{
    return (int)((u8 *)(bs->cur) - (u8 *)(bs->beg));
}

void oapv_bsw_init(oapv_bs_t *bs, u8 *buf, int size, oapv_bs_fn_flush_t fn_flush);
void oapv_bsw_deinit(oapv_bs_t *bs);
void *oapv_bsw_sink(oapv_bs_t *bs);
int oapv_bsw_write_direct(void *addr, u32 val, int len);
int oapv_bsw_write1(oapv_bs_t *bs, int val);
int oapv_bsw_write(oapv_bs_t *bs, u32 val, int len);
///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
#if 0
#if defined(X86F) || defined(ARMV8N_64)
/* on X86 machine, 32-bit shift means remaining of original value, so we
should set zero in that case. */
#define BSR_SKIP_CODE(bs, size) \
    oapv_assert((bs)->leftbits >= (size)); \
    if((size) == 32) {(bs)->code = 0; (bs)->leftbits = 0;} \
    else           {(bs)->code <<= (size); (bs)->leftbits -= (size);}
#else
#define BSR_SKIP_CODE(bs, size) \
    oapv_assert((bs)->leftbits >= (size)); \
    (bs)->code <<= (size); (bs)->leftbits -= (size);
#endif
#else
#define BSR_SKIP_CODE(bs, size) \
    oapv_assert((bs)->leftbits >= (size) && (size) <= 32); \
    (bs)->code <<= (size); (bs)->leftbits -= (size);
#endif

/*! Is end of bitstream ? */
#define BSR_IS_EOB(bs) (((bs)->cur >= (bs)->end && (bs)->leftbits==0)? 1: 0)

/*! Is bitstream byte aligned? */
#define BSR_IS_BYTE_ALIGN(bs) ((((bs)->leftbits & 0x7) == 0)? 1: 0)

/*! Is last byte of bitstream? */
#define BSR_IS_LAST_BYTE(bs) \
    (((bs)->cur >= (bs)->end && bs->leftbits > 0 && (bs)->leftbits <= 8)? 1: 0)

/* get left byte count in BS */
#define BSR_GET_LEFT_BYTE(bs) \
    ((int)((bs)->end - (bs)->cur) + ((bs)->leftbits >> 3))
/* get number of byte consumed */
#define BSR_GET_READ_BYTE(bs) \
    ((int)((bs)->cur - (bs)->beg) - ((bs)->leftbits >> 3))
/* get number of bit consumed */
#define BSR_GET_READ_BIT(bs) \
    (((int)((bs)->cur - (bs)->beg) << 3) - ((bs)->leftbits))

/* get address of current reading */
#define BSR_GET_CUR(bs) ((bs)->cur - (((bs)->leftbits + 7) >> 3))

/* move to # bytes align position */
#define BSR_MOVE_BYTE_ALIGN(bs, byte) \
    (bs)->cur += (byte) - ((bs)->leftbits >> 3); \
    (bs)->code = 0; \
    (bs)->leftbits = 0;

void oapv_bsr_init(oapv_bs_t *bs, u8 *buf, u32 size, oapv_bs_fn_flush_t fn_flush);
int oapv_bsr_clz_in_code(u32 code);
int oapv_bsr_clz(oapv_bs_t *bs);
void oapv_bsr_align8(oapv_bs_t *bs);
void oapv_bsr_skip(oapv_bs_t *bs, int size);
u32 oapv_bsr_peek(oapv_bs_t *bs, int size);
void *oapv_bsr_sink(oapv_bs_t *bs);
void oapv_bsr_move(oapv_bs_t *bs, u8 *pos);
u32 oapv_bsr_read(oapv_bs_t *bs, int size);
int oapv_bsr_read1(oapv_bs_t *bs);
u32 oapv_bsr_read_direct(void *addr, int len);

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

#endif /* _OAPV_BSR_H_ */
