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
#include "oapv_tq_avx.h"

#ifndef HAVE_MM256_SETR
#define _mm256_set_m128i(v0, v1)  _mm256_insertf128_si256(_mm256_castsi128_si256(v1), (v0), 1)
#define _mm256_setr_m128i(v0, v1) _mm256_set_m128i((v1), (v0))
#endif


#if X86_SSE
#ifndef _mm256_set_m128i
#define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) \
    _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)
#endif // !_mm256_set_m128i

#ifndef _mm256_loadu2_m128i
#define _mm256_loadu2_m128i(/* __m128i const* */ hiaddr, \
                            /* __m128i const* */ loaddr) \
    _mm256_set_m128i(_mm_loadu_si128(hiaddr), _mm_loadu_si128(loaddr))
#endif // !_mm256_loadu2_m128i

static void oapv_tx_avx(s16 *src, int shift1, int shift2, int line)
{
    __m256i v0, v1, v2, v3, v4, v5, v6, v7;
    __m256i d0, d1, d2, d3, d4, d5;
    __m256i coeff[8];
    coeff[0] = _mm256_set1_epi16(64);
    coeff[1] = _mm256_set_epi16(64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64);
    coeff[2] = _mm256_set_epi16(84, 35, -35, -84, -84, -35, 35, 84, 84, 35, -35, -84, -84, -35, 35, 84);
    coeff[3] = _mm256_set_epi16(35, -84, 84, -35, -35, 84, -84, 35, 35, -84, 84, -35, -35, 84, -84, 35);
    coeff[4] = _mm256_set_epi16(-89, -75, -50, -18, 18, 50, 75, 89, -89, -75, -50, -18, 18, 50, 75, 89);
    coeff[5] = _mm256_set_epi16(-75, 18, 89, 50, -50, -89, -18, 75, -75, 18, 89, 50, -50, -89, -18, 75);
    coeff[6] = _mm256_set_epi16(-50, 89, -18, -75, 75, 18, -89, 50, -50, 89, -18, -75, 75, 18, -89, 50);
    coeff[7] = _mm256_set_epi16(-18, 50, -75, 89, -89, 75, -50, 18, -18, 50, -75, 89, -89, 75, -50, 18);
    __m256i add1 = _mm256_set1_epi32(1 << (shift1 - 1));
    __m256i add2 = _mm256_set1_epi32(1 << (shift2 - 1));

    __m256i s0, s1, s2, s3;

    s0 = _mm256_loadu2_m128i((const __m128i *)&src[32], (const __m128i *)&src[0]);
    s1 = _mm256_loadu2_m128i((const __m128i *)&src[40], (const __m128i *)&src[8]);
    s2 = _mm256_loadu2_m128i((const __m128i *)&src[48], (const __m128i *)&src[16]);
    s3 = _mm256_loadu2_m128i((const __m128i *)&src[56], (const __m128i *)&src[24]);

    CALCU_2x8(coeff[0], coeff[4], d0, d1);
    CALCU_2x8(coeff[2], coeff[5], d2, d3);
    CALCU_2x8_ADD_SHIFT(d0, d1, d2, d3, add1, shift1);

    d0 = _mm256_packs_epi32(d0, d1);
    d1 = _mm256_packs_epi32(d2, d3);

    d0 = _mm256_permute4x64_epi64(d0, 0xd8);
    d1 = _mm256_permute4x64_epi64(d1, 0xd8);

    CALCU_2x8(coeff[1], coeff[6], d2, d3);
    CALCU_2x8(coeff[3], coeff[7], d4, d5);
    CALCU_2x8_ADD_SHIFT(d2, d3, d4, d5, add1, shift1);

    d2 = _mm256_packs_epi32(d2, d3);
    d3 = _mm256_packs_epi32(d4, d5);

    d2 = _mm256_permute4x64_epi64(d2, 0xd8);
    d3 = _mm256_permute4x64_epi64(d3, 0xd8);

    s0 = _mm256_setr_m128i(_mm256_castsi256_si128(d0), _mm256_castsi256_si128(d2));
    s1 = _mm256_setr_m128i(_mm256_extracti128_si256(d0, 1), _mm256_extracti128_si256(d2, 1));
    s2 = _mm256_setr_m128i(_mm256_castsi256_si128(d1), _mm256_castsi256_si128(d3));
    s3 = _mm256_setr_m128i(_mm256_extracti128_si256(d1, 1), _mm256_extracti128_si256(d3, 1));

    CALCU_2x8(coeff[0], coeff[4], d0, d1);
    CALCU_2x8(coeff[2], coeff[5], d2, d3);
    CALCU_2x8_ADD_SHIFT(d0, d1, d2, d3, add2, shift2)

    d0 = _mm256_packs_epi32(d0, d1);
    d1 = _mm256_packs_epi32(d2, d3);

    d0 = _mm256_permute4x64_epi64(d0, 0xd8);
    d1 = _mm256_permute4x64_epi64(d1, 0xd8);

    _mm_store_si128((__m128i *)src, _mm256_castsi256_si128(d0));
    _mm_store_si128((__m128i *)(src + 1 * line), _mm256_extracti128_si256(d0, 1));
    _mm_store_si128((__m128i *)(src + 2 * line), _mm256_castsi256_si128(d1));
    _mm_store_si128((__m128i *)(src + 3 * line), _mm256_extracti128_si256(d1, 1));

    CALCU_2x8(coeff[1], coeff[6], d0, d1);
    CALCU_2x8(coeff[3], coeff[7], d2, d3);
    CALCU_2x8_ADD_SHIFT(d0, d1, d2, d3, add2, shift2);

    d0 = _mm256_packs_epi32(d0, d1);
    d1 = _mm256_packs_epi32(d2, d3);

    d0 = _mm256_permute4x64_epi64(d0, 0xd8);
    d1 = _mm256_permute4x64_epi64(d1, 0xd8);

    _mm_store_si128((__m128i *)(src + 4 * line), _mm256_castsi256_si128(d0));
    _mm_store_si128((__m128i *)(src + 5 * line), _mm256_extracti128_si256(d0, 1));
    _mm_store_si128((__m128i *)(src + 6 * line), _mm256_castsi256_si128(d1));
    _mm_store_si128((__m128i *)(src + 7 * line), _mm256_extracti128_si256(d1, 1));
}

