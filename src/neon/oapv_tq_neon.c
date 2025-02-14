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
#include "oapv_tq_neon.h"

#if ARM_NEON

const s32 oapv_coeff[8][4] =
{
    {64, 64, 64, 64}, // 0th row coeff
    {89, 75, 50, 18}, // 2nd row coeff
    {84, 35, 84, 35}, // 3rd row coeff
    {75,-18,-89,-50}, // 4th row coeff
    {64,-64, 64,-64}, // 5th row coeff
    {50,-89, 18, 75}, // 6th row coeff
    {35,-84, 35,-84}, // 7th row coeff
    {18,-50, 75,-89}  // 8th row coeff
};

#if !defined(__aarch64__)
#define vpaddq_s32(a, b) \
    vcombine_s32(vpadd_s32(vget_low_s32(a), vget_high_s32(a)), vpadd_s32(vget_low_s32(b), vget_high_s32(b)))
#define vmovl_high_s16(a) \
    vmovl_s16(vget_high_s16(a))
#define vzip1_s16(a, b) \
    vget_low_s32(vorrq_s32(vmovl_u16(a), vshll_n_s16(b, 16)))
#define vzip1_s32(a, b) \
    vget_low_s64(vorrq_s64(vmovl_u32(a), vshll_n_s32(b, 32)))
#define vzip2_s16(a, b) \
    vget_high_s32(vorrq_s32(vmovl_u16(a), vshll_n_s16(b, 16)))
#define vzip2_s32(a, b) \
    vget_high_s64(vorrq_s64(vmovl_u32(a), vshll_n_s32(b, 32)))
#endif

#define multiply_s32(part1, part2, coeff, res) \
    low = vmulq_s32(part1, coeff); \
    high = vmulq_s32(part2, coeff); \
    res = vpaddq_s32(low, high)

