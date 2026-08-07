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

#include "oapv_sad_avx.h"

#if X86_SSE

/* SAD ***********************************************************************/
/* SSD ***********************************************************************/
static s64 ssd_16b_avx_8x8(int w, int h, void* src1, void* src2, int s_src1, int s_src2)
{
    s16* s1 = (s16*)src1;
    s16* s2 = (s16*)src2;
    __m256i s1_vector, s2_vector, diff_vector, sq_vector1, sq_vector2;
    s64 sum_arr[4];
    // Because we are working with 16 elements at a time, stride is multiplied by 2.
    s16 s1_stride = 2 * s_src1;
    s16 s2_stride = 2 * s_src2;
    s64 ssd = 0;
    { // Row 0 and Row 1
        // Load Row 0 and Row 1 data into registers.
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        // Calculate squared difference between two rows.
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector1 = _mm256_madd_epi16(diff_vector, diff_vector);
    }
    { // Row 2 and Row 3
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
    }
    // Add squared differences to running total.
    __m256i sum = _mm256_add_epi32(sq_vector1, sq_vector2);
    { // Row 4 and Row 5
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
        sum = _mm256_add_epi32(sum, sq_vector2);
    }
    { // Row 6 and Row 7
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
        sum = _mm256_add_epi32(sum, sq_vector2);
    }
    // Convert 16-bit integers to 32-bit integers for summation.
    __m128i sum_low = _mm256_extracti128_si256(sum, 0);
    __m128i sum_high = _mm256_extracti128_si256(sum, 1);
    __m256i sum_low_64 = _mm256_cvtepi32_epi64(sum_low);
    __m256i sum_high_64 = _mm256_cvtepi32_epi64(sum_high);
    // Sum up all the values in the array to get final SSD value.
    sum = _mm256_add_epi64(sum_low_64, sum_high_64);
    _mm256_storeu_si256((__m256i*)sum_arr, sum); // store in array for summation.
    ssd = sum_arr[0] + sum_arr[1] + sum_arr[2] + sum_arr[3];
    return ssd;
}

const oapv_fn_ssd_t oapv_tbl_fn_ssd_16b_avx[2] =
{
    ssd_16b_avx_8x8,
    NULL
};