const oapv_fn_tx_t oapv_tbl_fn_txb_avx[2] =
{
    oapv_tx_avx,
        NULL
};

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
// ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

#define TRANSPOSE_8x4_16BIT(I0, I1, I2, I3, I4, I5, I6, I7, O0, O1, O2, O3) \
    tr0_0 = _mm_unpacklo_epi16(I0, I1); \
    tr0_1 = _mm_unpacklo_epi16(I2, I3); \
    tr0_2 = _mm_unpacklo_epi16(I4, I5); \
    tr0_3 = _mm_unpacklo_epi16(I6, I7); \
    tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1); \
    tr1_1 = _mm_unpackhi_epi32(tr0_0, tr0_1); \
    tr1_2 = _mm_unpacklo_epi32(tr0_2, tr0_3); \
    tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3); \
    O0 = _mm_unpacklo_epi64(tr1_0, tr1_2); \
    O1 = _mm_unpackhi_epi64(tr1_0, tr1_2); \
    O2 = _mm_unpacklo_epi64(tr1_1, tr1_3); \
    O3 = _mm_unpackhi_epi64(tr1_1, tr1_3);

// transpose 8x8: 8 x 8(32bit) --> 8 x 8(16bit)
// O0: row0, row4
// O1: row1, row5
// O2: row2, row6
// O3: row3, row7
#define TRANSPOSE_8x8_32BIT_16BIT(I0, I1, I2, I3, I4, I5, I6, I7, O0, O1, O2, O3) \
    I0 = _mm256_packs_epi32(I0, I4);    \
    I1 = _mm256_packs_epi32(I1, I5);    \
    I2 = _mm256_packs_epi32(I2, I6);    \
    I3 = _mm256_packs_epi32(I3, I7);    \
    I4 = _mm256_unpacklo_epi16(I0, I2); \
    I5 = _mm256_unpackhi_epi16(I0, I2); \
    I6 = _mm256_unpacklo_epi16(I1, I3); \
    I7 = _mm256_unpackhi_epi16(I1, I3); \
    I0 = _mm256_unpacklo_epi16(I4, I6); \
    I1 = _mm256_unpackhi_epi16(I4, I6); \
    I2 = _mm256_unpacklo_epi16(I5, I7); \
    I3 = _mm256_unpackhi_epi16(I5, I7); \
    O0 = _mm256_unpacklo_epi64(I0, I2); \
    O1 = _mm256_unpackhi_epi64(I0, I2); \
    O2 = _mm256_unpacklo_epi64(I1, I3); \
    O3 = _mm256_unpackhi_epi64(I1, I3)

