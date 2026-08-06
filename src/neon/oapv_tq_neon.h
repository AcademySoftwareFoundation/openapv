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

#ifndef _OAPV_TQ_NEON_H_
#define _OAPV_TQ_NEON_H_

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

#if ARM_NEON

extern const oapv_fn_tx_t oapv_tbl_fn_txb_neon[2];
extern const oapv_fn_quant_t oapv_tbl_fn_quant_neon[2];
extern const oapv_fn_itx_t oapv_tbl_fn_itx_neon[2];

#define CALCU_2x8(c0, c1, d0, d1)  \
   v0 = _mm256_madd_epi16(s0, c0); \
   v1 = _mm256_madd_epi16(s1, c0); \
   v2 = _mm256_madd_epi16(s2, c0); \
   v3 = _mm256_madd_epi16(s3, c0); \
   v4 = _mm256_madd_epi16(s0, c1); \
   v5 = _mm256_madd_epi16(s1, c1); \
   v6 = _mm256_madd_epi16(s2, c1); \
   v7 = _mm256_madd_epi16(s3, c1); \
   v0 = _mm256_hadd_epi32(v0, v1); \
   v2 = _mm256_hadd_epi32(v2, v3); \
   v4 = _mm256_hadd_epi32(v4, v5); \
   v6 = _mm256_hadd_epi32(v6, v7); \
   d0 = _mm256_hadd_epi32(v0, v2); \
   d1 = _mm256_hadd_epi32(v4, v6)

#define CALCU_2x8_ADD_SHIFT(d0, d1, d2, d3, add, shift) \
   d0 = _mm256_add_epi32(d0, add);                      \
   d1 = _mm256_add_epi32(d1, add);                      \
   d2 = _mm256_add_epi32(d2, add);                      \
   d3 = _mm256_add_epi32(d3, add);                      \
   d0 = _mm256_srai_epi32(d0, shift);                   \
   d1 = _mm256_srai_epi32(d1, shift);                   \
   d2 = _mm256_srai_epi32(d2, shift);                   \
   d3 = _mm256_srai_epi32(d3, shift);

#define CALCU_2x4(c0, c1, c2, c3, d0, d1)   \
   v0 = _mm256_madd_epi16(s0, c0);          \
   v1 = _mm256_madd_epi16(s1, c0);          \
   v2 = _mm256_madd_epi16(s0, c1);          \
   v3 = _mm256_madd_epi16(s1, c1);          \
   v4 = _mm256_madd_epi16(s0, c2);          \
   v5 = _mm256_madd_epi16(s1, c2);          \
   v6 = _mm256_madd_epi16(s0, c3);          \
   v7 = _mm256_madd_epi16(s1, c3);          \
   v0 = _mm256_hadd_epi32(v0, v1);          \
   v2 = _mm256_hadd_epi32(v2, v3);          \
   v4 = _mm256_hadd_epi32(v4, v5);          \
   v6 = _mm256_hadd_epi32(v6, v7);          \
   d0 = _mm256_hadd_epi32(v0, v2);          \
   d1 = _mm256_hadd_epi32(v4, v6);          \
   d0 = _mm256_permute4x64_epi64(d0, 0xd8); \
   d1 = _mm256_permute4x64_epi64(d1, 0xd8)

#define CALCU_LINE_1x8(coeff0, dst)              \
   v0 = _mm256_madd_epi16(s00, coeff0);          \
   v1 = _mm256_madd_epi16(s01, coeff0);          \
   v2 = _mm256_madd_epi16(s02, coeff0);          \
   v3 = _mm256_madd_epi16(s03, coeff0);          \
   v4 = _mm256_madd_epi16(s04, coeff0);          \
   v5 = _mm256_madd_epi16(s05, coeff0);          \
   v6 = _mm256_madd_epi16(s06, coeff0);          \
   v7 = _mm256_madd_epi16(s07, coeff0);          \
   v0 = _mm256_hadd_epi32(v0, v1);               \
   v2 = _mm256_hadd_epi32(v2, v3);               \
   v4 = _mm256_hadd_epi32(v4, v5);               \
   v6 = _mm256_hadd_epi32(v6, v7);               \
   v0 = _mm256_hadd_epi32(v0, v2);               \
   v4 = _mm256_hadd_epi32(v4, v6);               \
   v1 = _mm256_permute2x128_si256(v0, v4, 0x20); \
   v2 = _mm256_permute2x128_si256(v0, v4, 0x31); \
   dst = _mm256_add_epi32(v1, v2)

#define CALCU_LINE_1x8_ADD_SHIFT(d0, d1, d2, d3, d4, d5, d6, d7, add, shift) \
   d0 = _mm256_add_epi32(d0, add);                                           \
   d1 = _mm256_add_epi32(d1, add);                                           \
   d2 = _mm256_add_epi32(d2, add);                                           \
   d3 = _mm256_add_epi32(d3, add);                                           \
   d4 = _mm256_add_epi32(d4, add);                                           \
   d5 = _mm256_add_epi32(d5, add);                                           \
   d6 = _mm256_add_epi32(d6, add);                                           \
   d7 = _mm256_add_epi32(d7, add);                                           \
   d0 = _mm256_srai_epi32(d0, shift);                                        \
   d1 = _mm256_srai_epi32(d1, shift);                                        \
   d2 = _mm256_srai_epi32(d2, shift);                                        \
   d3 = _mm256_srai_epi32(d3, shift);                                        \
   d4 = _mm256_srai_epi32(d4, shift);                                        \
   d5 = _mm256_srai_epi32(d5, shift);                                        \
   d6 = _mm256_srai_epi32(d6, shift);                                        \
   d7 = _mm256_srai_epi32(d7, shift)

#endif // ARM_NEON

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

#endif /* _OAPV_TQ_NEON_H_  */
