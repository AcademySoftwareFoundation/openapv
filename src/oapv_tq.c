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

#include "oapv_tq.h"
#include <math.h>

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

const int   oapv_quant_scale[6] = { 26214, 23302, 20560, 18396, 16384, 14769 };

static void oapv_tx_part(s16 *src, s16 *dst, int shift, int line)
{
    int j, k;
    int E[4], O[4];
    int EE[2], EO[2];
    int add = 1 << (shift - 1);

    for(j = 0; j < line; j++) {
        /* E and O*/
        for(k = 0; k < 4; k++) {
            E[k] = src[j * 8 + k] + src[j * 8 + 7 - k];
            O[k] = src[j * 8 + k] - src[j * 8 + 7 - k];
        }
        /* EE and EO */
        EE[0] = E[0] + E[3];
        EO[0] = E[0] - E[3];
        EE[1] = E[1] + E[2];
        EO[1] = E[1] - E[2];

        dst[0 * line + j] = (oapv_tbl_tm8[0][0] * EE[0] + oapv_tbl_tm8[0][1] * EE[1] + add) >> shift;
        dst[4 * line + j] = (oapv_tbl_tm8[4][0] * EE[0] + oapv_tbl_tm8[4][1] * EE[1] + add) >> shift;
        dst[2 * line + j] = (oapv_tbl_tm8[2][0] * EO[0] + oapv_tbl_tm8[2][1] * EO[1] + add) >> shift;
        dst[6 * line + j] = (oapv_tbl_tm8[6][0] * EO[0] + oapv_tbl_tm8[6][1] * EO[1] + add) >> shift;

        dst[1 * line + j] = (oapv_tbl_tm8[1][0] * O[0] + oapv_tbl_tm8[1][1] * O[1] + oapv_tbl_tm8[1][2] * O[2] + oapv_tbl_tm8[1][3] * O[3] + add) >> shift;
        dst[3 * line + j] = (oapv_tbl_tm8[3][0] * O[0] + oapv_tbl_tm8[3][1] * O[1] + oapv_tbl_tm8[3][2] * O[2] + oapv_tbl_tm8[3][3] * O[3] + add) >> shift;
        dst[5 * line + j] = (oapv_tbl_tm8[5][0] * O[0] + oapv_tbl_tm8[5][1] * O[1] + oapv_tbl_tm8[5][2] * O[2] + oapv_tbl_tm8[5][3] * O[3] + add) >> shift;
        dst[7 * line + j] = (oapv_tbl_tm8[7][0] * O[0] + oapv_tbl_tm8[7][1] * O[1] + oapv_tbl_tm8[7][2] * O[2] + oapv_tbl_tm8[7][3] * O[3] + add) >> shift;
    }
}

static void oapv_tx(s16 *src, int shift1, int shift2, int line)
{
    ALIGNED_16(s16 dst[OAPV_BLK_D]);
    oapv_tx_part(src, dst, shift1, line);
    oapv_tx_part(dst, src, shift2, line);
}

const oapv_fn_tx_t oapv_tbl_fn_tx[2] = {
    oapv_tx,
    NULL
};

static __inline int get_transform_shift(int log2_size, int type, int bit_depth)
{
    if(type == 0) {
        return ((log2_size)-1 + bit_depth - 8);
    }
    else {
        return ((log2_size) + 6);
    }
}

void oapv_trans(oapve_ctx_t *ctx, s16 *coef, int log2_w, int log2_h, int bit_depth)
{
    int shift1 = get_transform_shift(log2_w, 0, bit_depth);
    int shift2 = get_transform_shift(log2_h, 1, bit_depth);

    (ctx->fn_txb)[0](coef, shift1, shift2, 1 << log2_h);
}