// transpose 8x8: 16 x 8(32bit) --> 8 x 16(16bit)
#define TRANSPOSE_16x8_32BIT_16BIT(I00, I01, I02, I03, I04, I05, I06, I07, I08, I09, I10, I11, I12, I13, I14, I15, O0, O1, O2, O3, O4, O5, O6, O7)\
    TRANSPOSE_8x8_32BIT_16BIT(I00, I01, I02, I03, I04, I05, I06, I07, I04, I05, I06, I07); \
    TRANSPOSE_8x8_32BIT_16BIT(I08, I09, I10, I11, I12, I13, I14, I15, I12, I13, I14, I15); \
    O0 = _mm256_insertf128_si256(I04, _mm256_castsi256_si128(I12), 1);      \
    O1 = _mm256_insertf128_si256(I05, _mm256_castsi256_si128(I13), 1);      \
    O2 = _mm256_insertf128_si256(I06, _mm256_castsi256_si128(I14), 1);      \
    O3 = _mm256_insertf128_si256(I07, _mm256_castsi256_si128(I15), 1);      \
    O4 = _mm256_insertf128_si256(I12, _mm256_extracti128_si256(I04, 1), 0); \
    O5 = _mm256_insertf128_si256(I13, _mm256_extracti128_si256(I05, 1), 0); \
    O6 = _mm256_insertf128_si256(I14, _mm256_extracti128_si256(I06, 1), 0); \
    O7 = _mm256_insertf128_si256(I15, _mm256_extracti128_si256(I07, 1), 0)

#define set_vals(a,b) b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a
#define set_vals1(a,b) b, a, b, a, b, a, b, a