int oapv_dc_removed_had8x8_avx(pel* org, int s_org)
{
    /* first pass is register-wise on 128-bit row vectors; after a transpose
       the second pass runs register-wise on 256-bit s32 vectors, since its
       values can reach 64 * 4095 and do not fit in s16 */
    __m128i r0 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r1 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r2 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r3 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r4 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r5 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r6 = _mm_loadu_si128((__m128i*)(org)); org += s_org;
    __m128i r7 = _mm_loadu_si128((__m128i*)(org));

    /* pass 1: vertical butterflies */
    __m128i a0 = _mm_add_epi16(r0, r4), a4 = _mm_sub_epi16(r0, r4);
    __m128i a1 = _mm_add_epi16(r1, r5), a5 = _mm_sub_epi16(r1, r5);
    __m128i a2 = _mm_add_epi16(r2, r6), a6 = _mm_sub_epi16(r2, r6);
    __m128i a3 = _mm_add_epi16(r3, r7), a7 = _mm_sub_epi16(r3, r7);

    __m128i b0 = _mm_add_epi16(a0, a2), b2 = _mm_sub_epi16(a0, a2);
    __m128i b1 = _mm_add_epi16(a1, a3), b3 = _mm_sub_epi16(a1, a3);
    __m128i b4 = _mm_add_epi16(a4, a6), b6 = _mm_sub_epi16(a4, a6);
    __m128i b5 = _mm_add_epi16(a5, a7), b7 = _mm_sub_epi16(a5, a7);

    r0 = _mm_add_epi16(b0, b1); r1 = _mm_sub_epi16(b0, b1);
    r2 = _mm_add_epi16(b2, b3); r3 = _mm_sub_epi16(b2, b3);
    r4 = _mm_add_epi16(b4, b5); r5 = _mm_sub_epi16(b4, b5);
    r6 = _mm_add_epi16(b6, b7); r7 = _mm_sub_epi16(b6, b7);

    /* 8x8 s16 transpose */
    __m128i t0 = _mm_unpacklo_epi16(r0, r1), t1 = _mm_unpackhi_epi16(r0, r1);
    __m128i t2 = _mm_unpacklo_epi16(r2, r3), t3 = _mm_unpackhi_epi16(r2, r3);
    __m128i t4 = _mm_unpacklo_epi16(r4, r5), t5 = _mm_unpackhi_epi16(r4, r5);
    __m128i t6 = _mm_unpacklo_epi16(r6, r7), t7 = _mm_unpackhi_epi16(r6, r7);
    __m128i u0 = _mm_unpacklo_epi32(t0, t2), u1 = _mm_unpackhi_epi32(t0, t2);
    __m128i u2 = _mm_unpacklo_epi32(t1, t3), u3 = _mm_unpackhi_epi32(t1, t3);
    __m128i u4 = _mm_unpacklo_epi32(t4, t6), u5 = _mm_unpackhi_epi32(t4, t6);
    __m128i u6 = _mm_unpacklo_epi32(t5, t7), u7 = _mm_unpackhi_epi32(t5, t7);
    r0 = _mm_unpacklo_epi64(u0, u4); r1 = _mm_unpackhi_epi64(u0, u4);
    r2 = _mm_unpacklo_epi64(u1, u5); r3 = _mm_unpackhi_epi64(u1, u5);
    r4 = _mm_unpacklo_epi64(u2, u6); r5 = _mm_unpackhi_epi64(u2, u6);
    r6 = _mm_unpacklo_epi64(u3, u7); r7 = _mm_unpackhi_epi64(u3, u7);

    /* widen to s32 and run pass 2 register-wise */
    __m256i w0 = _mm256_cvtepi16_epi32(r0);
    __m256i w1 = _mm256_cvtepi16_epi32(r1);
    __m256i w2 = _mm256_cvtepi16_epi32(r2);
    __m256i w3 = _mm256_cvtepi16_epi32(r3);
    __m256i w4 = _mm256_cvtepi16_epi32(r4);
    __m256i w5 = _mm256_cvtepi16_epi32(r5);
    __m256i w6 = _mm256_cvtepi16_epi32(r6);
    __m256i w7 = _mm256_cvtepi16_epi32(r7);

    __m256i c0 = _mm256_add_epi32(w0, w4), c4 = _mm256_sub_epi32(w0, w4);
    __m256i c1 = _mm256_add_epi32(w1, w5), c5 = _mm256_sub_epi32(w1, w5);
    __m256i c2 = _mm256_add_epi32(w2, w6), c6 = _mm256_sub_epi32(w2, w6);
    __m256i c3 = _mm256_add_epi32(w3, w7), c7 = _mm256_sub_epi32(w3, w7);

    __m256i d0 = _mm256_add_epi32(c0, c2), d2 = _mm256_sub_epi32(c0, c2);
    __m256i d1 = _mm256_add_epi32(c1, c3), d3 = _mm256_sub_epi32(c1, c3);
    __m256i d4 = _mm256_add_epi32(c4, c6), d6 = _mm256_sub_epi32(c4, c6);
    __m256i d5 = _mm256_add_epi32(c5, c7), d7 = _mm256_sub_epi32(c5, c7);

    w0 = _mm256_abs_epi32(_mm256_add_epi32(d0, d1));
    w1 = _mm256_abs_epi32(_mm256_sub_epi32(d0, d1));
    w2 = _mm256_abs_epi32(_mm256_add_epi32(d2, d3));
    w3 = _mm256_abs_epi32(_mm256_sub_epi32(d2, d3));
    w4 = _mm256_abs_epi32(_mm256_add_epi32(d4, d5));
    w5 = _mm256_abs_epi32(_mm256_sub_epi32(d4, d5));
    w6 = _mm256_abs_epi32(_mm256_add_epi32(d6, d7));
    w7 = _mm256_abs_epi32(_mm256_sub_epi32(d6, d7));

    __m256i sum = _mm256_add_epi32(w0, w1);
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(w2, w3));
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(w4, w5));
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(w6, w7));

    __m128i s128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    s128 = _mm_add_epi32(s128, _mm_srli_si128(s128, 8));
    s128 = _mm_add_epi32(s128, _mm_srli_si128(s128, 4));

    int satd = _mm_cvtsi128_si32(s128) - _mm_cvtsi128_si32(_mm256_castsi256_si128(w0)); // remove DC
    return (satd + 2) >> 2;
}


/* DIFF ***********************************************************************/
#endif