static void oapv_tx_pb8b_neon(const s16 *src, s16 *dst, const int shift, int line)
{
    s16 i;
    const s16 *tempSrc = src;
    int16x4_t src_part1, src_part2;
    int32x4_t coeff0, coeff1, coeff2, coeff3, coeff4, coeff5, coeff6, coeff7;
    int32x4_t sh = vdupq_n_s32(-shift);

    int32x4_t EE_part1, EE_part2, EO_part1, EO_part2, low, high, result0, result1, result2, result3, result4, result5, result6, result7, E1, O1, E2, O2, res1, res2, res3, res4;

    for(i = 0; i < 8; i += 4)
    {
        // Loading src[0 - 3] and src[4 - 7]
        src_part1 = vld1_s16(tempSrc);
        tempSrc += 4;
        src_part2 = vld1_s16(tempSrc);
        tempSrc += 4;

        //reverse src_part2
        src_part2 = vrev64_s16(src_part2);

        E1 = vaddl_s16(src_part1, src_part2);
        O1 = vsubl_s16(src_part1, src_part2);

        // Loading src[8 - 11] and src[12 - 15]
        src_part1 = vld1_s16(tempSrc);
        tempSrc += 4;
        src_part2 = vld1_s16(tempSrc);
        tempSrc += 4;

        //reverse src_part2
        src_part2 = vrev64_s16(src_part2);

        E2 = vaddl_s16(src_part1, src_part2);
        O2 = vsubl_s16(src_part1, src_part2);

        int32x4_t tmp1 = vcombine_s32(vget_low_s32(E1), vget_low_s32(E2));
        int32x4_t tmp2 = vcombine_s32(vget_high_s32(E1), vget_high_s32(E2));
        tmp2 = vrev64q_s32(tmp2);

        EE_part1 = vaddq_s32(tmp1, tmp2);
        EO_part1 = vsubq_s32(tmp1, tmp2);

        coeff1 = vld1q_s32(oapv_coeff[1]);
        coeff3 = vld1q_s32(oapv_coeff[3]);
        coeff5 = vld1q_s32(oapv_coeff[5]);
        coeff7 = vld1q_s32(oapv_coeff[7]);

        multiply_s32(O1, O2, coeff1, result1);
        multiply_s32(O1, O2, coeff3, result3);
        multiply_s32(O1, O2, coeff5, result5);
        multiply_s32(O1, O2, coeff7, result7);

        res1 = vpaddq_s32(result1, result3);
        res2 = vpaddq_s32(result5, result7);

        // add and shift
        res1 = vrshlq_s32(res1, sh);
        res2 = vrshlq_s32(res2, sh);

        // Loading src[16 - 19] and src[20 - 23]
        src_part1 = vld1_s16(tempSrc);
        tempSrc += 4;
        src_part2 = vld1_s16(tempSrc);
        tempSrc += 4;

        //reverse src_part2
        src_part2 = vrev64_s16(src_part2);

        E1 = vaddl_s16(src_part1, src_part2);
        O1 = vsubl_s16(src_part1, src_part2);

        // Loading src[24 - 27] and src[28 - 31]
        src_part1 = vld1_s16(tempSrc);
        tempSrc += 4;
        src_part2 = vld1_s16(tempSrc);
        tempSrc += 4;

        //reverse src_part2
        src_part2 = vrev64_s16(src_part2);

        E2 = vaddl_s16(src_part1, src_part2);
        O2 = vsubl_s16(src_part1, src_part2);

        multiply_s32(O1, O2, coeff1, result1);
        multiply_s32(O1, O2, coeff3, result3);
        multiply_s32(O1, O2, coeff5, result5);
        multiply_s32(O1, O2, coeff7, result7);

        res3 = vpaddq_s32(result1, result3);
        res4 = vpaddq_s32(result5, result7);

        // add and shift
        res3 = vrshlq_s32(res3, sh);
        res4 = vrshlq_s32(res4, sh);

        // store result in destination
        vst1_s16(dst + 1 * line + i, vmovn_s32(vcombine_s32(vget_low_s32(res1), vget_low_s32(res3))));
        vst1_s16(dst + 3 * line + i, vmovn_s32(vcombine_s32(vget_high_s32(res1), vget_high_s32(res3))));
        vst1_s16(dst + 5 * line + i, vmovn_s32(vcombine_s32(vget_low_s32(res2), vget_low_s32(res4))));
        vst1_s16(dst + 7 * line + i, vmovn_s32(vcombine_s32(vget_high_s32(res2), vget_high_s32(res4))));

        coeff0 = vld1q_s32(oapv_coeff[0]);
        coeff2 = vld1q_s32(oapv_coeff[2]);
        coeff4 = vld1q_s32(oapv_coeff[4]);
        coeff6 = vld1q_s32(oapv_coeff[6]);

        tmp1 = vcombine_s32(vget_low_s32(E1), vget_low_s32(E2));
        tmp2 = vcombine_s32(vget_high_s32(E1), vget_high_s32(E2));
        tmp2 = vrev64q_s32(tmp2);

        EE_part2 = vaddq_s32(tmp1, tmp2);
        EO_part2 = vsubq_s32(tmp1, tmp2);

        multiply_s32(EE_part1, EE_part2, coeff0, result0);
        multiply_s32(EE_part1, EE_part2, coeff4, result4);
        multiply_s32(EO_part1, EO_part2, coeff2, result2);
        multiply_s32(EO_part1, EO_part2, coeff6, result6);

        // add and shift
        result0 = vrshlq_s32(result0, sh);
        result2 = vrshlq_s32(result2, sh);
        result4 = vrshlq_s32(result4, sh);
        result6 = vrshlq_s32(result6, sh);

        // store result in destination
        vst1_s16(dst + 0 * line + i, vmovn_s32(result0));
        vst1_s16(dst + 2 * line + i, vmovn_s32(result2));
        vst1_s16(dst + 4 * line + i, vmovn_s32(result4));
        vst1_s16(dst + 6 * line + i, vmovn_s32(result6));
    }
}

const oapv_fn_tx_t oapv_tbl_fn_txb_neon[2] =
    {
        oapv_tx_pb8b_neon,
            NULL
};

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
// ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

// Required coefficients from oapv_tbl_tm8
# define OAPV_INVTX_COEF_0	    89 // coef10, -coef32, -coef51,  coef73
# define OAPV_INVTX_COEF_1	    75 // coef11,  coef30,  coef53,  coef72
# define OAPV_INVTX_COEF_2	    50 // coef12, -coef33,  coef50, -coef71
# define OAPV_INVTX_COEF_3	    18 // coef13, -coef31,  coef52,  coef70
# define OAPV_INVTX_COEF_5	    84 // coef20, -coef61
# define OAPV_INVTX_COEF_6      35 // coef21,  coef60
# define OAPV_INVTX_COEF_4_LOG2	 6 // log2(coef00), log2(coef01), log2(coef40), log2(-coef41)

