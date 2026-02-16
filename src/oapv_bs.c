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

#include "oapv_def.h"

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////
/* number of bytes to be sunk */
#define BSW_GET_SINK_BYTE(bs) ((64 - (bs)->leftbits + 7) >> 3)

static int bsw_flush(oapv_bs_t *bs, int bytes)
{
    if(bytes == 0)
        bytes = BSW_GET_SINK_BYTE(bs);

    oapv_assert_rv(bs->cur + bytes <= bs->end, -1);

    while(bytes--) {
        *bs->cur++ = (bs->code >> 56) & 0xFF;
        bs->code <<= 8;
    }

    bs->leftbits = 64;

    return 0;
}

void oapv_bsw_init(oapv_bs_t *bs, u8 *buf, int size, oapv_bs_fn_flush_t fn_flush)
{
    bs->size = size;
    bs->beg = buf;
    bs->cur = buf;
    bs->end = buf + size;
    bs->code = 0;
    bs->leftbits = 64;
    bs->fn_flush = (fn_flush == NULL ? bsw_flush : fn_flush);
}

void oapv_bsw_deinit(oapv_bs_t *bs)
{
    bs->fn_flush(bs, 0);
}

void *oapv_bsw_sink(oapv_bs_t *bs)
{
    oapv_assert_rv(bs->cur + BSW_GET_SINK_BYTE(bs) < bs->end, NULL);
    bs->fn_flush(bs, 0);
    bs->code = 0;
    bs->leftbits = 64;
    return (void *)bs->cur;
}

int oapv_bsw_write_direct(void *addr, u32 val, int len)
{
    int            i;
    unsigned char *p = (unsigned char *)addr;

    oapv_assert_rv((len & 0x7) == 0, -1); // len should be byte-aligned

    val <<= (32 - len);
    for(i = 0; i < (len >> 3); i++) {
        p[i] = (val >> 24) & 0xFF;
        val <<= 8;
    }
    return 0;
}

int oapv_bsw_write1(oapv_bs_t *bs, int val)
{
    oapv_assert(bs);

    bs->leftbits--;
    bs->code |= ((u64)(val & 0x1) << bs->leftbits);

    if(bs->leftbits == 0) {
        oapv_assert_rv(bs->cur < bs->end, -1);
        bs->fn_flush(bs, 0);

        bs->code = 0;
        bs->leftbits = 64;
    }

    return 0;
}

