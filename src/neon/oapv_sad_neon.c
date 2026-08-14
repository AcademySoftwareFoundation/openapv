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

/* SAD for 16bit **************************************************************/
/* SSD ***********************************************************************/
static s64 ssd_16b_neon_8x8(int w, int h, void *src1, void *src2, int s_src1, int s_src2)
{
    s64 ssd = 0;
    s16* s1 = (s16*) src1;
    s16* s2 = (s16*) src2;
    int16x8_t s1_vector, s2_vector;
    int32x4_t diff1, diff2;
    int32x2_t diff1_low, diff2_low;
    int64x2_t sq_diff1_low, sq_diff1_high, sq_diff2_low, sq_diff2_high, sq_diff;
    // Loop unrolling      
    { // Row 0
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);

        sq_diff = vaddq_s64(sq_diff1_low, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 1
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 2
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 3
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 4
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 5
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 6
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    { // Row 7
        s1_vector = vld1q_s16(s1);
        s1 += s_src1;
        s2_vector = vld1q_s16(s2);
        s2 += s_src2;

        diff1 = vsubl_s16(vget_low_s16(s1_vector), vget_low_s16(s2_vector));
        diff2 = vsubl_high_s16(s1_vector, s2_vector);
        diff1_low = vget_low_s32(diff1);
        diff2_low = vget_low_s32(diff2);

        sq_diff1_low = vmull_s32(diff1_low, diff1_low);
        sq_diff1_high = vmull_high_s32(diff1, diff1);
        sq_diff2_low = vmull_s32(diff2_low, diff2_low);
        sq_diff2_high = vmull_high_s32(diff2, diff2);
        
        sq_diff = vaddq_s64(sq_diff, sq_diff1_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff1_high);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_low);
        sq_diff = vaddq_s64(sq_diff, sq_diff2_high);
    }
    ssd += vaddvq_s64(sq_diff);
    return ssd;
}

const oapv_fn_ssd_t oapv_tbl_fn_ssd_16b_neon[2] =
    {
        ssd_16b_neon_8x8,
            NULL};

