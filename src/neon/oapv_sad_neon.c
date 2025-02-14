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
#include <math.h>

#if ARM_NEON

#if defined(__aarch64__)

#define VADDVQ_S32_(s, v) \
    s += vaddvq_s32(v);
#define VADDVQ_S64_(s, v) \
    s += vaddvq_s64(v);

#else // __aarch64__

#define VADDVQ_S32_(s, v) \
    { \
        int32x2_t tmp = vadd_s32(vget_low_s32(v), vget_high_s32(v)); \
        tmp = vpadd_s32(tmp, tmp); \
        s += vget_lane_s32(tmp, 0); \
    }
#define VADDVQ_S64_(s, v) \
    s += vgetq_lane_s64(v, 0) + vgetq_lane_s64(v, 1);
#define vabal_high_s16(a, b, c) \
    vabal_s16(a, vget_high_s16(b), vget_high_s16(c))
#define vsubl_high_s16(a, b) \
    vsubl_s16(vget_high_s16(a), vget_high_s16(b))
#define vmlal_high_s32(a, b, c) \
    vmlal_s32(a, vget_high_s32(b), vget_high_s32(c))
#define vzip1q_s16(a, b) \
    vorrq_s32(vshll_n_s16(vget_low_s16(a), 16), vshll_n_s16(vget_low_s16(b), 0))
#define vzip1q_s32(a, b) \
    vorrq_s64(vshll_n_s32(vget_low_s32(a), 32), vshll_n_s32(vget_low_s32(b), 0))
#define vzip1q_s64(a, b) \
    vcombine_s64(vget_low_s64(a), vget_low_s64(b))
#define vzip2q_s16(a, b) \
    vorrq_s32(vshll_n_s16(vget_high_s16(a), 16), vshll_n_s16(vget_high_s16(b), 0))
#define vzip2q_s32(a, b) \
    vorrq_s64(vshll_n_s32(vget_high_s32(a), 32), vshll_n_s32(vget_high_s32(b), 0))
#define vzip2q_s64(a, b) \
    vcombine_s64(vget_high_s64(a), vget_high_s64(b))

#endif // __aarch64__