void oapve_init_rdoq(oapve_core_t * core, int bit_depth, int ch_type)
{
    double err_scale;

    for(int cnt = 0; cnt < OAPV_BLK_D; cnt++) {
        int q_value = core->q_mat_enc[ch_type][cnt];
        int tr_shift = MAX_TX_DYNAMIC_RANGE - bit_depth - 3;
        err_scale = (double)pow(2.0, -tr_shift);
        err_scale = err_scale / q_value ;
        core->err_scale_tbl[ch_type][cnt] = err_scale;
    }
}

int oapve_rdoq(oapve_core_t* core, s16 *src_coef, s16 *dst_coef, int log2_cuw, int log2_cuh, int ch_type,  int bit_depth, double lambda)
{
    u8 qp = core->qp[ch_type];
    const s32 tr_shift = MAX_TX_DYNAMIC_RANGE - bit_depth - 3;
    const u32 max_num_coef = 1 << (log2_cuw + log2_cuh);
    const u8 *scan = oapv_tbl_scan;
    const int q_bits = QUANT_SHIFT + tr_shift + (qp / 6);
    int nnz = 0;
    u32 sum_all = 0;
    u32 scan_pos;
    u32 run = 0;
    s32 rice_level = 0;
    s32 rice_run = 0;
    s32 prev_run = 0;
    int k_ac = core->kparam_ac[ch_type];
    s16 tmp_coef[OAPV_BLK_D];
    s64 tmp_level_double[OAPV_BLK_D];
    double best_cost = 0;
    double uncoded_cost = 0;
    double best_dist = 0;
    double base_dist = 0;
    double best_bit_cost = 0;
    double base_bit_cost = 0;
    double prev_run_bit_cost = 0;
    double base_prev_run_bit_cost = 0;

    /* ===== quantization ===== */
    for(scan_pos = 0; scan_pos < max_num_coef; scan_pos++) {
        u32 blk_pos = scan[scan_pos];
        s32 level_double;
        u32 max_abs_level;
        s64 temp_level;
        double err;

        temp_level = ((s64)oapv_abs(src_coef[blk_pos]) * core->q_mat_enc[ch_type][blk_pos]);
        level_double = (s32)oapv_min(temp_level, ((s32)0x7FFFFFFF) - ((1 << q_bits) - 1));
        tmp_level_double[blk_pos] = level_double;
        max_abs_level = (u32)oapv_min((level_double + ((1 << q_bits) - 1)) >> q_bits, (1 << MAX_TX_DYNAMIC_RANGE) - 1);
        err = (double)level_double * core->err_scale_tbl[ch_type][blk_pos];
        base_dist += err * err;
        tmp_coef[blk_pos] = src_coef[blk_pos] >= 0 ? (s16)max_abs_level : -(s16)(max_abs_level);
        sum_all += max_abs_level;
    }

    if(sum_all == 0) {
        oapv_mset(dst_coef, 0, sizeof(s16)*max_num_coef);
        return nnz;
    }

    rice_level = core->kparam_dc[ch_type];
    rice_run = prev_run / 4;
    if(rice_run > 2) {
        rice_run = 2;
    }
    best_bit_cost = base_bit_cost = oapve_vlc_get_run_cost(63, 0, lambda) + oapve_vlc_get_level_cost(oapv_abs(core->prev_dc[ch_type]), rice_level, lambda);
    best_dist = base_dist;
    best_cost = best_dist + best_bit_cost;

    //DC
    {
        u32 blk_pos = 0;
        s32 org_level = tmp_coef[blk_pos];
        s32 best_level = 0;
        s32 tmp_level;
        s32 max_level = org_level > 0 ? org_level : org_level + 1;
        s32 min_level = org_level > 0 ? org_level - 1 : org_level;
        double err1;

        err1 = (double)tmp_level_double[blk_pos] * core->err_scale_tbl[ch_type][blk_pos];
        uncoded_cost = err1 * err1;

        rice_level = core->kparam_dc[ch_type];

        for(tmp_level = max_level; tmp_level >= min_level; tmp_level--) {
            if(tmp_level == 0) {
                continue;
            }
            double delta = (double)(tmp_level_double[blk_pos]  - ((s64)oapv_abs(tmp_level) << q_bits));
            double err = delta * core->err_scale_tbl[ch_type][blk_pos];
            double curr_dist = best_dist - uncoded_cost + err * err;
            double curr_run_bit_cost = oapve_vlc_get_run_cost(63, 0, lambda);
            double curr_bit_cost = oapve_vlc_get_level_cost(oapv_abs(tmp_level - core->prev_dc[ch_type]), rice_level, lambda) + curr_run_bit_cost;
            double curr_cost = curr_dist + curr_bit_cost;

            if(curr_cost < best_cost) {
                best_level = tmp_level;
                base_dist = curr_dist;
                base_bit_cost = curr_bit_cost;
                best_cost = curr_cost;
                base_prev_run_bit_cost = curr_run_bit_cost;
            }
        }
        dst_coef[blk_pos] = best_level;
        best_dist = base_dist;
        best_bit_cost = base_bit_cost;
        prev_run_bit_cost = base_prev_run_bit_cost;
    }

    // RUN & AC
    for(scan_pos = 1; scan_pos < max_num_coef; scan_pos++) {
        u32 blk_pos = scan[scan_pos];
        s32 org_level = tmp_coef[blk_pos];
        s32 best_level = 0;
        s32 tmp_level;
        s32 max_level = org_level > 0 ? org_level : org_level + 1;
        s32 min_level = org_level > 0 ? org_level - 1 : org_level;
        double err1;

        err1 = (double)tmp_level_double[blk_pos] * core->err_scale_tbl[ch_type][blk_pos];
        uncoded_cost = err1 * err1;

        rice_run = prev_run / 4;
        if(rice_run > 2) {
            rice_run = 2;
        }
        rice_level = k_ac;

        for(tmp_level = max_level; tmp_level >= min_level; tmp_level--) {
            if(tmp_level == 0) {
                continue;
            }
            double delta = (double)(tmp_level_double[blk_pos] - ((s64)oapv_abs(tmp_level) << q_bits));
            double err = delta * core->err_scale_tbl[ch_type][blk_pos];
            double curr_dist = best_dist - uncoded_cost + err * err;
            int rice_run_last = run / 4;
            if(rice_run_last > 2) {
                rice_run_last = 2;
            }
            double curr_run_bit_cost = blk_pos == 63 ? 0 : oapve_vlc_get_run_cost(63 - scan_pos, rice_run_last, lambda);
            double curr_bit_cost = best_bit_cost - prev_run_bit_cost + oapve_vlc_get_run_cost(run, rice_run, lambda) + oapve_vlc_get_level_cost(abs(tmp_level) - 1, rice_level, lambda) + curr_run_bit_cost;
            double curr_cost = curr_dist + curr_bit_cost;

            if(curr_cost < best_cost) {
                best_level = tmp_level;
                base_dist = curr_dist;
                base_bit_cost = curr_bit_cost;
                best_cost = curr_cost;
                base_prev_run_bit_cost = curr_run_bit_cost;
            }
        }
        dst_coef[blk_pos] = best_level;
        best_dist = base_dist;
        best_bit_cost = base_bit_cost;
        prev_run_bit_cost = base_prev_run_bit_cost;

        if(dst_coef[blk_pos]) {
            prev_run = run;
            k_ac = KPARAM_AC(oapv_abs(dst_coef[blk_pos]));
            run = 0;
            nnz++;
        }
        else {
            run++;
        }
    }
    return nnz;
}

