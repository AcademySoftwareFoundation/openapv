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
#include "oapv_blk_avx.h"

#if X86_SSE

/* the decoder and the encoder hand this a whole 8x8 block, so its rows are
   contiguous and one 256-bit load covers two of them */
void oapv_blk_to_pic_16_avx(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const s16 *s = (const s16 *)blk;
    u8        *d = (u8 *)pic;

    if(w != 8 || blk_s != (8 << 1) || (h & 1) != 0) {
        oapv_blk_to_pic_16(w, h, blk, blk_s, pic, pic_x, pic_s, bd);
        return;
    }

    const __m256i mid = _mm256_set1_epi16((short)(1 << (bd - 1)));
    const __m256i max = _mm256_set1_epi16((short)((1 << bd) - 1));
    const __m256i zero = _mm256_setzero_si256();

    for(int j = 0; j < h; j += 2) {
        // the saturating add stands in for the scalar path's promotion to int:
        // both leave the sum below zero, within range, or above max_val
        __m256i v = _mm256_loadu_si256((const __m256i *)(s + (size_t)j * 8));
        v = _mm256_adds_epi16(v, mid);
        v = _mm256_max_epi16(v, zero);
        v = _mm256_min_epi16(v, max);

        _mm_storeu_si128((__m128i *)(d + (size_t)j * pic_s), _mm256_castsi256_si128(v));
        _mm_storeu_si128((__m128i *)(d + (size_t)(j + 1) * pic_s), _mm256_extracti128_si256(v, 1));
    }
}

#endif /* X86_SSE */