static void oapv_itx_pb8b_opt_neon(s16* src, int shift1, int shift2, int line)
{
    int32x4_t sh1 = vdupq_n_s32(-shift1);
    int32x4_t sh2 = vdupq_n_s32(-shift2);

    int16x4_t dest0, dest1, dest2, dest3, dest4, dest5, dest6, dest7, dest8, dest9, dest10, dest11, dest12, dest13, dest14, dest15;

    //DCT Pass 1
    {
        int16x8_t v_src_0_8 = vld1q_s16(src);
        int16x8_t v_src_1_9 = vld1q_s16(src + line);
        int16x8_t v_src_2_10 = vld1q_s16(src + 2 * line);
        int16x8_t v_src_3_11 = vld1q_s16(src + 3 * line);
        int16x8_t v_src_4_12 = vld1q_s16(src + 4 * line);
        int16x8_t v_src_5_13 = vld1q_s16(src + 5 * line);
        int16x8_t v_src_6_14 = vld1q_s16(src + 6 * line);
        int16x8_t v_src_7_15 = vld1q_s16(src + 7 * line);

        int16x4_t v_src_0  = vget_low_s16(v_src_0_8);
        int16x4_t v_src_1  = vget_low_s16(v_src_1_9);
        int16x4_t v_src_2  = vget_low_s16(v_src_2_10);
        int16x4_t v_src_3  = vget_low_s16(v_src_3_11);
        int16x4_t v_src_4  = vget_low_s16(v_src_4_12);
        int16x4_t v_src_5  = vget_low_s16(v_src_5_13);
        int16x4_t v_src_6  = vget_low_s16(v_src_6_14);
        int16x4_t v_src_7  = vget_low_s16(v_src_7_15);
        int16x4_t v_src_8  = vget_high_s16(v_src_0_8);
        int16x4_t v_src_9  = vget_high_s16(v_src_1_9);
        int16x4_t v_src_10 = vget_high_s16(v_src_2_10);
        int16x4_t v_src_11 = vget_high_s16(v_src_3_11);
        int16x4_t v_src_12 = vget_high_s16(v_src_4_12);
        int16x4_t v_src_13 = vget_high_s16(v_src_5_13);
        int16x4_t v_src_14 = vget_high_s16(v_src_6_14);
        int16x4_t v_src_15 = vget_high_s16(v_src_7_15);

        int32x4_t temp1 = vaddq_s32(vmull_n_s16(v_src_1, OAPV_INVTX_COEF_0), vmull_n_s16(v_src_3, OAPV_INVTX_COEF_1));
        int32x4_t temp2 = vsubq_s32(vmull_n_s16(v_src_1, OAPV_INVTX_COEF_1), vmull_n_s16(v_src_3, OAPV_INVTX_COEF_3));

        int32x4_t temp3 = vsubq_s32(vmull_n_s16(v_src_1, OAPV_INVTX_COEF_2), vmull_n_s16(v_src_3, OAPV_INVTX_COEF_0));
        int32x4_t temp4 = vsubq_s32(vmull_n_s16(v_src_1, OAPV_INVTX_COEF_3), vmull_n_s16(v_src_3, OAPV_INVTX_COEF_2));

        int32x4_t temp5 = vaddq_s32(vmull_n_s16(v_src_5, OAPV_INVTX_COEF_2), vmull_n_s16(v_src_7, OAPV_INVTX_COEF_3));
        int32x4_t temp6 = vaddq_s32(vmull_n_s16(v_src_5, OAPV_INVTX_COEF_0), vmull_n_s16(v_src_7, OAPV_INVTX_COEF_2));
        temp6 = vnegq_s32(temp6);

        int32x4_t temp7 = vaddq_s32(vmull_n_s16(v_src_5, OAPV_INVTX_COEF_3), vmull_n_s16(v_src_7, OAPV_INVTX_COEF_1));
        int32x4_t temp8 = vsubq_s32(vmull_n_s16(v_src_5, OAPV_INVTX_COEF_1), vmull_n_s16(v_src_7, OAPV_INVTX_COEF_0));

        int32x4_t temp9 = vaddq_s32(vmull_n_s16(v_src_9, OAPV_INVTX_COEF_0), vmull_n_s16(v_src_11, OAPV_INVTX_COEF_1));
        int32x4_t temp10 = vsubq_s32(vmull_n_s16(v_src_9, OAPV_INVTX_COEF_1), vmull_n_s16(v_src_11, OAPV_INVTX_COEF_3));

        int32x4_t temp11 = vsubq_s32(vmull_n_s16(v_src_9, OAPV_INVTX_COEF_2), vmull_n_s16(v_src_11, OAPV_INVTX_COEF_0));
        int32x4_t temp12 = vsubq_s32(vmull_n_s16(v_src_9, OAPV_INVTX_COEF_3), vmull_n_s16(v_src_11, OAPV_INVTX_COEF_2));

        int32x4_t temp13 = vaddq_s32(vmull_n_s16(v_src_13, OAPV_INVTX_COEF_2), vmull_n_s16(v_src_15, OAPV_INVTX_COEF_3));
        int32x4_t temp14 = vaddq_s32(vmull_n_s16(v_src_13, OAPV_INVTX_COEF_0), vmull_n_s16(v_src_15, OAPV_INVTX_COEF_2));
        temp14 = vnegq_s32(temp14);

        int32x4_t temp15 = vaddq_s32(vmull_n_s16(v_src_13, OAPV_INVTX_COEF_3), vmull_n_s16(v_src_15, OAPV_INVTX_COEF_1));
        int32x4_t temp16 = vsubq_s32(vmull_n_s16(v_src_13, OAPV_INVTX_COEF_1), vmull_n_s16(v_src_15, OAPV_INVTX_COEF_0));

        int32x4_t O0 = vaddq_s32(temp1, temp5);
        int32x4_t O1 = vaddq_s32(temp2, temp6);
        int32x4_t O2 = vaddq_s32(temp3, temp7);
        int32x4_t O3 = vaddq_s32(temp4, temp8);
        int32x4_t O4 = vaddq_s32(temp9, temp13);
        int32x4_t O5 = vaddq_s32(temp10, temp14);
        int32x4_t O6 = vaddq_s32(temp11, temp15);
        int32x4_t O7 = vaddq_s32(temp12, temp16);

        int32x4_t EO0 = vaddq_s32(vmull_n_s16(v_src_2, OAPV_INVTX_COEF_5), vmull_n_s16(v_src_6, OAPV_INVTX_COEF_6));
        int32x4_t EO1 = vsubq_s32(vmull_n_s16(v_src_2, OAPV_INVTX_COEF_6), vmull_n_s16(v_src_6, OAPV_INVTX_COEF_5));
        int32x4_t EE0 = vaddq_s32(vshll_n_s16(v_src_0, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(v_src_4, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EE1 = vsubq_s32(vshll_n_s16(v_src_0, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(v_src_4, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EO2 = vaddq_s32(vmull_n_s16(v_src_10, OAPV_INVTX_COEF_5), vmull_n_s16(v_src_14, OAPV_INVTX_COEF_6));
        int32x4_t EO3 = vsubq_s32(vmull_n_s16(v_src_10, OAPV_INVTX_COEF_6), vmull_n_s16(v_src_14, OAPV_INVTX_COEF_5));
        int32x4_t EE2 = vaddq_s32(vshll_n_s16(v_src_8, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(v_src_12, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EE3 = vsubq_s32(vshll_n_s16(v_src_8, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(v_src_12, OAPV_INVTX_COEF_4_LOG2));

        int32x4_t E0 = vaddq_s32(EE0, EO0);
        int32x4_t E1 = vaddq_s32(EE1, EO1);
        int32x4_t E2 = vsubq_s32(EE1, EO1);
        int32x4_t E3 = vsubq_s32(EE0, EO0);
        int32x4_t E4 = vaddq_s32(EE2, EO2);
        int32x4_t E5 = vaddq_s32(EE3, EO3);
        int32x4_t E6 = vsubq_s32(EE3, EO3);
        int32x4_t E7 = vsubq_s32(EE2, EO2);

        dest0 = vmovn_s32(vrshlq_s32(vaddq_s32(E0, O0), sh1));
        dest1 = vmovn_s32(vrshlq_s32(vaddq_s32(E1, O1), sh1));
        dest2 = vmovn_s32(vrshlq_s32(vaddq_s32(E2, O2), sh1));
        dest3 = vmovn_s32(vrshlq_s32(vaddq_s32(E3, O3), sh1));
        dest4 = vmovn_s32(vrshlq_s32(vsubq_s32(E0, O0), sh1));
        dest5 = vmovn_s32(vrshlq_s32(vsubq_s32(E1, O1), sh1));
        dest6 = vmovn_s32(vrshlq_s32(vsubq_s32(E2, O2), sh1));
        dest7 = vmovn_s32(vrshlq_s32(vsubq_s32(E3, O3), sh1));
        dest8 = vmovn_s32(vrshlq_s32(vaddq_s32(E4, O4), sh1));
        dest9 = vmovn_s32(vrshlq_s32(vaddq_s32(E5, O5), sh1));
        dest10 = vmovn_s32(vrshlq_s32(vaddq_s32(E6, O6), sh1));
        dest11 = vmovn_s32(vrshlq_s32(vaddq_s32(E7, O7), sh1));
        dest12 = vmovn_s32(vrshlq_s32(vsubq_s32(E4, O4), sh1));
        dest13 = vmovn_s32(vrshlq_s32(vsubq_s32(E5, O5), sh1));
        dest14 = vmovn_s32(vrshlq_s32(vsubq_s32(E6, O6), sh1));
        dest15 = vmovn_s32(vrshlq_s32(vsubq_s32(E7, O7), sh1));

        int16x4_t t0 = vzip1_s16(dest0, dest1);
        int16x4_t t1 = vzip1_s16(dest2, dest3);
        int16x4_t t2 = vzip2_s16(dest0, dest1);
        int16x4_t t3 = vzip2_s16(dest2, dest3);
        int16x4_t t4 = vzip1_s16(dest8, dest9);
        int16x4_t t5 = vzip1_s16(dest10, dest11);
        int16x4_t t6 = vzip2_s16(dest8, dest9);
        int16x4_t t7 = vzip2_s16(dest10, dest11);

        dest0 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t0), vreinterpret_s32_s16(t1)));
        dest1 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t0), vreinterpret_s32_s16(t1)));
        dest2 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t2), vreinterpret_s32_s16(t3)));
        dest3 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t2), vreinterpret_s32_s16(t3)));
        dest8 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t4), vreinterpret_s32_s16(t5)));
        dest9 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t4), vreinterpret_s32_s16(t5)));
        dest10 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t6), vreinterpret_s32_s16(t7)));
        dest11 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t6), vreinterpret_s32_s16(t7)));

        int16x4_t t8 = vzip1_s16(dest5, dest4);
        int16x4_t t9 = vzip1_s16(dest7, dest6);
        int16x4_t t10 = vzip2_s16(dest5, dest4);
        int16x4_t t11 = vzip2_s16(dest7, dest6);
        int16x4_t t12 = vzip1_s16(dest13, dest12);
        int16x4_t t13 = vzip1_s16(dest15, dest14);
        int16x4_t t14 = vzip2_s16(dest13, dest12);
        int16x4_t t15 = vzip2_s16(dest15, dest14);

        dest4 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t9), vreinterpret_s32_s16(t8)));
        dest5 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t9), vreinterpret_s32_s16(t8)));
        dest6 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t11), vreinterpret_s32_s16(t10)));
        dest7 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t11), vreinterpret_s32_s16(t10)));
        dest12 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t13), vreinterpret_s32_s16(t12)));
        dest13 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t13), vreinterpret_s32_s16(t12)));
        dest14 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t15), vreinterpret_s32_s16(t14)));
        dest15 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t15), vreinterpret_s32_s16(t14)));
    }

    //DCT Pass 2
    {
        int32x4_t temp1 = vaddq_s32(vmull_n_s16(dest1, OAPV_INVTX_COEF_0), vmull_n_s16(dest3, OAPV_INVTX_COEF_1));
        int32x4_t temp2 = vsubq_s32(vmull_n_s16(dest1, OAPV_INVTX_COEF_1), vmull_n_s16(dest3, OAPV_INVTX_COEF_3));

        int32x4_t temp3 = vsubq_s32(vmull_n_s16(dest1, OAPV_INVTX_COEF_2), vmull_n_s16(dest3, OAPV_INVTX_COEF_0));
        int32x4_t temp4 = vsubq_s32(vmull_n_s16(dest1, OAPV_INVTX_COEF_3), vmull_n_s16(dest3, OAPV_INVTX_COEF_2));

        int32x4_t temp5 = vaddq_s32(vmull_n_s16(dest9, OAPV_INVTX_COEF_2), vmull_n_s16(dest11, OAPV_INVTX_COEF_3));
        int32x4_t temp6 = vaddq_s32(vmull_n_s16(dest9, OAPV_INVTX_COEF_0), vmull_n_s16(dest11, OAPV_INVTX_COEF_2));
        temp6 = vnegq_s32(temp6);

        int32x4_t temp7 = vaddq_s32(vmull_n_s16(dest9, OAPV_INVTX_COEF_3), vmull_n_s16(dest11, OAPV_INVTX_COEF_1));
        int32x4_t temp8 = vsubq_s32(vmull_n_s16(dest9, OAPV_INVTX_COEF_1), vmull_n_s16(dest11, OAPV_INVTX_COEF_0));

        int32x4_t temp9 = vaddq_s32(vmull_n_s16(dest5, OAPV_INVTX_COEF_0), vmull_n_s16(dest7, OAPV_INVTX_COEF_1));
        int32x4_t temp10 = vsubq_s32(vmull_n_s16(dest5, OAPV_INVTX_COEF_1), vmull_n_s16(dest7, OAPV_INVTX_COEF_3));

        int32x4_t temp11 = vsubq_s32(vmull_n_s16(dest5, OAPV_INVTX_COEF_2), vmull_n_s16(dest7, OAPV_INVTX_COEF_0));
        int32x4_t temp12 = vsubq_s32(vmull_n_s16(dest5, OAPV_INVTX_COEF_3), vmull_n_s16(dest7, OAPV_INVTX_COEF_2));

        int32x4_t temp13 = vaddq_s32(vmull_n_s16(dest13, OAPV_INVTX_COEF_2), vmull_n_s16(dest15, OAPV_INVTX_COEF_3));
        int32x4_t temp14 = vaddq_s32(vmull_n_s16(dest13, OAPV_INVTX_COEF_0), vmull_n_s16(dest15, OAPV_INVTX_COEF_2));
        temp14 = vnegq_s32(temp14);

        int32x4_t temp15 = vaddq_s32(vmull_n_s16(dest13, OAPV_INVTX_COEF_3), vmull_n_s16(dest15, OAPV_INVTX_COEF_1));
        int32x4_t temp16 = vsubq_s32(vmull_n_s16(dest13, OAPV_INVTX_COEF_1), vmull_n_s16(dest15, OAPV_INVTX_COEF_0));

        int32x4_t O0 = vaddq_s32(temp1, temp5);
        int32x4_t O1 = vaddq_s32(temp2, temp6);
        int32x4_t O2 = vaddq_s32(temp3, temp7);
        int32x4_t O3 = vaddq_s32(temp4, temp8);
        int32x4_t O4 = vaddq_s32(temp9, temp13);
        int32x4_t O5 = vaddq_s32(temp10, temp14);
        int32x4_t O6 = vaddq_s32(temp11, temp15);
        int32x4_t O7 = vaddq_s32(temp12, temp16);

        int32x4_t EO0 = vaddq_s32(vmull_n_s16(dest2, OAPV_INVTX_COEF_5), vmull_n_s16(dest10, OAPV_INVTX_COEF_6));
        int32x4_t EO1 = vsubq_s32(vmull_n_s16(dest2, OAPV_INVTX_COEF_6), vmull_n_s16(dest10, OAPV_INVTX_COEF_5));
        int32x4_t EE0 = vaddq_s32(vshll_n_s16(dest0, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(dest8, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EE1 = vsubq_s32(vshll_n_s16(dest0, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(dest8, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EO2 = vaddq_s32(vmull_n_s16(dest6, OAPV_INVTX_COEF_5), vmull_n_s16(dest14, OAPV_INVTX_COEF_6));
        int32x4_t EO3 = vsubq_s32(vmull_n_s16(dest6, OAPV_INVTX_COEF_6), vmull_n_s16(dest14, OAPV_INVTX_COEF_5));
        int32x4_t EE2 = vaddq_s32(vshll_n_s16(dest4, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(dest12, OAPV_INVTX_COEF_4_LOG2));
        int32x4_t EE3 = vsubq_s32(vshll_n_s16(dest4, OAPV_INVTX_COEF_4_LOG2), vshll_n_s16(dest12, OAPV_INVTX_COEF_4_LOG2));

        int32x4_t E0 = vaddq_s32(EE0, EO0);
        int32x4_t E1 = vaddq_s32(EE1, EO1);
        int32x4_t E2 = vsubq_s32(EE1, EO1);
        int32x4_t E3 = vsubq_s32(EE0, EO0);
        int32x4_t E4 = vaddq_s32(EE2, EO2);
        int32x4_t E5 = vaddq_s32(EE3, EO3);
        int32x4_t E6 = vsubq_s32(EE3, EO3);
        int32x4_t E7 = vsubq_s32(EE2, EO2);

        int16x4_t v_src_0 = vmovn_s32(vrshlq_s32(vaddq_s32(E0, O0), sh2));
        int16x4_t v_src_1 = vmovn_s32(vrshlq_s32(vaddq_s32(E1, O1), sh2));
        int16x4_t v_src_2 = vmovn_s32(vrshlq_s32(vaddq_s32(E2, O2), sh2));
        int16x4_t v_src_3 = vmovn_s32(vrshlq_s32(vaddq_s32(E3, O3), sh2));
        int16x4_t v_src_4 = vmovn_s32(vrshlq_s32(vsubq_s32(E0, O0), sh2));
        int16x4_t v_src_5 = vmovn_s32(vrshlq_s32(vsubq_s32(E1, O1), sh2));
        int16x4_t v_src_6 = vmovn_s32(vrshlq_s32(vsubq_s32(E2, O2), sh2));
        int16x4_t v_src_7 = vmovn_s32(vrshlq_s32(vsubq_s32(E3, O3), sh2));
        int16x4_t v_src_8 = vmovn_s32(vrshlq_s32(vaddq_s32(E4, O4), sh2));
        int16x4_t v_src_9 = vmovn_s32(vrshlq_s32(vaddq_s32(E5, O5), sh2));
        int16x4_t v_src_10 = vmovn_s32(vrshlq_s32(vaddq_s32(E6, O6), sh2));
        int16x4_t v_src_11 = vmovn_s32(vrshlq_s32(vaddq_s32(E7, O7), sh2));
        int16x4_t v_src_12 = vmovn_s32(vrshlq_s32(vsubq_s32(E4, O4), sh2));
        int16x4_t v_src_13 = vmovn_s32(vrshlq_s32(vsubq_s32(E5, O5), sh2));
        int16x4_t v_src_14 = vmovn_s32(vrshlq_s32(vsubq_s32(E6, O6), sh2));
        int16x4_t v_src_15 = vmovn_s32(vrshlq_s32(vsubq_s32(E7, O7), sh2));

        int16x4_t t0 = vzip1_s16(v_src_0, v_src_1);
        int16x4_t t1 = vzip1_s16(v_src_2, v_src_3);
        int16x4_t t2 = vzip2_s16(v_src_0, v_src_1);
        int16x4_t t3 = vzip2_s16(v_src_2, v_src_3);
        int16x4_t t4 = vzip1_s16(v_src_8, v_src_9);
        int16x4_t t5 = vzip1_s16(v_src_10, v_src_11);
        int16x4_t t6 = vzip2_s16(v_src_8, v_src_9);
        int16x4_t t7 = vzip2_s16(v_src_10, v_src_11);

        v_src_0 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t0), vreinterpret_s32_s16(t1)));
        v_src_1 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t0), vreinterpret_s32_s16(t1)));
        v_src_2 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t2), vreinterpret_s32_s16(t3)));
        v_src_3 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t2), vreinterpret_s32_s16(t3)));
        v_src_8 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t4), vreinterpret_s32_s16(t5)));
        v_src_9 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t4), vreinterpret_s32_s16(t5)));
        v_src_10 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t6), vreinterpret_s32_s16(t7)));
        v_src_11 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t6), vreinterpret_s32_s16(t7)));

        int16x4_t t8 = vzip1_s16(v_src_5, v_src_4);
        int16x4_t t9 = vzip1_s16(v_src_7, v_src_6);
        int16x4_t t10 = vzip2_s16(v_src_5, v_src_4);
        int16x4_t t11 = vzip2_s16(v_src_7, v_src_6);
        int16x4_t t12 = vzip1_s16(v_src_13, v_src_12);
        int16x4_t t13 = vzip1_s16(v_src_15, v_src_14);
        int16x4_t t14 = vzip2_s16(v_src_13, v_src_12);
        int16x4_t t15 = vzip2_s16(v_src_15, v_src_14);

        v_src_4 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t9), vreinterpret_s32_s16(t8)));
        v_src_5 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t9), vreinterpret_s32_s16(t8)));
        v_src_6 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t11), vreinterpret_s32_s16(t10)));
        v_src_7 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t11), vreinterpret_s32_s16(t10)));
        v_src_12 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t13), vreinterpret_s32_s16(t12)));
        v_src_13 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t13), vreinterpret_s32_s16(t12)));
        v_src_14 = vreinterpret_s16_s32(vzip1_s32(vreinterpret_s32_s16(t15), vreinterpret_s32_s16(t14)));
        v_src_15 = vreinterpret_s16_s32(vzip2_s32(vreinterpret_s32_s16(t15), vreinterpret_s32_s16(t14)));

        int16x8_t v_src_0_4 = vcombine_s16(v_src_0, v_src_4);
        int16x8_t v_src_1_5 = vcombine_s16(v_src_1, v_src_5);
        int16x8_t v_src_2_6 = vcombine_s16(v_src_2, v_src_6);
        int16x8_t v_src_3_7 = vcombine_s16(v_src_3, v_src_7);
        int16x8_t v_src_8_12 = vcombine_s16(v_src_8, v_src_12);
        int16x8_t v_src_9_13 = vcombine_s16(v_src_9, v_src_13);
        int16x8_t v_src_10_14 = vcombine_s16(v_src_10, v_src_14);
        int16x8_t v_src_11_15 = vcombine_s16(v_src_11, v_src_15);

        vst1q_s16(src, v_src_0_4);
        vst1q_s16(src + 8, v_src_1_5);
        vst1q_s16(src + 16, v_src_2_6);
        vst1q_s16(src + 24, v_src_3_7);
        vst1q_s16(src + 32, v_src_8_12);
        vst1q_s16(src + 40, v_src_9_13);
        vst1q_s16(src + 48, v_src_10_14);
        vst1q_s16(src + 56, v_src_11_15);
    }
}