static int oapv_quant(s16 *coef, u8 qp, int q_matrix[OAPV_BLK_D], int log2_w, int log2_h, int bit_depth, int deadzone_offset)
{
    // coef is the output of the transform, the bit range is 16
    // q_matrix has the value of q_scale * 16 / q_matrix, the bit range is 19
    // (precision of q_scale is 15, and the range of q_mtrix is 1~255)
    // lev is the product of abs(coef) and q_matrix, the bit range is 35

    s64 lev;
    s32 offset;
    int sign;
    int i;
    int shift;
    int tr_shift;
    int log2_size = (log2_w + log2_h) >> 1;

    tr_shift = MAX_TX_DYNAMIC_RANGE - bit_depth - log2_size;
    shift = QUANT_SHIFT + tr_shift + (qp / 6);
    offset = deadzone_offset << (shift - 9);
    int pixels = (1 << (log2_w + log2_h));

    for(i = 0; i < pixels; i++) {
        sign = oapv_get_sign(coef[i]);
        lev = (s64)oapv_abs(coef[i]) * (q_matrix[i]);
        lev = (lev + offset) >> shift;
        lev = oapv_set_sign(lev, sign);
        coef[i] = (s16)(oapv_clip3(-32768, 32767, lev));
    }
    return OAPV_OK;
}