int oapv_bsw_write(oapv_bs_t *bs, u32 val, int len)
{
    int leftbits;
    u64 code_t;

    oapv_assert(bs);

    leftbits = bs->leftbits;
    code_t = ((u64)val) << (64 - len);
    bs->code |= (code_t >> (64 - leftbits));

    if(len < leftbits) {
        bs->leftbits -= len;
    }
    else {
        bs->fn_flush(bs, 8);
        bs->code = code_t << leftbits;
        bs->leftbits = 64 - (len - leftbits);
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

/* Table of count of leading zero for 4 bit value */
static const u8 tbl_zero_count4[16] = {
    4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0
};

// skip code if lefbits are larger than skip bit count;
static void inline bsr_skip_code(oapv_bs_t *bs, int size)
{
    oapv_assert(size <= 32);
    oapv_assert(bs->leftbits >= size);
    if(size == 64) {
        bs->code = 0;
        bs->leftbits = 0;
    }
    else {
        bs->code <<= size;
        bs->leftbits -= size;
    }
}

static int bsr_flush(oapv_bs_t *bs, int byte)
{
    int shift = 56, remained;
    u64 code = 0;

    oapv_assert(byte);

    remained = (int)(bs->end - bs->cur);
    if(byte > remained)
        byte = remained;

    if(byte <= 0) {
        bs->code = 0;
        bs->leftbits = 0;
        return -1;
    }

    bs->leftbits = byte << 3;

    while(byte) {
        code |= (u64)(*(bs->cur++)) << shift;
        byte--;
        shift -= 8;
    }
    bs->code = code;
    return 0;
}

void oapv_bsr_init(oapv_bs_t *bs, u8 *buf, u32 size, oapv_bs_fn_flush_t fn_flush)
{
    bs->size = size;
    bs->cur = buf;
    bs->beg = buf;
    bs->end = buf + size;
    bs->code = 0;
    bs->leftbits = 0;
    bs->fn_flush = (fn_flush == NULL) ? bsr_flush : fn_flush;
}

int oapv_bsr_clz_in_code(u32 code)
{
    int clz, bits4, shift;

    if(code == 0)
        return 64; /* to protect infinite loop */

    bits4 = 0;
    clz = 0;
    shift = 56;

    while(bits4 == 0 && shift >= 0) {
        bits4 = (code >> shift) & 0xf;
        clz += tbl_zero_count4[bits4];
        shift -= 4;
    }
    return clz;
}

int oapv_bsr_clz(oapv_bs_t *bs)
{
    int clz;
    u32 code;

    code = oapv_bsr_peek(bs, 64); /* supports max 64bit codeword */
    oapv_assert(code != 0);
    clz = oapv_bsr_clz_in_code(code);
    return clz;
}

void oapv_bsr_align8(oapv_bs_t *bs)
{
    /*
    while (!bsr_is_align8(bs)) {
        oapv_bsr_read1(bs);
    }
    */
    int size;

    size = bs->leftbits & 0x7;

    bs->code <<= size;
    bs->leftbits -= size;
}

void oapv_bsr_skip(oapv_bs_t *bs, int size)
{
    oapv_assert(size > 0 && size <= 32);

    if(bs->leftbits < size) {
        size -= bs->leftbits;
        if(bs->fn_flush(bs, 8)) {
            // oapv_trace("already reached the end of bitstream\n");  /* should be updated */
            return;
        }
    }
    bsr_skip_code(bs, size);
}

u32 oapv_bsr_peek(oapv_bs_t *bs, int size)
{
    int byte, leftbits;
    u32 code = 0;

    if(bs->leftbits < size) {
        byte = (64 - bs->leftbits) >> 3;

        /* We should not check the return value
        because this function could be failed at the EOB. */
        if(byte) {
            code = bs->code;
            leftbits = bs->leftbits;

            bs->fn_flush(bs, byte);

            bs->code >>= leftbits;
            bs->code |= code;
            bs->leftbits += leftbits;
        }
    }

    oapv_assert(bs->leftbits <= 64);

    code = bs->code >> (64 - size);
    size -= bs->leftbits;

    if(size > 0) {
        /* even though we update several bytes, the requested size would be
        larger than current bs->leftbits.
        In this case, we should read one more byte, but we could not store
        the read byte. */
        if(bs->cur < bs->end) {
            code |= *(bs->cur) >> (8 - size);
        }
    }
    return code;
}

void *oapv_bsr_sink(oapv_bs_t *bs)
{
    oapv_assert_rv(bs->cur - BSW_GET_SINK_BYTE(bs) < bs->end, NULL);
    oapv_assert_rv((bs->leftbits & 7) == 0, NULL);
    bs->cur = bs->cur - (bs->leftbits >> 3);
    bs->code = 0;
    bs->leftbits = 0;
    return (void *)bs->cur;
}

void oapv_bsr_move(oapv_bs_t *bs, u8 *pos)
{
    bs->code = 0;
    bs->leftbits = 0;
    bs->cur = pos;
}

u32 oapv_bsr_read(oapv_bs_t *bs, int size)
{
    u32 code = 0;

    oapv_assert(size > 0);

    if(bs->leftbits < size) {
        code = bs->code >> (64 - size);
        size -= bs->leftbits;
        if(bs->fn_flush(bs, 8)) {
            oapv_trace("already reached the end of bitstream\n"); /* should be updated */
            return (u32)(-1);
        }
    }
    code |= (u32)(bs->code >> (64 - size));

    bsr_skip_code(bs, size);

    return code;
}

int oapv_bsr_read1(oapv_bs_t *bs)
{
    int code;
    if(bs->leftbits == 0) {
        if(bs->fn_flush(bs, 8)) {
            oapv_trace("already reached the end of bitstream\n"); /* should be updated */
            return -1;
        }
    }
    code = (int)(bs->code >> 63);

    bs->code <<= 1;
    bs->leftbits -= 1;

    return code;
}

u32 oapv_bsr_read_direct(void *addr, int len)
{
    u32 code = 0;
    int shift = 24;
    u8 *p = (u8 *)addr;
    int byte = (len + 7) >> 3;

    oapv_assert(len <= 32);

    while(byte) {
        code |= *(p) << shift;
        shift -= 8;
        byte--;
        p++;
    }
    return (code >> (32 - len));
}


///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