/* SAD for 16bit **************************************************************/
static int sad_16b_neon_8x2n(int w, int h, void *src1, void *src2, int s_src1, int s_src2)
{
    int sad = 0;
    s16* s1 = (s16*) src1;
    s16* s2 = (s16*) src2;
    int16x8_t s1_vector, s2_vector;
    int32x4_t  sad_vector = vdupq_n_s32(0);
    // Loop unrolling
#ifndef _MSC_VER
#pragma GCC unroll 8
#endif
    for (s32 i = 0; i < 8; ++i)
    { // Row
        // Loading one row (8 elements) each of src1 and src_2
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        // Getting absolute difference s1_vector and s2_vector and storing in 32 bits
        sad_vector = vabal_s16(sad_vector, vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        sad_vector = vabal_high_s16(sad_vector, s1_vector, s2_vector);
    }
    // Adding all the elments in sad vector
    VADDVQ_S32_(sad, sad_vector)
    return sad;
}

const oapv_fn_sad_t oapv_tbl_fn_sad_16b_neon[2] = {
    sad_16b_neon_8x2n,
    NULL
};

/* SSD ***********************************************************************/
static s64 ssd_16b_neon_8x8(int w, int h, void *src1, void *src2, int s_src1, int s_src2)
{
    s64 ssd = 0;
    s16* s1 = (s16*) src1;
    s16* s2 = (s16*) src2;
    int16x8_t s1_vector, s2_vector;
    int32x4_t diff1, diff2;
    int32x2_t diff1_low, diff2_low;
    int64x2_t sq_diff = vdupq_n_s64(0);
    // Loop unrolling
#ifndef _MSC_VER
#pragma GCC unroll 8
#endif
    for (s32 i = 0; i < 8; ++i)
    { // Row
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff = vmlal_s32(sq_diff, diff1_low, diff1_low);
        sq_diff = vmlal_high_s32(sq_diff, diff1, diff1);
        sq_diff = vmlal_s32(sq_diff, diff2_low, diff2_low);
        sq_diff = vmlal_high_s32(sq_diff, diff2, diff2);
    }
    VADDVQ_S64_(ssd, sq_diff)
    return ssd;
}

const oapv_fn_ssd_t oapv_tbl_fn_ssd_16b_neon[2] =
    {
        ssd_16b_neon_8x8,
            NULL};

/* DIFF **********************************************************************/
static void diff_16b_neon_8x8(int w, int h, void *src1, void *src2, int s_src1, int s_src2, int s_diff, s16 *diff)
{
    s16* s1 = (s16*) src1;
    s16* s2 = (s16*) src2;
    int16x8_t s1_vector, s2_vector, diff_vector;
    // Loop unrolling
#ifndef _MSC_VER
#pragma GCC unroll 8
#endif
    for (s32 i = 0; i < 8; ++i)
    { // Row
        // Loading one row (8 elements) each of src1 and src_2
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        // Subtracting s1_vector from s2_vector
        diff_vector = vsubq_s16(s1_vector, s2_vector);

        // Storing the result in diff
        vst1q_s16(diff, diff_vector);
        diff += s_diff;
    }
}

const oapv_fn_diff_t oapv_tbl_fn_diff_16b_neon[2] = {
    diff_16b_neon_8x8,
    NULL
};

int oapv_dc_removed_had8x8_neon(pel* org, int s_org)
{
    int satd = 0;
    /* all 128 bit registers are named with a suffix mxnb, where m is the */
    /* number of n bits packed in the register                            */

    int16x8_t src0_8x16b, src1_8x16b, src2_8x16b, src3_8x16b;
    int16x8_t src4_8x16b, src5_8x16b, src6_8x16b, src7_8x16b;
    int16x8_t pred0_8x16b, pred1_8x16b, pred2_8x16b, pred3_8x16b;
    int16x8_t pred4_8x16b, pred5_8x16b, pred6_8x16b, pred7_8x16b;
    int16x8_t out0_8x16b, out1_8x16b, out2_8x16b, out3_8x16b;
    int16x8_t out4_8x16b, out5_8x16b, out6_8x16b, out7_8x16b;

    src0_8x16b = (vld1q_s16(org));
    org += s_org;
    src1_8x16b = (vld1q_s16(org));
    org += s_org;
    src2_8x16b = (vld1q_s16(org));
    org += s_org;
    src3_8x16b = (vld1q_s16(org));
    org += s_org;
    src4_8x16b = (vld1q_s16(org));
    org += s_org;
    src5_8x16b = (vld1q_s16(org));
    org += s_org;
    src6_8x16b = (vld1q_s16(org));
    org += s_org;
    src7_8x16b = (vld1q_s16(org));
    org += s_org;

    /**************** 8x8 horizontal transform *******************************/
    /***********************    8x8 16 bit Transpose  ************************/

    out3_8x16b = vcombine_s16(vget_low_s16(src0_8x16b), vget_low_s16(src1_8x16b));
    out7_8x16b = vcombine_s16(vget_high_s16(src0_8x16b), vget_high_s16(src1_8x16b));

    pred0_8x16b = vcombine_s16(vget_low_s16(src2_8x16b), vget_low_s16(src3_8x16b));
    src2_8x16b = vcombine_s16(vget_high_s16(src2_8x16b), vget_high_s16(src3_8x16b));

    out2_8x16b = vcombine_s16(vget_low_s16(src4_8x16b), vget_low_s16(src5_8x16b));
    pred7_8x16b = vcombine_s16(vget_high_s16(src4_8x16b), vget_high_s16(src5_8x16b));

    pred3_8x16b = vcombine_s16(vget_low_s16(src6_8x16b), vget_low_s16(src7_8x16b));
    src6_8x16b = vcombine_s16(vget_high_s16(src6_8x16b), vget_high_s16(src7_8x16b));


    out1_8x16b = vzip1q_s32(out3_8x16b, pred0_8x16b);
    out3_8x16b = vzip2q_s32(out3_8x16b, pred0_8x16b);

    pred1_8x16b = vzip1q_s32(out2_8x16b, pred3_8x16b);
    pred3_8x16b = vzip2q_s32(out2_8x16b, pred3_8x16b);

    out5_8x16b = vzip1q_s32(out7_8x16b, src2_8x16b);
    out7_8x16b = vzip2q_s32(out7_8x16b, src2_8x16b);

    pred5_8x16b = vzip1q_s32(pred7_8x16b, src6_8x16b);
    pred7_8x16b = vzip2q_s32(pred7_8x16b, src6_8x16b);

    out0_8x16b = vzip1q_s64(out1_8x16b,pred1_8x16b);
    out1_8x16b = vzip2q_s64(out1_8x16b,pred1_8x16b);
    out2_8x16b = vzip1q_s64(out3_8x16b,pred3_8x16b);
    out3_8x16b = vzip2q_s64(out3_8x16b,pred3_8x16b);
    out4_8x16b = vzip1q_s64(out5_8x16b,pred5_8x16b);
    out5_8x16b = vzip2q_s64(out5_8x16b,pred5_8x16b);
    out6_8x16b = vzip1q_s64(out7_8x16b,pred7_8x16b);
    out7_8x16b = vzip2q_s64(out7_8x16b,pred7_8x16b);

    /**********************   8x8 16 bit Transpose End   *********************/

    /* r0 + r1 */
    pred0_8x16b = vaddq_s16(out0_8x16b, out1_8x16b);
    /* r2 + r3 */
    pred2_8x16b = vaddq_s16(out2_8x16b, out3_8x16b);
    /* r4 + r5 */
    pred4_8x16b = vaddq_s16(out4_8x16b, out5_8x16b);
    /* r6 + r7 */
    pred6_8x16b = vaddq_s16(out6_8x16b, out7_8x16b);


    /* r0 + r1 + r2 + r3 */
    pred1_8x16b = vaddq_s16(pred0_8x16b, pred2_8x16b);
    /* r4 + r5 + r6 + r7 */
    pred5_8x16b = vaddq_s16(pred4_8x16b, pred6_8x16b);
    /* r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7 */
    src0_8x16b = vaddq_s16(pred1_8x16b, pred5_8x16b);
    /* r0 + r1 + r2 + r3 - r4 - r5 - r6 - r7 */
    src4_8x16b = vsubq_s16(pred1_8x16b, pred5_8x16b);

    /* r0 + r1 - r2 - r3 */
    pred1_8x16b = vsubq_s16(pred0_8x16b, pred2_8x16b);
    /* r4 + r5 - r6 - r7 */
    pred5_8x16b = vsubq_s16(pred4_8x16b, pred6_8x16b);
    /* r0 + r1 - r2 - r3 + r4 + r5 - r6 - r7 */
    src2_8x16b = vaddq_s16(pred1_8x16b, pred5_8x16b);
    /* r0 + r1 - r2 - r3 - r4 - r5 + r6 + r7 */
    src6_8x16b = vsubq_s16(pred1_8x16b, pred5_8x16b);

    /* r0 - r1 */
    pred0_8x16b = vsubq_s16(out0_8x16b, out1_8x16b);
    /* r2 - r3 */
    pred2_8x16b = vsubq_s16(out2_8x16b, out3_8x16b);
    /* r4 - r5 */
    pred4_8x16b = vsubq_s16(out4_8x16b, out5_8x16b);
    /* r6 - r7 */
    pred6_8x16b = vsubq_s16(out6_8x16b, out7_8x16b);

    /* r0 - r1 + r2 - r3 */
    pred1_8x16b = vaddq_s16(pred0_8x16b, pred2_8x16b);
    /* r4 - r5 + r6 - r7 */
    pred5_8x16b = vaddq_s16(pred4_8x16b, pred6_8x16b);
    /* r0 - r1 + r2 - r3 + r4 - r5 + r6 - r7 */
    src1_8x16b = vaddq_s16(pred1_8x16b, pred5_8x16b);
    /* r0 - r1 + r2 - r3 - r4 + r5 - r6 + r7 */
    src5_8x16b = vsubq_s16(pred1_8x16b, pred5_8x16b);

    /* r0 - r1 - r2 + r3 */
    pred1_8x16b = vsubq_s16(pred0_8x16b, pred2_8x16b);
    /* r4 - r5 - r6 + r7 */
    pred5_8x16b = vsubq_s16(pred4_8x16b, pred6_8x16b);
    /* r0 - r1 - r2 + r3 + r4 - r5 - r6 + r7 */
    src3_8x16b = vaddq_s16(pred1_8x16b, pred5_8x16b);
    /* r0 - r1 - r2 + r3 - r4 + r5 + r6 - r7 */
    src7_8x16b = vsubq_s16(pred1_8x16b, pred5_8x16b);


    /***********************    8x8 16 bit Transpose  ************************/
    out3_8x16b = vzip1q_s16(src0_8x16b, src1_8x16b);
    pred0_8x16b = vzip1q_s16(src2_8x16b, src3_8x16b);
    out2_8x16b = vzip1q_s16(src4_8x16b, src5_8x16b);
    pred3_8x16b = vzip1q_s16(src6_8x16b, src7_8x16b);
    out7_8x16b = vzip2q_s16(src0_8x16b, src1_8x16b);
    src2_8x16b = vzip2q_s16(src2_8x16b, src3_8x16b);
    pred7_8x16b = vzip2q_s16(src4_8x16b, src5_8x16b);
    src6_8x16b = vzip2q_s16(src6_8x16b, src7_8x16b);

    out1_8x16b = vzip1q_s32(out3_8x16b, pred0_8x16b);
    out3_8x16b = vzip2q_s32(out3_8x16b, pred0_8x16b);

    pred1_8x16b = vzip1q_s32(out2_8x16b, pred3_8x16b);
    pred3_8x16b = vzip2q_s32(out2_8x16b, pred3_8x16b);

    out5_8x16b = vzip1q_s32(out7_8x16b, src2_8x16b);
    out7_8x16b = vzip2q_s32(out7_8x16b, src2_8x16b);

    pred5_8x16b = vzip1q_s32(pred7_8x16b, src6_8x16b);
    pred7_8x16b = vzip2q_s32(pred7_8x16b, src6_8x16b);

    src0_8x16b = vzip1q_s64(out1_8x16b,pred1_8x16b);
    src1_8x16b = vzip2q_s64(out1_8x16b,pred1_8x16b);
    src2_8x16b = vzip1q_s64(out3_8x16b,pred3_8x16b);
    src3_8x16b = vzip2q_s64(out3_8x16b,pred3_8x16b);
    src4_8x16b = vzip1q_s64(out5_8x16b,pred5_8x16b);
    src5_8x16b = vzip2q_s64(out5_8x16b,pred5_8x16b);
    src6_8x16b = vzip1q_s64(out7_8x16b,pred7_8x16b);
    src7_8x16b = vzip2q_s64(out7_8x16b,pred7_8x16b);

    /**********************   8x8 16 bit Transpose End   *********************/
    /**************** 8x8 horizontal transform *******************************/
    {
        int16x8_t out0a_8x16b, out1a_8x16b, out2a_8x16b, out3a_8x16b;
        int16x8_t out4a_8x16b, out5a_8x16b, out6a_8x16b, out7a_8x16b;
        int16x8_t tmp0_8x16b, tmp1_8x16b, tmp2_8x16b, tmp3_8x16b;
        int16x8_t tmp4_8x16b, tmp5_8x16b, tmp6_8x16b, tmp7_8x16b;

        /************************* 8x8 Vertical Transform*************************/
        tmp0_8x16b = vcombine_s16(vget_high_s16(src0_8x16b), vcreate_s32(0));
        tmp1_8x16b = vcombine_s16(vget_high_s16(src1_8x16b), vcreate_s32(0));
        tmp2_8x16b = vcombine_s16(vget_high_s16(src2_8x16b), vcreate_s32(0));
        tmp3_8x16b = vcombine_s16(vget_high_s16(src3_8x16b), vcreate_s32(0));
        tmp4_8x16b = vcombine_s16(vget_high_s16(src4_8x16b), vcreate_s32(0));
        tmp5_8x16b = vcombine_s16(vget_high_s16(src5_8x16b), vcreate_s32(0));
        tmp6_8x16b = vcombine_s16(vget_high_s16(src6_8x16b), vcreate_s32(0));
        tmp7_8x16b = vcombine_s16(vget_high_s16(src7_8x16b), vcreate_s32(0));

        /*************************First 4 pixels ********************************/

        src0_8x16b = vmovl_s16(vget_low_s16(src0_8x16b));
        src1_8x16b = vmovl_s16(vget_low_s16(src1_8x16b));
        src2_8x16b = vmovl_s16(vget_low_s16(src2_8x16b));
        src3_8x16b = vmovl_s16(vget_low_s16(src3_8x16b));
        src4_8x16b = vmovl_s16(vget_low_s16(src4_8x16b));
        src5_8x16b = vmovl_s16(vget_low_s16(src5_8x16b));
        src6_8x16b = vmovl_s16(vget_low_s16(src6_8x16b));
        src7_8x16b = vmovl_s16(vget_low_s16(src7_8x16b));

        /* r0 + r1 */
        pred0_8x16b = vaddq_s32(src0_8x16b, src1_8x16b);
        /* r2 + r3 */
        pred2_8x16b = vaddq_s32(src2_8x16b, src3_8x16b);
        /* r4 + r5 */
        pred4_8x16b = vaddq_s32(src4_8x16b, src5_8x16b);
        /* r6 + r7 */
        pred6_8x16b = vaddq_s32(src6_8x16b, src7_8x16b);

        /* r0 + r1 + r2 + r3 */
        pred1_8x16b = vaddq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 + r5 + r6 + r7 */
        pred5_8x16b = vaddq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7 */
        out0_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 + r1 + r2 + r3 - r4 - r5 - r6 - r7 */
        out4_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 + r1 - r2 - r3 */
        pred1_8x16b = vsubq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 + r5 - r6 - r7 */
        pred5_8x16b = vsubq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 + r1 - r2 - r3 + r4 + r5 - r6 - r7 */
        out2_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 + r1 - r2 - r3 - r4 - r5 + r6 + r7 */
        out6_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 - r1 */
        pred0_8x16b = vsubq_s32(src0_8x16b, src1_8x16b);
        /* r2 - r3 */
        pred2_8x16b = vsubq_s32(src2_8x16b, src3_8x16b);
        /* r4 - r5 */
        pred4_8x16b = vsubq_s32(src4_8x16b, src5_8x16b);
        /* r6 - r7 */
        pred6_8x16b = vsubq_s32(src6_8x16b, src7_8x16b);

        /* r0 - r1 + r2 - r3 */
        pred1_8x16b = vaddq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 - r5 + r6 - r7 */
        pred5_8x16b = vaddq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 - r1 + r2 - r3 + r4 - r5 + r6 - r7 */
        out1_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 - r1 + r2 - r3 - r4 + r5 - r6 + r7 */
        out5_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 - r1 - r2 + r3 */
        pred1_8x16b = vsubq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 - r5 - r6 + r7 */
        pred5_8x16b = vsubq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 - r1 - r2 + r3 + r4 - r5 - r6 + r7 */
        out3_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 - r1 - r2 + r3 - r4 + r5 + r6 - r7 */
        out7_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /*************************First 4 pixels ********************************/

        /**************************Next 4 pixels *******************************/
        src0_8x16b = vmovl_s16(vget_low_s16(tmp0_8x16b));
        src1_8x16b = vmovl_s16(vget_low_s16(tmp1_8x16b));
        src2_8x16b = vmovl_s16(vget_low_s16(tmp2_8x16b));
        src3_8x16b = vmovl_s16(vget_low_s16(tmp3_8x16b));
        src4_8x16b = vmovl_s16(vget_low_s16(tmp4_8x16b));
        src5_8x16b = vmovl_s16(vget_low_s16(tmp5_8x16b));
        src6_8x16b = vmovl_s16(vget_low_s16(tmp6_8x16b));
        src7_8x16b = vmovl_s16(vget_low_s16(tmp7_8x16b));

        /* r0 + r1 */
        pred0_8x16b = vaddq_s32(src0_8x16b, src1_8x16b);
        /* r2 + r3 */
        pred2_8x16b = vaddq_s32(src2_8x16b, src3_8x16b);
        /* r4 + r5 */
        pred4_8x16b = vaddq_s32(src4_8x16b, src5_8x16b);
        /* r6 + r7 */
        pred6_8x16b = vaddq_s32(src6_8x16b, src7_8x16b);

        /* r0 + r1 + r2 + r3 */
        pred1_8x16b = vaddq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 + r5 + r6 + r7 */
        pred5_8x16b = vaddq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7 */
        out0a_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 + r1 + r2 + r3 - r4 - r5 - r6 - r7 */
        out4a_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 + r1 - r2 - r3 */
        pred1_8x16b = vsubq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 + r5 - r6 - r7 */
        pred5_8x16b = vsubq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 + r1 - r2 - r3 + r4 + r5 - r6 - r7 */
        out2a_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 + r1 - r2 - r3 - r4 - r5 + r6 + r7 */
        out6a_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 - r1 */
        pred0_8x16b = vsubq_s32(src0_8x16b, src1_8x16b);
        /* r2 - r3 */
        pred2_8x16b = vsubq_s32(src2_8x16b, src3_8x16b);
        /* r4 - r5 */
        pred4_8x16b = vsubq_s32(src4_8x16b, src5_8x16b);
        /* r6 - r7 */
        pred6_8x16b = vsubq_s32(src6_8x16b, src7_8x16b);

        /* r0 - r1 + r2 - r3 */
        pred1_8x16b = vaddq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 - r5 + r6 - r7 */
        pred5_8x16b = vaddq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 - r1 + r2 - r3 + r4 - r5 + r6 - r7 */
        out1a_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 - r1 + r2 - r3 - r4 + r5 - r6 + r7 */
        out5a_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /* r0 - r1 - r2 + r3 */
        pred1_8x16b = vsubq_s32(pred0_8x16b, pred2_8x16b);
        /* r4 - r5 - r6 + r7 */
        pred5_8x16b = vsubq_s32(pred4_8x16b, pred6_8x16b);
        /* r0 - r1 - r2 + r3 + r4 - r5 - r6 + r7 */
        out3a_8x16b = vaddq_s32(pred1_8x16b, pred5_8x16b);
        /* r0 - r1 - r2 + r3 - r4 + r5 + r6 - r7 */
        out7a_8x16b = vsubq_s32(pred1_8x16b, pred5_8x16b);

        /**************************Next 4 pixels *******************************/
        /************************* 8x8 Vertical Transform*************************/

        /****************************SATD calculation ****************************/
        src0_8x16b = vabsq_s32(out0_8x16b);
        src1_8x16b = vabsq_s32(out1_8x16b);
        src2_8x16b = vabsq_s32(out2_8x16b);
        src3_8x16b = vabsq_s32(out3_8x16b);
        src4_8x16b = vabsq_s32(out4_8x16b);
        src5_8x16b = vabsq_s32(out5_8x16b);
        src6_8x16b = vabsq_s32(out6_8x16b);
        src7_8x16b = vabsq_s32(out7_8x16b);
        s32* p = (s32*)&src0_8x16b;
        p[0] = 0;

        VADDVQ_S32_(satd, src0_8x16b)
        VADDVQ_S32_(satd, src1_8x16b)
        VADDVQ_S32_(satd, src2_8x16b)
        VADDVQ_S32_(satd, src3_8x16b)
        VADDVQ_S32_(satd, src4_8x16b)
        VADDVQ_S32_(satd, src5_8x16b)
        VADDVQ_S32_(satd, src6_8x16b)
        VADDVQ_S32_(satd, src7_8x16b)

        src0_8x16b = vabsq_s32(out0a_8x16b);
        src1_8x16b = vabsq_s32(out1a_8x16b);
        src2_8x16b = vabsq_s32(out2a_8x16b);
        src3_8x16b = vabsq_s32(out3a_8x16b);
        src4_8x16b = vabsq_s32(out4a_8x16b);
        src5_8x16b = vabsq_s32(out5a_8x16b);
        src6_8x16b = vabsq_s32(out6a_8x16b);
        src7_8x16b = vabsq_s32(out7a_8x16b);

        VADDVQ_S32_(satd, src0_8x16b)
        VADDVQ_S32_(satd, src1_8x16b)
        VADDVQ_S32_(satd, src2_8x16b)
        VADDVQ_S32_(satd, src3_8x16b)
        VADDVQ_S32_(satd, src4_8x16b)
        VADDVQ_S32_(satd, src5_8x16b)
        VADDVQ_S32_(satd, src6_8x16b)
        VADDVQ_S32_(satd, src7_8x16b)

        satd = (satd + 2) >> 2;
        return satd;
    }
}
#endif /* ARM_NEON */