const oapv_fn_quant_t oapv_tbl_fn_quant[2] = {
    oapv_quant,
    NULL
};

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

void oapv_itx_get_wo_sft(s16 *src, s16 *dst, s32 *dst32, int shift, int line)
{
    int j, k;
    s32 E[4], O[4];
    s32 EE[2], EO[2];
    int add = 1 << (shift - 1);

    for(j = 0; j < line; j++) {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
        for(k = 0; k < 4; k++) {
            O[k] = oapv_tbl_tm8[1][k] * src[1 * line + j] + oapv_tbl_tm8[3][k] * src[3 * line + j] + oapv_tbl_tm8[5][k] * src[5 * line + j] + oapv_tbl_tm8[7][k] * src[7 * line + j];
        }

        EO[0] = oapv_tbl_tm8[2][0] * src[2 * line + j] + oapv_tbl_tm8[6][0] * src[6 * line + j];
        EO[1] = oapv_tbl_tm8[2][1] * src[2 * line + j] + oapv_tbl_tm8[6][1] * src[6 * line + j];
        EE[0] = oapv_tbl_tm8[0][0] * src[0 * line + j] + oapv_tbl_tm8[4][0] * src[4 * line + j];
        EE[1] = oapv_tbl_tm8[0][1] * src[0 * line + j] + oapv_tbl_tm8[4][1] * src[4 * line + j];

        /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];

        for(k = 0; k < 4; k++) {
            dst32[j * 8 + k] = E[k] + O[k];
            dst32[j * 8 + k + 4] = E[3 - k] - O[3 - k];

            dst[j * 8 + k] = ((dst32[j * 8 + k] + add) >> shift);
            dst[j * 8 + k + 4] = ((dst32[j * 8 + k + 4] + add) >> shift);
        }
    }
}

static void oapv_itx_part(s16 *src, s16 *dst, int shift, int line)
{
    int j, k;
    int E[4], O[4];
    int EE[2], EO[2];
    int add = 1 << (shift - 1);

    for(j = 0; j < line; j++) {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
        for(k = 0; k < 4; k++) {
            O[k] = oapv_tbl_tm8[1][k] * src[1 * line + j] + oapv_tbl_tm8[3][k] * src[3 * line + j] + oapv_tbl_tm8[5][k] * src[5 * line + j] + oapv_tbl_tm8[7][k] * src[7 * line + j];
        }

        EO[0] = oapv_tbl_tm8[2][0] * src[2 * line + j] + oapv_tbl_tm8[6][0] * src[6 * line + j];
        EO[1] = oapv_tbl_tm8[2][1] * src[2 * line + j] + oapv_tbl_tm8[6][1] * src[6 * line + j];
        EE[0] = oapv_tbl_tm8[0][0] * src[0 * line + j] + oapv_tbl_tm8[4][0] * src[4 * line + j];
        EE[1] = oapv_tbl_tm8[0][1] * src[0 * line + j] + oapv_tbl_tm8[4][1] * src[4 * line + j];

        /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];

        for(k = 0; k < 4; k++) {
            dst[j * 8 + k] = ((E[k] + O[k] + add) >> shift);
            dst[j * 8 + k + 4] = ((E[3 - k] - O[3 - k] + add) >> shift);
        }
    }
}

