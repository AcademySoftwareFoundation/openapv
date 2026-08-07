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

/* SAD for 16bit **************************************************************/
/* DIFF **********************************************************************/
/* SSD ***********************************************************************/
s64 oapv_ssd_16b(int w, int h, void *src1, void *src2, int s_src1, int s_src2)
{
    s16 *s1;
    s16 *s2;
    int  i, j, diff;
    s64  ssd;

    s1 = (s16 *)src1;
    s2 = (s16 *)src2;

    ssd = 0;

    for(i = 0; i < h; i++) {
        for(j = 0; j < w; j++) {
            diff = s1[j] - s2[j];
            ssd += (diff * diff);
        }
        s1 += s_src1;
        s2 += s_src2;
    }
    return ssd;
}

const oapv_fn_ssd_t oapv_tbl_fn_ssd_16b[2] = {
    oapv_ssd_16b,
    NULL
};

int oapv_dc_removed_had8x8(pel *org, int s_org)
{
    int  k, i, j, jj;
    int  satd = 0;
    int  sub[64], interm1[8][8], interm2[8][8], interm3[8][8];
    pel *orgn = org;

    for(k = 0; k < 64; k += 8) {
        sub[k + 0] = orgn[0];
        sub[k + 1] = orgn[1];
        sub[k + 2] = orgn[2];
        sub[k + 3] = orgn[3];
        sub[k + 4] = orgn[4];
        sub[k + 5] = orgn[5];
        sub[k + 6] = orgn[6];
        sub[k + 7] = orgn[7];
        orgn += s_org;
    }

    /* horizontal */
    for(j = 0; j < 8; j++) {
        jj = j << 3;
        interm2[j][0] = sub[jj] + sub[jj + 4];
        interm2[j][1] = sub[jj + 1] + sub[jj + 5];
        interm2[j][2] = sub[jj + 2] + sub[jj + 6];
        interm2[j][3] = sub[jj + 3] + sub[jj + 7];
        interm2[j][4] = sub[jj] - sub[jj + 4];
        interm2[j][5] = sub[jj + 1] - sub[jj + 5];
        interm2[j][6] = sub[jj + 2] - sub[jj + 6];
        interm2[j][7] = sub[jj + 3] - sub[jj + 7];

        interm1[j][0] = interm2[j][0] + interm2[j][2];
        interm1[j][1] = interm2[j][1] + interm2[j][3];
        interm1[j][2] = interm2[j][0] - interm2[j][2];
        interm1[j][3] = interm2[j][1] - interm2[j][3];
        interm1[j][4] = interm2[j][4] + interm2[j][6];
        interm1[j][5] = interm2[j][5] + interm2[j][7];
        interm1[j][6] = interm2[j][4] - interm2[j][6];
        interm1[j][7] = interm2[j][5] - interm2[j][7];

        interm2[j][0] = interm1[j][0] + interm1[j][1];
        interm2[j][1] = interm1[j][0] - interm1[j][1];
        interm2[j][2] = interm1[j][2] + interm1[j][3];
        interm2[j][3] = interm1[j][2] - interm1[j][3];
        interm2[j][4] = interm1[j][4] + interm1[j][5];
        interm2[j][5] = interm1[j][4] - interm1[j][5];
        interm2[j][6] = interm1[j][6] + interm1[j][7];
        interm2[j][7] = interm1[j][6] - interm1[j][7];
    }

    /* vertical */
    for(i = 0; i < 8; i++) {
        interm3[0][i] = interm2[0][i] + interm2[4][i];
        interm3[1][i] = interm2[1][i] + interm2[5][i];
        interm3[2][i] = interm2[2][i] + interm2[6][i];
        interm3[3][i] = interm2[3][i] + interm2[7][i];
        interm3[4][i] = interm2[0][i] - interm2[4][i];
        interm3[5][i] = interm2[1][i] - interm2[5][i];
        interm3[6][i] = interm2[2][i] - interm2[6][i];
        interm3[7][i] = interm2[3][i] - interm2[7][i];

        interm1[0][i] = interm3[0][i] + interm3[2][i];
        interm1[1][i] = interm3[1][i] + interm3[3][i];
        interm1[2][i] = interm3[0][i] - interm3[2][i];
        interm1[3][i] = interm3[1][i] - interm3[3][i];
        interm1[4][i] = interm3[4][i] + interm3[6][i];
        interm1[5][i] = interm3[5][i] + interm3[7][i];
        interm1[6][i] = interm3[4][i] - interm3[6][i];
        interm1[7][i] = interm3[5][i] - interm3[7][i];

        interm2[0][i] = oapv_abs(interm1[0][i] + interm1[1][i]);
        interm2[1][i] = oapv_abs(interm1[0][i] - interm1[1][i]);
        interm2[2][i] = oapv_abs(interm1[2][i] + interm1[3][i]);
        interm2[3][i] = oapv_abs(interm1[2][i] - interm1[3][i]);
        interm2[4][i] = oapv_abs(interm1[4][i] + interm1[5][i]);
        interm2[5][i] = oapv_abs(interm1[4][i] - interm1[5][i]);
        interm2[6][i] = oapv_abs(interm1[6][i] + interm1[7][i]);
        interm2[7][i] = oapv_abs(interm1[6][i] - interm1[7][i]);
    }

    satd = 0;
    for(j = 1; j < 8; j++) {
        satd += interm2[0][j];
    }
    for(i = 1; i < 8; i++) {
        for(j = 0; j < 8; j++) {
            satd += interm2[i][j];
        }
    }

    satd = ((satd + 2) >> 2);

    return satd;
}