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

#ifndef __OAPV_UTIL_H__
#define __OAPV_UTIL_H__

#include "oapv_def.h"

#define oapv_max(a, b)               (((a) > (b)) ? (a) : (b))
#define oapv_min(a, b)               (((a) < (b)) ? (a) : (b))
#define oapv_median(x, y, z)         ((((y) < (z)) ^ ((z) < (x))) ? (((x) < (y)) ^ ((z) < (x))) ? (y) : (x) : (z))

#define oapv_div_round_up(n, d)      ((int)(((n) + (d) - 1) / (d)))
#define oapv_div_round_closest(n, d) ((int)(((n) + (d)/2)/(d)))


#define oapv_abs(a)                  (((a) > (0)) ? (a) : (-(a)))
#define oapv_abs64(a)                (((a) ^ ((a) >> 63)) - ((a) >> 63)) // only for 64bit variable
#define oapv_abs32(a)                (((a) ^ ((a) >> 31)) - ((a) >> 31)) // only for 32bit variable
#define oapv_abs16(a)                (((a) ^ ((a) >> 15)) - ((a) >> 15)) // only for 16bit variable

#define oapv_clip3(min, max, val)    oapv_max((min), oapv_min((max), (val)))
#define oapv_clip16_add(a, b)        (oapv_min((a) + (b), 0xffff)) // clipping addition

// macro to get a sign from a value.
// operation: if(val < 0) return 1, else return 0
#define oapv_get_sign(val)           ((val < 0) ? 1 : 0)

// macro to set sign into a value.
// operation: if(sign == 0) return val, else if(sign == 1) return -val
#define oapv_set_sign(val, sign)     ((sign) ? -val : val)

// macro to get a sign from a 16-bit value.
// operation: if(val < 0) return 1, else return 0
#define oapv_get_sign16(val)         (((val) >> 15) & 1)

// macro to get a sign from a 32-bit value.
// operation: if(val < 0) return 1, else return 0
#define oapv_get_sign32(val)         (((val) >> 31) & 1)

// macro to set sign to a 16-bit value.
// operation: if(sign == 0) return val, else if(sign == 1) return -val
#define oapv_set_sign16(val, sign)   (((val) ^ ((s16)((sign) << 15) >> 15)) + (sign))

#define oapv_modulo_idx(num, mod)    (((num) + (mod)) % (mod))

#define oapv_align_value(val, align) ((((val) + (align) - 1) / (align)) * (align))

// validate tile topology and compute number of tiles (overflow-safe).
// returns OAPV_OK and sets *num_tiles on success, error otherwise.
static inline int oapv_validate_tile_topology(int tile_cols, int tile_rows, int *num_tiles)
{
    if(tile_cols < 1 || tile_cols > OAPV_MAX_TILE_COLS)
        return OAPV_ERR_MALFORMED_BITSTREAM;
    if(tile_rows < 1 || tile_rows > OAPV_MAX_TILE_ROWS)
        return OAPV_ERR_MALFORMED_BITSTREAM;
    // both operands are bounded by 20, so the product cannot overflow int
    int n = tile_cols * tile_rows;
    if(n > OAPV_MAX_TILES)
        return OAPV_ERR_MALFORMED_BITSTREAM;
    if(num_tiles)
        *num_tiles = n;
    return OAPV_OK;
}

// validate that (header + payload) fits within remaining bytes without
// integer wrap. operands are promoted to u64 so a crafted u32 payload
// size cannot bypass the check via overflow.
static inline int oapv_validate_payload_bounds(u64 remaining, u64 header, u64 payload)
{
    if(remaining < header)
        return OAPV_ERR_MALFORMED_BITSTREAM;
    if(payload > remaining - header)
        return OAPV_ERR_MALFORMED_BITSTREAM;
    return OAPV_OK;
}

static inline int chroma_format_idc_to_color_format(int chroma_format_idc)
{
    return ((chroma_format_idc == 0)   ? OAPV_CF_YCBCR400
            : (chroma_format_idc == 1) ? OAPV_CF_YCBCR420
            : (chroma_format_idc == 2) ? OAPV_CF_YCBCR422
            : (chroma_format_idc == 3) ? OAPV_CF_YCBCR444
                                       : OAPV_CF_YCBCR4444);
}