#define SET_COEFF                                                                                                                       \
    const __m256i coeff_p89_p75 = _mm256_setr_epi16(89, 75, 89, 75, 89, 75, 89, 75, 89, 75, 89, 75, 89, 75, 89, 75);                    \
    const __m256i coeff_p50_p18 = _mm256_setr_epi16(50, 18, 50, 18, 50, 18, 50, 18, 50, 18, 50, 18, 50, 18, 50, 18);                    \
    const __m256i coeff_p75_n18 = _mm256_setr_epi16(75, -18, 75, -18, 75, -18, 75, -18, 75, -18, 75, -18, 75, -18, 75, -18);            \
    const __m256i coeff_n89_n50 = _mm256_setr_epi16(-89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50);    \
    const __m256i coeff_p50_n89 = _mm256_setr_epi16(50, -89, 50, -89, 50, -89, 50, -89, 50, -89, 50, -89, 50, -89, 50, -89);            \
    const __m256i coeff_p18_p75 = _mm256_setr_epi16(18, 75, 18, 75, 18, 75, 18, 75, 18, 75, 18, 75, 18, 75, 18, 75);                    \
    const __m256i coeff_p18_n50 = _mm256_setr_epi16(18, -50, 18, -50, 18, -50, 18, -50, 18, -50, 18, -50, 18, -50, 18, -50);            \
    const __m256i coeff_p75_n89 = _mm256_setr_epi16(75, -89, 75, -89, 75, -89, 75, -89, 75, -89, 75, -89, 75, -89, 75, -89);            \
    const __m256i coeff_p64_p64 = _mm256_setr_epi16(64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64);                    \
    const __m256i coeff_p64_n64 = _mm256_setr_epi16(64, -64, 64, -64, 64, -64, 64, -64, 64, -64, 64, -64, 64, -64, 64, -64);            \
    const __m256i coeff_p84_n35 = _mm256_setr_epi16(84, 35, 84, 35, 84, 35, 84, 35, 84, 35, 84, 35, 84, 35, 84, 35);                    \
    const __m256i coeff_p35_n84 = _mm256_setr_epi16(35, -84, 35, -84, 35, -84, 35, -84, 35, -84, 35, -84, 35, -84, 35, -84);

#define ITX_PROCESSING_ODD                      \
    ss0 = _mm_unpacklo_epi16(s1, s3);           \
    ss1 = _mm_unpackhi_epi16(s1, s3);           \
    ss2 = _mm_unpacklo_epi16(s5, s7);           \
    ss3 = _mm_unpackhi_epi16(s5, s7);           \
    e0 = _mm256_set_m128i(ss1, ss0);            \
    e1 = _mm256_set_m128i(ss3, ss2);            \
    t0 = _mm256_madd_epi16(e0, coeff_p89_p75);  \
    t1 = _mm256_madd_epi16(e1, coeff_p50_p18);  \
    t2 = _mm256_madd_epi16(e0, coeff_p75_n18);  \
    t3 = _mm256_madd_epi16(e1, coeff_n89_n50);  \
    o0 = _mm256_add_epi32(t0, t1);              \
    o1 = _mm256_add_epi32(t2, t3);              \
    t0 = _mm256_madd_epi16(e0, coeff_p50_n89);  \
    t1 = _mm256_madd_epi16(e1, coeff_p18_p75);  \
    t2 = _mm256_madd_epi16(e0, coeff_p18_n50);  \
    t3 = _mm256_madd_epi16(e1, coeff_p75_n89);  \
    o2 = _mm256_add_epi32(t0, t1);              \
    o3 = _mm256_add_epi32(t2, t3);              \

#define ITX_PROCESSING_EVEN                     \
    ss0 = _mm_unpacklo_epi16(s0, s4);           \
    ss1 = _mm_unpackhi_epi16(s0, s4);           \
    ss2 = _mm_unpacklo_epi16(s2, s6);           \
    ss3 = _mm_unpackhi_epi16(s2, s6);           \
    e0 = _mm256_set_m128i(ss1, ss0);            \
    e1 = _mm256_set_m128i(ss3, ss2);            \
    ee0 = _mm256_madd_epi16(e0, coeff_p64_p64); \
    ee1 = _mm256_madd_epi16(e0, coeff_p64_n64); \
    eo0 = _mm256_madd_epi16(e1, coeff_p84_n35); \
    eo1 = _mm256_madd_epi16(e1, coeff_p35_n84); \
    e0 = _mm256_add_epi32(ee0, eo0);            \
    e3 = _mm256_sub_epi32(ee0, eo0);            \
    e1 = _mm256_add_epi32(ee1, eo1);            \
    e2 = _mm256_sub_epi32(ee1, eo1);                    