const oapv_fn_itx_t oapv_tbl_fn_itx_neon[2] =
    {
        oapv_itx_pb8b_opt_neon,
            NULL
};

static int oapv_quant_neon(s16* coef, u8 qp, const int q_matrix[OAPV_BLK_D], int log2_w, int log2_h, int bit_depth, int deadzone_offset)
{
    s64 offset;
    int shift;
    int tr_shift;

    int log2_size = (log2_w + log2_h) >> 1;
    tr_shift = MAX_TX_DYNAMIC_RANGE - bit_depth - log2_size;
    shift = QUANT_SHIFT + tr_shift + (qp / 6);
    offset = (s64)deadzone_offset << (shift - 9);
    int pixels=(1 << (log2_w + log2_h));

    int i;
    int16x8_t coef_row;
    int64x2_t offset_vector     = vdupq_n_s64(offset);
    int64x2_t shift_vector      = vdupq_n_s64(-shift);
    uint16x8_t zero_vector      = vdupq_n_s16(0);

    for (i = 0; i < pixels; i+=8)
    {
        // Load one coef row
        coef_row = vld1q_s16(coef+i);

		// Extract coef signs and construct abs coef-vec
        uint16x8_t sign_mask   = vcltq_s16(coef_row, zero_vector);
        int16x8_t coef_row_abs = vabsq_s16(coef_row);

        // Split abs coef-vec and unpack to s32
        int32x4_t coef_low_32b  = vmovl_s16(vget_low_s16(coef_row_abs));
        int32x4_t coef_high_32b = vmovl_high_s16(coef_row_abs);

        // Load q_matrix elements
        int32x4_t quant_matrix_low  = vld1q_s32(q_matrix + i);
        int32x4_t quant_matrix_high = vld1q_s32(q_matrix + i + 4);

        // Multiply 2X: 32-bit coef with 32-bit q_matrix and add 64-bit offset_vector to store result as 64-bit
        int64x2_t coef_low_32b_first_half   = vmlal_s32(offset_vector, vget_low_s32 (coef_low_32b), vget_low_s32 (quant_matrix_low));
        int64x2_t coef_low_32b_second_half  = vmlal_s32(offset_vector, vget_high_s32(coef_low_32b), vget_high_s32(quant_matrix_low));

        int64x2_t coef_high_32b_first_half  = vmlal_s32(offset_vector, vget_low_s32 (coef_high_32b), vget_low_s32 (quant_matrix_high));
        int64x2_t coef_high_32b_second_half = vmlal_s32(offset_vector, vget_high_s32(coef_high_32b), vget_high_s32(quant_matrix_high));

        // Shift 64-bit results
        coef_low_32b_first_half   = vshlq_s64(coef_low_32b_first_half, shift_vector);
        coef_low_32b_second_half  = vshlq_s64(coef_low_32b_second_half, shift_vector);
        coef_high_32b_first_half  = vshlq_s64(coef_high_32b_first_half, shift_vector);
        coef_high_32b_second_half = vshlq_s64(coef_high_32b_second_half, shift_vector);

        // Combine 2X: 64x2 registers into one 32x4 register
        coef_low_32b  = vcombine_u32(vmovn_s64(coef_low_32b_first_half),  vmovn_s64(coef_low_32b_second_half));
        coef_high_32b = vcombine_u32(vmovn_s64(coef_high_32b_first_half), vmovn_s64(coef_high_32b_second_half));

        // Combine 2X: 32x4 registers into one 16x8 register
        int16x8_t output_vector = vcombine_u16(vmovn_s32(coef_low_32b), vmovn_s32(coef_high_32b));

        // Apply extracted coef sign to result
        output_vector = vbslq_s16(sign_mask,  vnegq_s16(output_vector), output_vector);

        // Store result row into buffer
        vst1q_s16(coef + i, output_vector);
    }
    return OAPV_OK;
}


const oapv_fn_quant_t oapv_tbl_fn_quant_neon[2] =
{
    oapv_quant_neon,
        NULL
};

#endif /* ARM_NEON */