const oapv_fn_itx_part_t oapv_tbl_fn_itx_part[2] = {
    oapv_itx_part,
    NULL
};

static void oapv_itx(s16 *src, int shift1, int shift2, int line)
{
    ALIGNED_16(s16 dst[OAPV_BLK_D]);
    oapv_itx_part(src, dst, shift1, line);
    oapv_itx_part(dst, src, shift2, line);
}

const oapv_fn_itx_t oapv_tbl_fn_itx[2] = {
    oapv_itx,
    NULL
};

static void oapv_dquant(s16 *coef, s16 q_matrix[OAPV_BLK_D], int log2_w, int log2_h, s8 shift)
{
    int i;
    int lev;
    int pixels = (1 << (log2_w + log2_h));

    if(shift > 0) {
        s32 offset = (1 << (shift - 1));
        for(i = 0; i < pixels; i++) {
            lev = (coef[i] * q_matrix[i] + offset) >> shift;
            coef[i] = (s16)oapv_clip3(-32768, 32767, lev);
        }
    }
    else {
        int left_shift = -shift;
        for(i = 0; i < pixels; i++) {
            lev = (coef[i] * q_matrix[i]) << left_shift;
            coef[i] = (s16)oapv_clip3(-32768, 32767, lev);
        }
    }
}

const oapv_fn_dquant_t oapv_tbl_fn_dquant[2] = {
    oapv_dquant,
    NULL
};

void oapv_adjust_itrans(int *src, int *dst, int itrans_diff_idx, int diff_step, int shift)
{
    int offset = 1 << (shift - 1);
    short* itrans_diff = oapv_itrans_diff[itrans_diff_idx];
    for(int k = 0; k < 4; k++) {
        dst[0]  = src[0] +  ((itrans_diff[0]  * diff_step + offset) >> shift);
        dst[1]  = src[1] +  ((itrans_diff[1]  * diff_step + offset) >> shift);
        dst[2]  = src[2] +  ((itrans_diff[2]  * diff_step + offset) >> shift);
        dst[3]  = src[3] +  ((itrans_diff[3]  * diff_step + offset) >> shift);
        dst[4]  = src[4] +  ((itrans_diff[8]  * diff_step + offset) >> shift);
        dst[5]  = src[5] +  ((itrans_diff[9]  * diff_step + offset) >> shift);
        dst[6]  = src[6] +  ((itrans_diff[10] * diff_step + offset) >> shift);
        dst[7]  = src[7] +  ((itrans_diff[11] * diff_step + offset) >> shift);
        dst[8]  = src[8] +  ((itrans_diff[4]  * diff_step + offset) >> shift);
        dst[9]  = src[9] +  ((itrans_diff[5]  * diff_step + offset) >> shift);
        dst[10] = src[10] + ((itrans_diff[6]  * diff_step + offset) >> shift);
        dst[11] = src[11] + ((itrans_diff[7]  * diff_step + offset) >> shift);
        dst[12] = src[12] + ((itrans_diff[12] * diff_step + offset) >> shift);
        dst[13] = src[13] + ((itrans_diff[13] * diff_step + offset) >> shift);
        dst[14] = src[14] + ((itrans_diff[14] * diff_step + offset) >> shift);
        dst[15] = src[15] + ((itrans_diff[15] * diff_step + offset) >> shift);
        itrans_diff += 16;
        dst += 16;
        src += 16;
    }
}

const oapv_fn_itx_adj_t oapv_tbl_fn_itx_adj[2] = {
    oapv_adjust_itrans,
    NULL,
};