#define ITX_POSTPROCESSING(shift, offset)                                       \
    e0 = _mm256_add_epi32(e0, offset);                                          \
    e3 = _mm256_add_epi32(e3, offset);                                          \
    e1 = _mm256_add_epi32(e1, offset);                                          \
    e2 = _mm256_add_epi32(e2, offset);                                          \
    d0 = _mm256_add_epi32(e0, o0);                                              \
    d7 = _mm256_sub_epi32(e0, o0);                                              \
    d1 = _mm256_add_epi32(e1, o1);                                              \
    d6 = _mm256_sub_epi32(e1, o1);                                              \
    d2 = _mm256_add_epi32(e2, o2);                                              \
    d5 = _mm256_sub_epi32(e2, o2);                                              \
    d3 = _mm256_add_epi32(e3, o3);                                              \
    d4 = _mm256_sub_epi32(e3, o3);                                              \
    d0 = _mm256_srai_epi32(d0, shift);                                          \
    d7 = _mm256_srai_epi32(d7, shift);                                          \
    d1 = _mm256_srai_epi32(d1, shift);                                          \
    d6 = _mm256_srai_epi32(d6, shift);                                          \
    d2 = _mm256_srai_epi32(d2, shift);                                          \
    d5 = _mm256_srai_epi32(d5, shift);                                          \
    d3 = _mm256_srai_epi32(d3, shift);                                          \
    d4 = _mm256_srai_epi32(d4, shift);                                          \
    TRANSPOSE_8x8_32BIT_16BIT(d0, d1, d2, d3, d4, d5, d6, d7, d4, d5, d6, d7);  \
    d0 = _mm256_insertf128_si256(d4, _mm256_castsi256_si128(d5), 1);            \
    d1 = _mm256_insertf128_si256(d6, _mm256_castsi256_si128(d7), 1);            \
    d2 = _mm256_insertf128_si256(d5, _mm256_extracti128_si256(d4, 1), 0);       \
    d3 = _mm256_insertf128_si256(d7, _mm256_extracti128_si256(d6, 1), 0);       

static void oapv_itx_part_avx(s16* src, s16* dst, int shift, int line)
{
    SET_COEFF

    __m128i s0, s1, s2, s3, s4, s5, s6, s7;
    __m128i ss0, ss1, ss2, ss3;
    __m256i e0, e1, e2, e3, o0, o1, o2, o3, ee0, ee1, eo0, eo1;
    __m256i t0, t1, t2, t3;
    __m256i d0, d1, d2, d3, d4, d5, d6, d7;
    __m256i offset = _mm256_set1_epi32(1 << (shift - 1));
    int j;
    int i_src = line;
    int i_src2 = line << 1;
    int i_src3 = i_src + i_src2;
    int i_src4 = i_src << 2;
    int i_src5 = i_src2 + i_src3;
    int i_src6 = i_src3 << 1;
    int i_src7 = i_src3 + i_src4;
    for (j = 0; j < line; j += 8)
    {
        // O[0] -- O[3]
        s1 = _mm_loadu_si128((__m128i*)(src + i_src + j));
        s3 = _mm_loadu_si128((__m128i*)(src + i_src3 + j));
        s5 = _mm_loadu_si128((__m128i*)(src + i_src5 + j));
        s7 = _mm_loadu_si128((__m128i*)(src + i_src7 + j));

        ITX_PROCESSING_ODD

        // E[0] - E[3]
        s0 = _mm_loadu_si128((__m128i*)(src + j));
        s2 = _mm_loadu_si128((__m128i*)(src + i_src2 + j));
        s4 = _mm_loadu_si128((__m128i*)(src + i_src4 + j));
        s6 = _mm_loadu_si128((__m128i*)(src + i_src6 + j));

        ITX_PROCESSING_EVEN

        ITX_POSTPROCESSING(shift, offset)
        
        // store line x 8
        _mm256_storeu_si256((__m256i*)dst, d0);
        _mm256_storeu_si256((__m256i*)(dst + 16), d1);
        _mm256_storeu_si256((__m256i*)(dst + 32), d2);
        _mm256_storeu_si256((__m256i*)(dst + 48), d3);
        dst += 64;
    }
}