static inline int color_format_to_chroma_format_idc(int color_format)
{
    if(color_format == OAPV_CF_PLANAR2) {
        return 2;
    }
    else {
        return ((color_format == OAPV_CF_YCBCR400)   ? 0
                : (color_format == OAPV_CF_YCBCR420) ? 1
                : (color_format == OAPV_CF_YCBCR422) ? 2
                : (color_format == OAPV_CF_YCBCR444) ? 3
                                                     : 4);
    }
}

static inline int get_chroma_sft_w(int chroma_format_idc)
{
    return ((chroma_format_idc == 0)   ? 1
            : (chroma_format_idc == 1) ? 1
            : (chroma_format_idc == 2) ? 1
                                       : 0);
}

static inline int get_chroma_sft_h(int chroma_format_idc)
{
    return ((chroma_format_idc == 0)   ? 1
            : (chroma_format_idc == 1) ? 1
                                       : 0);
}
static inline int get_num_comp(int chroma_format_idc)
{

    return (chroma_format_idc == 0)   ? 1
           : (chroma_format_idc == 4) ? 4
                                      : 3;
}

static inline void imgb_addref(oapv_imgb_t *imgb)
{
    if(imgb->addref) {
        imgb->addref(imgb);
    }
}

static inline void imgb_release(oapv_imgb_t *imgb)
{
    if(imgb->release) {
        imgb->release(imgb);
    }
}

/* MD5 structure */
typedef struct
{
    u32 h[4];    /* hash state ABCD */
    u8  msg[64]; /*input buffer */
    u32 bits[2]; /* number of bits, modulo 2^64 (lsb first)*/
} oapv_md5_t;

/* MD5 Functions */
int  oapv_imgb_valid(oapv_imgb_t *imgb);
int  oapv_imgb_set_md5(oapv_imgb_t *imgb);
void oapv_imgb_clr_md5(oapv_imgb_t *imgb);

void oapv_block_copy(s16 *src, int src_stride, s16 *dst, int dst_stride, int log2_copy_w, int log2_copy_h);
int oapv_set_md5_pld(oapvm_t mid, int group_id, oapv_imgb_t *rec);

#if X86_SSE
int oapv_check_cpu_info_x86();
#endif

/* For debugging (START) */
#define ENC_DEC_DUMP 0
#if ENC_DEC_DUMP
#if defined(__GNUC__)
#pragma message "warning! syntax dump is on"
#else
#pragma message("warning! syntax dump is on")
#endif

#define DUMP_ENABLE_HLS  1
#define DUMP_ENABLE_COEF 1

typedef enum _OAPV_DUMP_OPTION {
    OAPV_DUMP_HLS,
    OAPV_DUMP_COEF,
} OAPV_DUMP_OPTION;

extern FILE *oapv_fp_dump;
extern int   oapv_is_dump;

void oapv_dump_string0(int cond, const char *fmt, ...);
void oapv_dump_coef0(int dc, short *coef, int size, int x, int y, int c);
void oapv_dump_create0(int is_enc);
void oapv_dump_delete0();

// Syntax dump macro functions
#define DUMP_CREATE(is_enc) oapv_dump_create0(is_enc)
#define DUMP_DELETE()       oapv_dump_delete0()
#define DUMP_SET(val)       oapv_is_dump = val
#define DUMP_HLS(name, val) \
    oapv_dump_string0(OAPV_DUMP_HLS, "%-34s: %12d\n", #name, val)
#define DUMP_COEF(dc, coef, size, x, y, c) \
    oapv_dump_coef0(dc, coef, size, x, y, c)
#define DUMP_SAVE(id) long dump32u8i90432890_##id = ftell(oapv_fp_dump)
#define DUMP_LOAD(id) fseek(oapv_fp_dump, dump32u8i90432890_##id, SEEK_SET)
#else
#define DUMP_CREATE(is_enc)
#define DUMP_DELETE()
#define DUMP_SET(val)
#define DUMP_HLS(name, val)
#define DUMP_COEF(dc, coef, size, args, ...)
#define DUMP_SAVE(id)
#define DUMP_LOAD(id)
#endif
/* For debugging (END) */

#endif /* __OAPV_UTIL_H__ */