/* DIFF **********************************************************************/
int oapv_dc_removed_had8x8_neon(pel* org, int s_org)
{
    /* first pass is register-wise on 128-bit s16 row vectors, so its values
       reach 8 * 4095 and only just fit in s16; the input is therefore limited
       to 12-bit samples. after a transpose the second pass runs register-wise
       on 128-bit s32 vectors, since its values can reach 64 * 4095 and do not
       fit in s16 */
    int16x8_t r0, r1, r2, r3, r4, r5, r6, r7, src, t0, t1, t4, t5 ,t6 ,t7;

// Vert-pass
    r0 = vld1q_s16(org);

    src = vld1q_s16(org + s_org);
    r1 = vsubq_s16(r0, src);
    r0 = vaddq_s16(r0, src);

    t0 = vld1q_s16(org + 2 * s_org);

    src = vld1q_s16(org + 3 * s_org);
    t1 = vsubq_s16(t0, src);
    t0 = vaddq_s16(t0, src);

    r3 = vsubq_s16(r1, t1);
    r2 = vsubq_s16(r0, t0);
    r1 = vaddq_s16(r1, t1);
    r0 = vaddq_s16(r0, t0);

    t4 = vld1q_s16(org + 4 * s_org);

    src = vld1q_s16(org + 5 * s_org);
    t5 = vsubq_s16(t4, src);
    t4 = vaddq_s16(t4, src);

    t0 = vld1q_s16(org + 6 * s_org);

    src = vld1q_s16(org + 7 * s_org);
    t1 = vsubq_s16(t0, src);
    t0 = vaddq_s16(t0, src);

    t7 = vsubq_s16(t5, t1);
    t6 = vsubq_s16(t4, t0);
    t5 = vaddq_s16(t5, t1);
    t4 = vaddq_s16(t4, t0);

    r7 = vsubq_s16(r3, t7);
    r6 = vsubq_s16(r2, t6);
    r5 = vsubq_s16(r1, t5);
    r4 = vsubq_s16(r0, t4);
    r3 = vaddq_s16(r3, t7);
    r2 = vaddq_s16(r2, t6);
    r1 = vaddq_s16(r1, t5);
    r0 = vaddq_s16(r0, t4);

// Transpose and Horz-pass
    int16x8x2_t tmp0_8x16bx2, tmp1_8x16bx2;
    int32x4x2_t tmp_4x32bx2;
    int32x4_t h0, h1, h2, h3, q0, q1, q2, q3, q4, q5, q6, q7;

    tmp0_8x16bx2 = vtrnq_s16(r0, r1);
    tmp1_8x16bx2 = vtrnq_s16(r2, r3);

    tmp_4x32bx2 = vtrnq_s32(vreinterpretq_s32_s16(tmp0_8x16bx2.val[0]), vreinterpretq_s32_s16(tmp1_8x16bx2.val[0]));

    h0 = vaddl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h1 = vsubl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    h2 = vaddl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h3 = vsubl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    q0 = vaddq_s32(h0, h2);
    q1 = vsubq_s32(h0, h2);
    q2 = vaddq_s32(h1, h3);
    q3 = vsubq_s32(h1, h3);

    tmp_4x32bx2 = vtrnq_s32(vreinterpretq_s32_s16(tmp0_8x16bx2.val[1]), vreinterpretq_s32_s16(tmp1_8x16bx2.val[1]));

    h0 = vaddl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h1 = vsubl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    h2 = vaddl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h3 = vsubl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    q4 = vaddq_s32(h0, h2);
    q5 = vsubq_s32(h0, h2);
    q6 = vaddq_s32(h1, h3);
    q7 = vsubq_s32(h1, h3);

    int32x4_t satv = vabsq_s32(vsetq_lane_s32(0, vaddq_s32(q0, q4), 0));
    satv = vabaq_s32(satv, q0, q4);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q1, q5)));
    satv = vabaq_s32(satv, q1, q5);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q2, q6)));
    satv = vabaq_s32(satv, q2, q6);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q3, q7)));
    satv = vabaq_s32(satv, q3, q7);

    tmp0_8x16bx2 = vtrnq_s16(r4, r5);
    tmp1_8x16bx2 = vtrnq_s16(r6, r7);

    tmp_4x32bx2 = vtrnq_s32(vreinterpretq_s32_s16(tmp0_8x16bx2.val[0]), vreinterpretq_s32_s16(tmp1_8x16bx2.val[0]));

    h0 = vaddl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h1 = vsubl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    h2 = vaddl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h3 = vsubl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    q0 = vaddq_s32(h0, h2);
    q1 = vsubq_s32(h0, h2);
    q2 = vaddq_s32(h1, h3);
    q3 = vsubq_s32(h1, h3);

    tmp_4x32bx2 = vtrnq_s32(vreinterpretq_s32_s16(tmp0_8x16bx2.val[1]), vreinterpretq_s32_s16(tmp1_8x16bx2.val[1]));

    h0 = vaddl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h1 = vsubl_s16(vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_high_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    h2 = vaddl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));
    h3 = vsubl_s16(vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[0])), vget_low_s16(vreinterpretq_s16_s32(tmp_4x32bx2.val[1])));

    q4 = vaddq_s32(h0, h2);
    q5 = vsubq_s32(h0, h2);
    q6 = vaddq_s32(h1, h3);
    q7 = vsubq_s32(h1, h3);

    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q0, q4)));
    satv = vabaq_s32(satv, q0, q4);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q1, q5)));
    satv = vabaq_s32(satv, q1, q5);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q2, q6)));
    satv = vabaq_s32(satv, q2, q6);
    satv = vaddq_s32(satv, vabsq_s32(vaddq_s32(q3, q7)));
    satv = vabaq_s32(satv, q3, q7);

    int satd = vaddvq_s32(satv);
    return (satd + 2) >> 2;
}
#endif /* ARM_NEON */