const oapv_fn_itx_part_t oapv_tbl_fn_itx_part_avx[2] =
{
    oapv_itx_part_avx,
        NULL
};

static void oapv_itx_avx(s16* src, int shift1, int shift2, int line)
{
    SET_COEFF

    __m128i s0, s1, s2, s3, s4, s5, s6, s7;
    __m128i ss0, ss1, ss2, ss3;
    __m256i e0, e1, e2, e3, o0, o1, o2, o3, ee0, ee1, eo0, eo1;
    __m256i t0, t1, t2, t3;
    __m256i d0, d1, d2, d3, d4, d5, d6, d7;
    __m256i offset1 = _mm256_set1_epi32(1 << (shift1 - 1));
    __m256i offset2 = _mm256_set1_epi32(1 << (shift2 - 1));
    int i_src = line;
    int i_src2 = line << 1;
    int i_src3 = i_src + i_src2;
    int i_src4 = i_src << 2;
    int i_src5 = i_src2 + i_src3;
    int i_src6 = i_src3 << 1;
    int i_src7 = i_src3 + i_src4;
    {
        // O[0] - O[3]
        s1 = _mm_loadu_si128((__m128i*)(src + i_src));
        s3 = _mm_loadu_si128((__m128i*)(src + i_src3));
        s5 = _mm_loadu_si128((__m128i*)(src + i_src5));
        s7 = _mm_loadu_si128((__m128i*)(src + i_src7));

        ITX_PROCESSING_ODD

        // E[0] - E[3]
        s0 = _mm_loadu_si128((__m128i*)(src));
        s2 = _mm_loadu_si128((__m128i*)(src + i_src2));
        s4 = _mm_loadu_si128((__m128i*)(src + i_src4));
        s6 = _mm_loadu_si128((__m128i*)(src + i_src6));

        ITX_PROCESSING_EVEN

        ITX_POSTPROCESSING(shift1, offset1)
    }
    {
        // O[0] - O[3]
        s1 = _mm256_extracti128_si256(d0, 1);
        s3 = _mm256_extracti128_si256(d1, 1);
        s5 = _mm256_extracti128_si256(d2, 1);
        s7 = _mm256_extracti128_si256(d3, 1);

        ITX_PROCESSING_ODD

        // E[0] - E[3]
        s0 = _mm256_extracti128_si256(d0, 0);
        s2 = _mm256_extracti128_si256(d1, 0);
        s4 = _mm256_extracti128_si256(d2, 0);
        s6 = _mm256_extracti128_si256(d3, 0);

        ITX_PROCESSING_EVEN

        ITX_POSTPROCESSING(shift2, offset2)

        // store line x 8
        _mm256_storeu_si256((__m256i*)src, d0);
        _mm256_storeu_si256((__m256i*)(src + 16), d1);
        _mm256_storeu_si256((__m256i*)(src + 32), d2);
        _mm256_storeu_si256((__m256i*)(src + 48), d3);
    }
}

const oapv_fn_itx_t oapv_tbl_fn_itx_avx[2] =
{
    oapv_itx_avx,
        NULL
};

__m256i mul_128i_to_256i_and_add(__m256i offset_vector, __m128i a, __m128i b)
{
    __m256i a_64 = _mm256_cvtepi32_epi64(a);
    __m256i b_64 = _mm256_cvtepi32_epi64(b);
    __m256i result = _mm256_mul_epi32(a_64, b_64);
    result = _mm256_add_epi64(result, offset_vector);
    return result;
}

