/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
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
#include "oapv_blk_neon.h"

#if ARM_NEON

/* the decoder and the encoder hand this a whole 8x8 block, so a row is exactly
   one 128-bit vector */
void oapv_blk_to_pic_16_neon(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const s16 *s = (const s16 *)blk;
    u8        *d = (u8 *)pic;

    if(w != 8 || blk_s != (8 << 1)) {
        oapv_blk_to_pic_16(w, h, blk, blk_s, pic, pic_x, pic_s, bd);
        return;
    }

    const int16x8_t mid = vdupq_n_s16((s16)(1 << (bd - 1)));
    const int16x8_t max = vdupq_n_s16((s16)((1 << bd) - 1));
    const int16x8_t zero = vdupq_n_s16(0);

    for(int j = 0; j < h; j++) {
        // the saturating add stands in for the scalar path's promotion to int:
        // both leave the sum below zero, within range, or above max_val
        int16x8_t v = vld1q_s16(s + (size_t)j * 8);
        v = vqaddq_s16(v, mid);
        v = vmaxq_s16(v, zero);
        v = vminq_s16(v, max);

        vst1q_s16((s16 *)(d + (size_t)j * pic_s), v);
    }
}

void oapv_blk_to_pic_p21x_y_neon(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const s16 *s = (const s16 *)blk;
    u8        *d = (u8 *)pic;

    if(w != 8 || blk_s != (8 << 1)) {
        oapv_blk_to_pic_p21x_y(w, h, blk, blk_s, pic, pic_x, pic_s, bd);
        return;
    }

    const int16x8_t mid = vdupq_n_s16((s16)(1 << (bd - 1)));
    const int16x8_t max = vdupq_n_s16((s16)((1 << bd) - 1));
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t cnt = vdupq_n_s16((s16)(16 - bd));

    for(int j = 0; j < h; j++) {
        int16x8_t v = vld1q_s16(s + (size_t)j * 8);
        v = vqaddq_s16(v, mid);
        v = vmaxq_s16(v, zero);
        v = vminq_s16(v, max);
        v = vshlq_s16(v, cnt);

        vst1q_s16((s16 *)(d + (size_t)j * pic_s), v);
    }
}

void oapv_blk_to_pic_p21x_uv_neon(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const s16 *s = (const s16 *)blk;
    u16       *p = (u16 *)pic + pic_x;

    if(w != 8 || blk_s != (8 << 1)) {
        oapv_blk_to_pic_p21x_uv(w, h, blk, blk_s, pic, pic_x, pic_s, bd);
        return;
    }

    static const u16 even[8] = { 0xFFFF, 0, 0xFFFF, 0, 0xFFFF, 0, 0xFFFF, 0 };

    const int16x8_t  mid = vdupq_n_s16((s16)(1 << (bd - 1)));
    const int16x8_t  max = vdupq_n_s16((s16)((1 << bd) - 1));
    const int16x8_t  zero = vdupq_n_s16(0);
    const int16x8_t  cnt = vdupq_n_s16((s16)(16 - bd));
    const uint16x8_t m_even = vld1q_u16(even);
    const uint16x8_t m_odd = vmvnq_u16(m_even);

    for(int j = 0; j < h; j++) {
        s16      *d = (s16 *)((u8 *)p + (size_t)j * pic_s);

        int16x8_t v = vld1q_s16(s + (size_t)j * 8);
        v = vqaddq_s16(v, mid);
        v = vmaxq_s16(v, zero);
        v = vminq_s16(v, max);
        v = vshlq_s16(v, cnt);

        // the other component sits between the samples of this one and has to
        // survive, so it is read back and put in place; the second window is
        // taken from d[7] to keep every access inside the range the block spans,
        // and both windows are read before either is written because they share
        // that sample
        int16x8x2_t z = vzipq_s16(v, v);

        int16x8_t   c0 = vld1q_s16(d + 0);
        int16x8_t   c1 = vld1q_s16(d + 7);

        vst1q_s16(d + 0, vbslq_s16(m_even, z.val[0], c0));
        vst1q_s16(d + 7, vbslq_s16(m_odd, z.val[1], c1));
    }
}

#endif /* ARM_NEON */