static int oapv_quant_avx(s16* coef, u8 qp, int q_matrix[OAPV_BLK_D], int log2_w, int log2_h, int bit_depth, int deadzone_offset)
{
    s64 offset;
    int shift;
    int tr_shift;

    int log2_size = (log2_w + log2_h) >> 1;
    tr_shift = MAX_TX_DYNAMIC_RANGE - bit_depth - log2_size;
    shift = QUANT_SHIFT + tr_shift + (qp / 6);
    offset = (s64)deadzone_offset << (shift - 9);
    __m256i offset_vector = _mm256_set1_epi64x(offset);
    __m256i reg_minval_int16 = _mm256_set1_epi32(-32768);
    __m256i reg_maxval_int16 = _mm256_set1_epi32(32767);

    int pixels = (1 << (log2_w + log2_h));
    int i;
    __m256i shuffle0 = _mm256_setr_epi32(1, 3, 5, 7, 0, 2, 4, 6);
    __m256i shuffle1 = _mm256_setr_epi8(
        0, 1, 4, 5, 8, 9, 12, 13,
        -128, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -128, -128, -128, -128);
    __m256i shuffle2 = _mm256_setr_epi8(
        -128, -128, -128, -128, -128, -128, -128, -128,
        0, 1, 4, 5, 8, 9, 12, 13,
        -128, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -128, -128, -128, -128);

    for (i = 0; i < pixels; i += 8)
    {
        // Load first row
        __m256i quant_matrix = _mm256_lddqu_si256((__m256i*)(q_matrix + i));
        __m128i coef_row = _mm_lddqu_si128((__m128i*)(coef + i));

        // Extract sign
        __m128i sign_mask = _mm_srai_epi16(coef_row, 15);
        __m256i sign_mask_ext = _mm256_cvtepi16_epi32(sign_mask);

        // Convert to 32 bits and take abs()
        __m256i coef_row_ext = _mm256_cvtepi16_epi32(coef_row);
        __m256i coef_row_abs = _mm256_abs_epi32(coef_row_ext);

        // Multiply coeff with quant values, add offset to result and shift
        __m256i lev1_low = mul_128i_to_256i_and_add(offset_vector, _mm256_castsi256_si128(coef_row_abs), _mm256_castsi256_si128(quant_matrix));
        __m256i lev1_high = mul_128i_to_256i_and_add(offset_vector, _mm256_extracti128_si256(coef_row_abs, 1), _mm256_extracti128_si256(quant_matrix, 1));
        __m256i lev2_low = _mm256_srli_epi64(lev1_low, shift);
        __m256i lev2_high = _mm256_srli_epi64(lev1_high, shift);

        // First level of combination
        lev2_low = _mm256_slli_epi64(lev2_low, 32);
        __m256i combined = _mm256_or_si256(lev2_low, lev2_high);
        __m256i levx = _mm256_permutevar8x32_epi32(combined, shuffle0);

        // Apply sign and clipping
        levx = _mm256_sub_epi32(_mm256_xor_si256(levx, sign_mask_ext), sign_mask_ext);
        levx = _mm256_max_epi32(levx, reg_minval_int16);
        levx = _mm256_min_epi32(levx, reg_maxval_int16);

        // Second level of combination
        __m256i levx_low_sh = _mm256_shuffle_epi8(levx, shuffle1);
        __m128i levx_high = _mm256_extracti128_si256(levx, 1);
        __m256i levx_high_ext = _mm256_castsi128_si256(levx_high);
        __m256i levx_high_sh = _mm256_shuffle_epi8(levx_high_ext, shuffle2);
        levx = _mm256_or_si256(levx_high_sh, levx_low_sh);

        // store in coef
        __m128i lev4 = _mm256_castsi256_si128(levx);
        _mm_storeu_si128((__m128i*)(coef + i), lev4);
    }
    return OAPV_OK;
}

const oapv_fn_quant_t oapv_tbl_fn_quant_avx[2] =
{
    oapv_quant_avx,
        NULL
};

#define DQUANT_POSTPROCESSING                           \
    lev3 = _mm256_max_epi32(lev3, reg_minval_int16);    \
    lev3 = _mm256_min_epi32(lev3, reg_maxval_int16);    \
    lev3 = _mm256_shuffle_epi8( lev3, shuffle );        \
    __m128i low = _mm256_castsi256_si128( lev3 );       \
    __m128i high = _mm256_extracti128_si256( lev3, 1 ); \
    __m128i lev4 = _mm_or_si128( low, high );           \
    _mm_storeu_si128((__m128i *)(coef + i), lev4);

static void oapv_dquant_avx(s16 *coef, s16 q_matrix[OAPV_BLK_D], int log2_w, int log2_h, s8 shift)
{
    int i;
    int pixels = (1 << (log2_w + log2_h));
    __m256i shuffle = _mm256_setr_epi8(
    0, 1, 4, 5, 8, 9, 12, 13,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    0, 1, 4, 5, 8, 9, 12, 13 );
    __m256i reg_minval_int16 = _mm256_set1_epi32(-32768);
    __m256i reg_maxval_int16 = _mm256_set1_epi32( 32767);
    if (shift > 0)
    {
        s32 offset = (1 << (shift - 1));
        __m256i offset_1 = _mm256_set1_epi32(offset);
        for (i = 0; i < pixels; i += 8)
        {
            __m256i cur_q_matrix = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i*)(q_matrix + i)));
            __m256i coef_8_val_act = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i*)(coef + i)));

            __m256i lev1 = _mm256_mullo_epi32(coef_8_val_act, cur_q_matrix);
            __m256i lev2 = _mm256_add_epi32(lev1, offset_1);
            __m256i lev3 = _mm256_srai_epi32(lev2, shift);

            DQUANT_POSTPROCESSING
        }
    }
    else
    {
        int left_shift = -shift;
        for (i = 0; i < pixels; i += 8)
        {
            __m256i cur_q_matrix = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i*)(q_matrix + i)));
            __m256i coef_8_val_act = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i*)(coef + i)));

            __m256i lev1 = _mm256_mullo_epi32(coef_8_val_act, cur_q_matrix);
            __m256i lev3 = _mm256_slli_epi32(lev1, left_shift);

            DQUANT_POSTPROCESSING
        }
    }
}
const oapv_fn_dquant_t oapv_tbl_fn_dquant_avx[2] =
    {
        oapv_dquant_avx,
            NULL,
};

void oapv_adjust_itrans_avx(int* src, int* dst, int itrans_diff_idx, int diff_step, int shift)
{
    __m256i v0 = _mm256_set1_epi32((1 << 16) | (diff_step & 0xffff));
    __m256i v1 = _mm256_set1_epi16(1 << (shift - 1));
    __m256i s0, s1, d, d0, d1;

    for (int j = 0; j < 64; j += 16)
    {
        s0 = _mm256_loadu_si256((const __m256i*)(src + j));
        d = _mm256_loadu_si256((const __m256i*)(oapv_itrans_diff[itrans_diff_idx] + j));

        d0 = _mm256_unpacklo_epi16(d, v1);
        d0 = _mm256_madd_epi16(d0, v0);
        d0 = _mm256_srai_epi32(d0, shift);
        d0 = _mm256_add_epi32(s0, d0);
        _mm256_storeu_si256((__m256i*)(dst + j), d0);

        s1 = _mm256_loadu_si256((const __m256i*)(src + j + 8));
        d1 = _mm256_unpackhi_epi16(d, v1);
        d1 = _mm256_madd_epi16(d1, v0);
        d1 = _mm256_srai_epi32(d1, shift);
        d1 = _mm256_add_epi32(s1, d1);
        _mm256_storeu_si256((__m256i*)(dst + j + 8), d1);
    }
}

const oapv_fn_itx_adj_t oapv_tbl_fn_itx_adj_avx[2] =
{
    oapv_adjust_itrans_avx,
        NULL,
};
#endif /* X86_SSE */