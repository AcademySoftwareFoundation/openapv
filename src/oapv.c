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

static void imgb_pad(oapv_imgb_t *imgb, int aw, int ah, int comp_sft[N_C][2])
{
    int imgb_w = imgb->w[0];
    int imgb_h = imgb->h[0];

    if(aw == imgb_w && ah == imgb_h) { // no need to pad
        return;
    }

    if(aw != imgb_w) {
        for(int c = 0; c < imgb->np; c++) {
            int  sw = imgb_w >> comp_sft[c][0];
            int  ew = aw >> comp_sft[c][0];
            int  th = ah >> comp_sft[c][1];
            pel *dst = (pel *)imgb->a[c];
            pel  src;

            for(int h = 0; h < th; h++) {
                src = dst[sw - 1];
                for(int w = sw; w < ew; w++) {
                    dst[w] = src;
                }
                dst += (imgb->s[c] >> 1);
            }
        }
    }

    if(ah != imgb_h) {
        for(int c = 0; c < imgb->np; c++) {
            int  sh = imgb_h >> comp_sft[c][1];
            int  eh = ah >> comp_sft[c][1];
            int  tw = aw >> comp_sft[c][0];
            pel *dst = ((pel *)imgb->a[c]) + sh * (imgb->s[c] >> 1);
            pel *src = dst - (imgb->s[c] >> 1);

            for(int h = sh; h < eh; h++) {
                oapv_mcpy(dst, src, sizeof(pel) * tw);
                dst += (imgb->s[c] >> 1);
            }
        }
    }
}

static void imgb_pad_p210(oapv_imgb_t *imgb, int aw, int ah, int comp_sft[N_C][2])
{
    int imgb_w = imgb->w[0];
    int imgb_h = imgb->h[0];

    if(aw == imgb_w && ah == imgb_h) { // no need to pad
        return;
    }

    if(aw != imgb_w) {
        for(int c = 0; c < imgb->np; c++) {
            int  shift_w = 0;
            int  shift_h = 0;

            int  sw = imgb_w >> shift_w;
            int  ew = aw >> shift_w;
            int  th = ah >> shift_h;
            pel *dst = (pel *)imgb->a[c];
            pel  src;

            for(int h = 0; h < th; h++) {
                src = dst[sw - 1];
                for(int w = sw; w < ew; w++) {
                    dst[w] = src;
                }
                dst += (imgb->s[c] >> 1);
            }
        }
    }

    if(ah != imgb_h) {
        for(int c = 0; c < imgb->np; c++) {
            int  shift_w = 0;
            int  shift_h = 0;

            int  sh = imgb_h >> shift_h;
            int  eh = ah >> shift_h;
            int  tw = aw >> shift_w;
            pel *dst = ((pel *)imgb->a[c]) + sh * (imgb->s[c] >> 1);
            pel *src = dst - (imgb->s[c] >> 1);

            for(int h = sh; h < eh; h++) {
                oapv_mcpy(dst, src, sizeof(pel) * tw);
                dst += (imgb->s[c] >> 1);
            }
        }
    }
}

static void fi_to_finfo(oapv_fi_t *fi, int pbu_type, int group_id, oapv_frm_info_t *finfo)
{
    finfo->w = (int)fi->frame_width; // casting to 'int' would be fine here
    finfo->h = (int)fi->frame_height; // casting to 'int' would be fine here
    if(fi->profile_idc == OAPV_PROFILE_444_16C12 || fi->profile_idc == OAPV_PROFILE_4444_16C12) {
        finfo->cs = OAPV_CS_SET(chroma_format_idc_to_color_format(fi->chroma_format_idc), 16, 0);
    }
    else {
        finfo->cs = OAPV_CS_SET(chroma_format_idc_to_color_format(fi->chroma_format_idc), fi->bit_depth, 0);
    }
    finfo->pbu_type = pbu_type;
    finfo->group_id = group_id;
    finfo->profile_idc = fi->profile_idc;
    finfo->level_idc = fi->level_idc;
    finfo->band_idc = fi->band_idc;
    finfo->chroma_format_idc = fi->chroma_format_idc;
    finfo->bit_depth = fi->bit_depth;
    finfo->capture_time_distance = fi->capture_time_distance;
    finfo->use_companding = fi->use_companding;
}

static void fh_to_finfo(oapv_fh_t *fh, int pbu_type, int group_id, oapv_frm_info_t *finfo)
{
    fi_to_finfo(&fh->fi, pbu_type, group_id, finfo);
    finfo->use_q_matrix = fh->use_q_matrix;
    for(int c = 0; c < OAPV_MAX_CC; c++) {
        int mod = (1 << OAPV_LOG2_BLK) - 1;
        for(int i = 0; i < OAPV_BLK_D; i++) {
            finfo->q_matrix[c][i] = fh->q_matrix[c][i >> OAPV_LOG2_BLK][i & mod];
        }
    }
    finfo->color_description_present_flag = fh->color_description_present_flag;
    finfo->color_primaries = fh->color_primaries;
    finfo->transfer_characteristics = fh->transfer_characteristics;
    finfo->matrix_coefficients = fh->matrix_coefficients;
    finfo->full_range_flag = fh->full_range_flag;
}

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

static oapve_ctx_t *enc_id_to_ctx(oapve_t id)
{
    oapve_ctx_t *ctx;
    oapv_assert_rv(id, NULL);
    ctx = (oapve_ctx_t *)id;
    oapv_assert_rv((ctx)->magic == OAPVE_MAGIC_CODE, NULL);
    return ctx;
}

static oapve_ctx_t *enc_ctx_alloc(void)
{
    oapve_ctx_t *ctx;
    ctx = (oapve_ctx_t *)oapv_malloc_fast(sizeof(oapve_ctx_t));
    oapv_assert_rv(ctx, NULL);
    oapv_mset_x64a(ctx, 0, sizeof(oapve_ctx_t));
    return ctx;
}

static void enc_ctx_free(oapve_ctx_t *ctx)
{
    oapv_mfree_fast(ctx);
}

static oapve_core_t *enc_core_alloc()
{
    oapve_core_t *core;
    core = (oapve_core_t *)oapv_malloc_fast(sizeof(oapve_core_t));

    oapv_assert_rv(core, NULL);
    oapv_mset_x64a(core, 0, sizeof(oapve_core_t));

    return core;
}

static void enc_core_free(oapve_core_t *core)
{
    oapv_mfree_fast(core);
}

static int enc_core_init(oapve_core_t *core, oapve_ctx_t *ctx, int tile_idx, int thread_idx)
{
    core->tile_idx = tile_idx;
    core->ctx = ctx;
    return OAPV_OK;
}

static void enc_minus_mid_val(s16 *coef, int w_blk, int h_blk, int bit_depth)
{
    int mid_val = 1 << (bit_depth - 1);
    for(int i = 0; i < h_blk * w_blk; i++) {
        coef[i] -= mid_val;
    }
}

static int enc_set_tile_info(oapve_tile_t *ti, int w_pel, int h_pel, int tile_w,
                             int tile_h, int *num_tile_cols, int *num_tile_rows, int *num_tiles)
{
    (*num_tile_cols) = oapv_div_round_up(w_pel, tile_w);
    (*num_tile_rows) = oapv_div_round_up(h_pel, tile_h);
    (*num_tiles) = (*num_tile_cols) * (*num_tile_rows);

    for(int i = 0; i < (*num_tiles); i++) {
        int tx = (i % (*num_tile_cols)) * tile_w;
        int ty = (i / (*num_tile_cols)) * tile_h;
        ti[i].x = tx;
        ti[i].y = ty;
        ti[i].w = tx + tile_w > w_pel ? w_pel - tx : tile_w;
        ti[i].h = ty + tile_h > h_pel ? h_pel - ty : tile_h;
    }
    return OAPV_OK;
}

static double enc_block(oapve_ctx_t *ctx, oapve_core_t *core, int log2_w, int log2_h, int c)
{
    int bit_depth = ctx->bit_depth;

    oapv_trans(ctx, core->coef, log2_w, log2_h, bit_depth);
    ctx->fn_quant[0](core->coef, core->qp[c], core->q_mat_enc[c], log2_w, log2_h, bit_depth, c ? 128 : 212);

    core->dc_diff = core->coef[0] - core->prev_dc[c];
    core->prev_dc[c] = core->coef[0];

    if(ctx->imgb_r) {
        oapv_mcpy(core->coef_rec, core->coef, sizeof(s16) * OAPV_BLK_D);
        ctx->fn_dquant[0](core->coef_rec, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
        ctx->fn_itx[0](core->coef_rec, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    }

    return 0;
}

static double enc_block_rdo_medium(oapve_ctx_t *ctx, oapve_core_t *core, int log2_w, int log2_h, int c)
{
    int bit_depth = ctx->bit_depth;
    int qp = core->qp[c];
    double lambda = 0.57 * pow(2.0, (qp - 12.0) / 3.0);

    oapv_trans(ctx, core->coef, log2_w, log2_h, bit_depth);
    oapve_rdoq(core,core->coef, core->coef, log2_w, log2_h, c, bit_depth, lambda);

    core->dc_diff = core->coef[0] - core->prev_dc[c];
    core->prev_dc[c] = core->coef[0];

    if(ctx->imgb_r) {
        oapv_mcpy(core->coef_rec, core->coef, sizeof(s16) * OAPV_BLK_D);
        ctx->fn_dquant[0](core->coef_rec, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
        ctx->fn_itx[0](core->coef_rec, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    }

    return 0;
}

static double enc_block_rdo_slow(oapve_ctx_t *ctx, oapve_core_t *core, int log2_w, int log2_h, int c)
{
    ALIGNED_16(s16 org[OAPV_BLK_D]);
    ALIGNED_16(s16 recon[OAPV_BLK_D]);
    ALIGNED_16(s16 coeff[OAPV_BLK_D]);
    ALIGNED_16(s16 tmp_buf[OAPV_BLK_D]);

    ALIGNED_32(int rec_ups[OAPV_BLK_D]);
    ALIGNED_32(int rec_tmp[OAPV_BLK_D]);

    int        blk_w = 1 << log2_w;
    int        blk_h = 1 << log2_h;
    int        bit_depth = ctx->bit_depth;
    int        qp = core->qp[c];

    s16       *best_coeff = core->coef;
    s16       *best_recon = core->coef_rec;

    int        best_cost = INT_MAX;
    int        zero_dist = 0;
    const u8 *scanp = oapv_tbl_scan;
    const int  map_idx_diff[15] = { 0, -1, 1, -2, 2, -3, 3, -4, 4, -5, 5, -6, 6, -7, 7 };
    double     lambda = 0.57 * pow(2.0, (qp - 12.0) / 3.0);

    oapv_mcpy(org, core->coef, sizeof(s16) * OAPV_BLK_D);
    oapv_trans(ctx, core->coef, log2_w, log2_h, bit_depth);
    oapv_mcpy(coeff, core->coef, sizeof(s16) * OAPV_BLK_D);
    oapve_rdoq(core, coeff, coeff, log2_w, log2_h, c, bit_depth, lambda);

    {
        oapv_mcpy(recon, coeff, sizeof(s16) * OAPV_BLK_D);
        ctx->fn_dquant[0](recon, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
        ctx->fn_itx_part[0](recon, tmp_buf, ITX_SHIFT1, 1 << log2_w);
        oapv_itx_get_wo_sft(tmp_buf, recon, rec_ups, ITX_SHIFT2(bit_depth), 1 << log2_h);

        int cost = (int)ctx->fn_ssd[0](blk_w, blk_h, org, recon, blk_w, blk_w);
        oapv_mcpy(best_coeff, coeff, sizeof(s16) * OAPV_BLK_D);
        if(ctx->imgb_r) {
            oapv_mcpy(best_recon, recon, sizeof(s16) * OAPV_BLK_D);
        }
        if(cost == 0) {
            zero_dist = 1;
        }
        best_cost = cost;
    }

    for(int itr = 0; itr < (c == 0 ? 2 : 1) && !zero_dist; itr++) {
        for(int j = 0; j < OAPV_BLK_D && !zero_dist; j++) {
            int best_idx = 0;
            s16 org_coef = coeff[scanp[j]];
            int adj_rng = (c == 0 ? 13 : 5);
            if(org_coef == 0) {
                if(c == 0 && scanp[j] < 3) {
                    adj_rng = 3;
                }
                else {
                    continue;
                }
            }
            int q_step = 0;
            if(core->dq_shift[c] > 0) {
                q_step = (core->q_mat_dec[c][scanp[j]] + (1 << (core->dq_shift[c] - 1))) >> core->dq_shift[c];
            }
            else {
                q_step = (core->q_mat_dec[c][scanp[j]]) << (-core->dq_shift[c]);
            }

            for(int i = 1; i < adj_rng && !zero_dist; i++) {
                if(i > 2) {
                    if(best_idx == 0) {
                        continue;
                    }
                    else if(best_idx % 2 == 1 && i % 2 == 0) {
                        continue;
                    }
                    else if(best_idx % 2 == 0 && i % 2 == 1) {
                        continue;
                    }
                }

                s16 test_coef = (s16)oapv_clip3(-32768, 32767, org_coef + map_idx_diff[i]);
                coeff[scanp[j]] = test_coef;
                int step_diff = q_step * map_idx_diff[i];
                ctx->fn_itx_adj[0](rec_ups, rec_tmp, j, step_diff, 9);
                for(int k = 0; k < 64; k++) {
                    recon[k] = (rec_tmp[k] + 512) >> 10;
                }

                int cost = (int)ctx->fn_ssd[0](blk_w, blk_h, org, recon, blk_w, blk_w);
                if(cost < best_cost) {
                    oapv_mcpy(rec_ups, rec_tmp, sizeof(int) * OAPV_BLK_D);
                    best_cost = cost;
                    best_coeff[scanp[j]] = test_coef;
                    best_idx = i;
                    if(cost == 0) {
                        zero_dist = 1;
                    }
                }
                else {
                    coeff[scanp[j]] = org_coef + map_idx_diff[best_idx];
                }
            }
        }
    }

    if(ctx->imgb_r) {
        oapv_mcpy(best_recon, best_coeff, sizeof(s16) * OAPV_BLK_D);
        ctx->fn_dquant[0](best_recon, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
        ctx->fn_itx[0](best_recon, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    }

    core->dc_diff = best_coeff[0] - core->prev_dc[c];
    core->prev_dc[c] = best_coeff[0];

    return best_cost;
}

#define OAPV_FULL_RDO_MAX_CAND 6

typedef struct oapve_coef_info oapve_coef_info_t;
struct oapve_coef_info
{
    int coef_pos;
    int coef_org;
    int coef_test;
    double cost;
};

void add_coef_list(oapve_coef_info_t* coef_list, oapve_coef_info_t coef_cur, int* list_cnt)
{
    if((*list_cnt) == OAPV_FULL_RDO_MAX_CAND && coef_cur.cost > coef_list[OAPV_FULL_RDO_MAX_CAND - 1].cost) {
        return;
    }

    int curr_pos = (*list_cnt) == OAPV_FULL_RDO_MAX_CAND ? OAPV_FULL_RDO_MAX_CAND - 1 : (*list_cnt);

    coef_list[curr_pos] = coef_cur;

    while(curr_pos > 0) {
        if(coef_list[curr_pos].cost < coef_list[curr_pos - 1].cost) {
            oapve_coef_info_t tmp = coef_list[curr_pos];
            coef_list[curr_pos] = coef_list[curr_pos - 1];
            coef_list[curr_pos - 1] = tmp;
            curr_pos--;
        }
        else {
            break;
        }
    }

    if(*list_cnt < OAPV_FULL_RDO_MAX_CAND) {
        (*list_cnt)++;
    }
}

static double enc_block_rdo_placebo(oapve_ctx_t* ctx, oapve_core_t* core, int log2_w, int log2_h, int c)
{
    ALIGNED_16(s16 org[OAPV_BLK_D]);
    ALIGNED_16(s16 recon[OAPV_BLK_D]);
    ALIGNED_16(s16 coeff[OAPV_BLK_D]);

    int        blk_w = 1 << log2_w;
    int        blk_h = 1 << log2_h;
    int        bit_depth = ctx->bit_depth;
    int        qp = core->qp[c];

    s16* best_coeff = core->coef;
    s16* best_recon = core->coef_rec;

    double     best_cost = INT_MAX;
    const u8* scanp = oapv_tbl_scan;

    oapv_mcpy(org, core->coef, sizeof(s16) * OAPV_BLK_D);
    oapv_trans(ctx, core->coef, log2_w, log2_h, bit_depth);
    ctx->fn_quant[0](core->coef, qp, core->q_mat_enc[c], log2_w, log2_h, bit_depth, c ? 128 : 128);

    oapv_mcpy(recon, core->coef, sizeof(s16) * OAPV_BLK_D);
    ctx->fn_dquant[0](recon, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
    ctx->fn_itx[0](recon, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    best_cost = (int)ctx->fn_ssd[0](blk_w, blk_h, org, recon, blk_w, blk_w);

    double lambda = (0.57 * pow(2.0, (core->qp[c] - 12) / 3.0));
    int rate_org = oapve_vlc_get_coef_rate(core, core->coef, c);
    best_cost += lambda * rate_org;

    for(int itr = 0; itr < 3; itr++) {
        int list_cnt = 0;
        oapve_coef_info_t coef_list[OAPV_FULL_RDO_MAX_CAND] = { 0 };

        for(int j = 0; j < OAPV_BLK_D; j++) {
            s16 org_coef = best_coeff[scanp[j]];
            int adj_rng = org_coef == 0 ? 3 : 2;

            oapve_coef_info_t coef_cur;
            coef_cur.cost = best_cost;
            for(int i = 1; i < adj_rng; i++) {
                s16 test_diff = org_coef == 0 ? (i == 1 ? 1 : -1) : (org_coef > 0 ? i : -i);
                s16 test_coef = (s16)oapv_clip3(-32768, 32767, org_coef + test_diff);

                oapv_mcpy(coeff, best_coeff, sizeof(s16) * OAPV_BLK_D);
                coeff[scanp[j]] = test_coef;

                int test_rate = oapve_vlc_get_coef_rate(core, coeff, c);
                ctx->fn_dquant[0](coeff, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
                ctx->fn_itx[0](coeff, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
                double cost = (int)ctx->fn_ssd[0](blk_w, blk_h, org, coeff, blk_w, blk_w);
                cost += (lambda) * (test_rate);

                if(cost < coef_cur.cost) {
                    coef_cur.cost = cost;
                    coef_cur.coef_org = org_coef;
                    coef_cur.coef_test = test_coef;
                    coef_cur.coef_pos = scanp[j];
                }
            }

            if(coef_cur.cost < best_cost) {
                add_coef_list(coef_list, coef_cur, &list_cnt);
            }
        }

        for(int j = 1; j < (1 << list_cnt) && j < (1 << OAPV_FULL_RDO_MAX_CAND); j++) {
            oapv_mcpy(coeff, best_coeff, sizeof(s16) * OAPV_BLK_D);
            for(int i = 0; i < OAPV_FULL_RDO_MAX_CAND && i < list_cnt; i++) {
                coeff[coef_list[i].coef_pos] = ((j >> i) & 1) ? coef_list[i].coef_test : coef_list[i].coef_org;
            }
            oapv_mcpy(recon, coeff, sizeof(s16) * OAPV_BLK_D);
            ctx->fn_dquant[0](recon, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
            ctx->fn_itx[0](recon, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
            double cost = (int)ctx->fn_ssd[0](blk_w, blk_h, org, recon, blk_w, blk_w);
            int test_rate = oapve_vlc_get_coef_rate(core, coeff, c);
            cost += (lambda) * (test_rate);
            if(cost < best_cost) {
                best_cost = cost;
                oapv_mcpy(best_coeff, coeff, sizeof(s16) * OAPV_BLK_D);
            }
        }
    }

    if(ctx->imgb_r) {
        oapv_mcpy(best_recon, best_coeff, sizeof(s16) * OAPV_BLK_D);
        ctx->fn_dquant[0](best_recon, core->q_mat_dec[c], log2_w, log2_h, core->dq_shift[c]);
        ctx->fn_itx[0](best_recon, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    }

    core->dc_diff = best_coeff[0] - core->prev_dc[c];
    core->prev_dc[c] = best_coeff[0];

    return best_cost;
}

static void enc_flush(oapve_ctx_t *ctx)
{
    // Release thread pool controller and created threads
    if(ctx->threads >= 1) {
        if(ctx->tpool) {
            // thread controller instance is present
            // terminate the created thread
            for(int i = 0; i < ctx->threads; i++) {
                if(ctx->thread_id[i]) {
                    // valid thread instance
                    ctx->tpool->release(&ctx->thread_id[i]);
                }
            }
            // deinitialize the tc
            oapv_tpool_deinit(ctx->tpool);
            oapv_mfree_fast(ctx->tpool);
            ctx->tpool = NULL;
        }
    }

    if(ctx->sync_obj != NULL) {
        oapv_tpool_sync_obj_delete(&ctx->sync_obj);
    }
    for(int i = 0; i < ctx->threads; i++) {
        enc_core_free(ctx->core[i]);
        ctx->core[i] = NULL;
    }

    oapv_mfree_fast(ctx->tile[0].bs_buf);
}

static int enc_ready(oapve_ctx_t *ctx)
{
    oapve_core_t *core = NULL;
    int           ret = OAPV_OK;
    oapv_assert(ctx->core[0] == NULL);

    ret = oapve_param_update(ctx);
    oapv_assert_g(ret == OAPV_OK, ERR);

    for(int i = 0; i < ctx->threads; i++) {
        core = enc_core_alloc();
        oapv_assert_gv(core != NULL, ret, OAPV_ERR_OUT_OF_MEMORY, ERR);
        ctx->core[i] = core;
    }

    // initialize the threads to NULL
    for(int i = 0; i < OAPV_MAX_THREADS; i++) {
        ctx->thread_id[i] = 0;
    }

    // get the context synchronization handle
    ctx->sync_obj = oapv_tpool_sync_obj_create();
    oapv_assert_gv(ctx->sync_obj != NULL, ret, OAPV_ERR_UNKNOWN, ERR);

    if(ctx->threads >= 1) {
        ctx->tpool = oapv_malloc(sizeof(oapv_tpool_t));
        oapv_tpool_init(ctx->tpool, ctx->threads);
        for(int i = 0; i < ctx->threads; i++) {
            ctx->thread_id[i] = ctx->tpool->create(ctx->tpool, i);
            oapv_assert_gv(ctx->thread_id[i] != NULL, ret, OAPV_ERR_UNKNOWN, ERR);
        }
    }

    for(int i = 0; i < OAPV_MAX_TILES; i++) {
        ctx->tile[i].stat = ENC_TILE_STAT_NOT_ENCODED;
    }
    ctx->tile[0].bs_buf = (u8 *)oapv_malloc(ctx->cdesc.max_bs_buf_size);
    oapv_assert_gv(ctx->tile[0].bs_buf, ret, OAPV_ERR_UNKNOWN, ERR);

    ctx->rc_param.alpha = OAPV_RC_ALPHA;
    ctx->rc_param.beta = OAPV_RC_BETA;
    ctx->au_bs_fmt = OAPV_CFG_VAL_AU_BS_FMT_RBAU; // default: enable raw bitstream format

    return OAPV_OK;
ERR:
    enc_flush(ctx);

    return ret;
}

static int enc_tile_comp(oapv_bs_t *bs, oapve_tile_t *tile, oapve_ctx_t *ctx, oapve_core_t *core, int c, int org_s, void *org, int rec_s, void *rec)
{
    int  mb_h, mb_w, y, x, i, j;
    s16 *pic = NULL, *rec_t = NULL;

    u8  *bs_cur = oapv_bsw_sink(bs);
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);

    mb_w = OAPV_MB_W >> ctx->c_sft[c][0];
    mb_h = OAPV_MB_H >> ctx->c_sft[c][1];

    int le = tile->x >> ctx->c_sft[c][0];
    int ri = (tile->w >> ctx->c_sft[c][0]) + le;
    int to = tile->y >> ctx->c_sft[c][1];
    int bo = (tile->h >> ctx->c_sft[c][1]) + to;

    for(y = to; y < bo; y += mb_h) {
        for(x = le; x < ri; x += mb_w) {
            for(j = y; j < (y + mb_h); j += OAPV_BLK_H) {
                for(i = x; i < (x + mb_w); i += OAPV_BLK_W) {
                    pic = (s16 *)((u8 *)org + j * org_s) + i;
                    ctx->fn_blk_from_pic[c](OAPV_BLK_W, OAPV_BLK_H, pic, i, org_s, core->coef, (OAPV_BLK_W << 1), ctx->bit_depth, (1 << (ctx->bit_depth - 1)));

                    ctx->fn_enc_blk(ctx, core, OAPV_LOG2_BLK_W, OAPV_LOG2_BLK_H, c);
                    oapve_vlc_dc_coef(bs, core->dc_diff, &core->kparam_dc[c]);
                    oapve_vlc_ac_coef(bs, core->coef, &core->kparam_ac[c]);
                    DUMP_COEF(core->dc_diff, core->coef + 1, OAPV_BLK_D - 1, i, j, c);

                    if(rec != NULL) {
                        rec_t = (s16 *)((u8 *)rec + j * rec_s) + i;
                        ctx->fn_blk_to_pic[c](OAPV_BLK_W, OAPV_BLK_H, core->coef_rec, (OAPV_BLK_W << 1), rec_t, i, rec_s, ctx->bit_depth);
                    }
                }
            }
        }
    }

    /* byte align */
    while(!bsw_is_align8(bs)) {
        oapv_bsw_write1(bs, 0);
    }

    int enc_bytes = (u8*)oapv_bsw_sink(bs) - bs_cur;
    oapv_assert(enc_bytes > 0);

    oapv_bsw_deinit(bs);
    return enc_bytes;
}

static int enc_tile(oapve_ctx_t *ctx, oapve_core_t *core, oapve_tile_t *tile)
{
    oapv_bs_t bs;
    oapv_bsw_init(&bs, tile->bs_buf, tile->bs_buf_max, NULL);

    int qp = 0;
    if(ctx->param->rc_type != OAPV_RC_CQP) {
        oapve_rc_get_qp(ctx, tile, ctx->qp[Y_C], &qp);
    }
    else {
        qp = ctx->qp[Y_C];
    }

    tile->tile_size = 0;
    DUMP_SAVE(0);
    oapve_vlc_tile_size(&bs, tile->tile_size);
    oapve_set_tile_header(ctx, &tile->th, core->tile_idx, qp);
    oapve_vlc_tile_header(ctx, &bs, &tile->th);

    for(int c = 0; c < ctx->num_c; c++) {
        int cnt = 0;
        core->qp[c] = tile->th.tile_qp[c];
        int qscale = oapv_quant_scale[core->qp[c] % 6];
        s32 scale_multiply_16 = (s32)(qscale << 4); // 15bit + 4bit
        for(int y = 0; y < OAPV_BLK_H; y++) {
            for(int x = 0; x < OAPV_BLK_W; x++) {
                core->q_mat_enc[c][cnt++] = scale_multiply_16 / ctx->fh.q_matrix[c][y][x];
            }
        }

        if(ctx->imgb_r || ctx->param->preset >= OAPV_PRESET_MEDIUM) {
            core->dq_shift[c] = ctx->bit_depth - 2 - (core->qp[c] / 6);

            int cnt = 0;
            u8 dq_scale = oapv_tbl_dq_scale[core->qp[c] % 6];
            for(int y = 0; y < OAPV_BLK_H; y++) {
                for(int x = 0; x < OAPV_BLK_W; x++) {
                    core->q_mat_dec[c][cnt++] = dq_scale * ctx->fh.q_matrix[c][y][x];
                }
            }
        }

        if(ctx->param->preset == OAPV_PRESET_MEDIUM || ctx->param->preset == OAPV_PRESET_SLOW) {
            oapve_init_rdoq(core, ctx->bit_depth, c);
        }
    }

    for(int c = 0; c < ctx->num_c; c++) {
        core->kparam_dc[c] = OAPV_KPARAM_DC_MAX;
        core->kparam_ac[c] = OAPV_KPARAM_AC_MIN;
        core->prev_dc[c] = 0;

        int  tc, org_s, rec_s;
        s16 *org, *rec;

        if(OAPV_CS_GET_FORMAT(ctx->imgb_i->cs) == OAPV_CF_PLANAR2) {
            tc = c > 0 ? 1 : 0;
            org = ctx->imgb_i->a[tc];
            org += (c > 1) ? 1 : 0;
            org_s = ctx->imgb_i->s[tc];

            if(ctx->imgb_r) {
                rec = ctx->imgb_r->a[tc];
                rec += (c > 1) ? 1 : 0;
                rec_s = ctx->imgb_i->s[tc];
            }
            else {
                rec = NULL;
                rec_s = 0;
            }
        }
        else {
            org = ctx->imgb_i->a[c];
            org_s = ctx->imgb_i->s[c];
            if(ctx->imgb_r) {
                rec = ctx->imgb_r->a[c];
                rec_s = ctx->imgb_i->s[c];
            }
            else {
                rec = NULL;
                rec_s = 0;
            }
        }

        tile->th.tile_data_size[c] = enc_tile_comp(&bs, tile, ctx, core, c, org_s, org, rec_s, rec);
    }

    u32 remained_bs_size = (int)((u8*)oapv_bsw_sink(&bs) - bs.beg);
    if(remained_bs_size > tile->bs_buf_max) {
        return OAPV_ERR_OUT_OF_BS_BUF;
    }
    tile->bs_size = remained_bs_size;

    oapv_bs_t bs_th;
    oapv_bsw_init(&bs_th, tile->bs_buf, tile->bs_size, NULL);
    tile->tile_size = remained_bs_size - OAPV_TILE_SIZE_LEN;

    DUMP_SAVE(1);
    DUMP_LOAD(0);
    oapve_vlc_tile_size(&bs_th, tile->tile_size);
    oapve_vlc_tile_header(ctx, &bs_th, &tile->th);
    DUMP_LOAD(1);
    oapv_bsw_deinit(&bs_th);
    return OAPV_OK;
}

static int enc_thread_tile(void *arg)
{
    oapve_core_t *core = (oapve_core_t *)arg;
    oapve_ctx_t  *ctx = core->ctx;
    oapve_tile_t *tile = ctx->tile;
    int           ret = OAPV_OK, i;

    while(1) {
        // find not encoded tile
        oapv_tpool_enter_cs(ctx->sync_obj);
        for(i = 0; i < ctx->num_tiles; i++) {
            if(tile[i].stat == ENC_TILE_STAT_NOT_ENCODED) {
                tile[i].stat = ENC_TILE_STAT_ON_ENCODING;
                core->tile_idx = i;
                break;
            }
        }
        oapv_tpool_leave_cs(ctx->sync_obj);
        if(i == ctx->num_tiles) {
            break;
        }

        ret = enc_tile(ctx, core, &tile[core->tile_idx]);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

        oapv_tpool_enter_cs(ctx->sync_obj);
        tile[core->tile_idx].stat = ENC_TILE_STAT_ENCODED;
        oapv_tpool_leave_cs(ctx->sync_obj);
    }
ERR:
    return ret;
}

static int enc_profile_spec[][5] = {
    // {profile-idc, cfi-min, cfi-max, bit-depth-min, bit-depth-max}
    {OAPV_PROFILE_422_10, 2, 2, 10, 10},
    {OAPV_PROFILE_422_12, 2, 2, 10, 12},
    {OAPV_PROFILE_444_10, 2, 3, 10, 10},
    {OAPV_PROFILE_444_12, 2, 3, 10, 12},
    {OAPV_PROFILE_4444_10, 2, 4, 10, 10},
    {OAPV_PROFILE_4444_12, 2, 4, 10, 12},
    {OAPV_PROFILE_400_10, 0, 0, 10, 10},
    {OAPV_PROFILE_4444_16C12, 3, 4, 16, 16},
    {0, 0, 0, 0, 0} // termination
};

static int enc_check_profile(int profile_idc, int cfi, int bit_depth)
{
    int idx = 0;
    while(enc_profile_spec[idx][0] != 0) {
        if(profile_idc == enc_profile_spec[idx][0]) {
            if(cfi >= enc_profile_spec[idx][1] && cfi <= enc_profile_spec[idx][2]) { // check cfi
                if(bit_depth >= enc_profile_spec[idx][3] && bit_depth <= enc_profile_spec[idx][4]) { // check bit-depth
                    return OAPV_OK;
                }
            }
        }
        idx++;
    }
    return OAPV_ERR_INVALID_PROFILE;
}

static int enc_frm_prepare(oapve_ctx_t *ctx, oapve_param_t *param, oapv_imgb_t *imgb_i, oapv_imgb_t *imgb_r)
{
    int i, ret;

    // check basic parameters
    oapv_assert_rv(param->w == imgb_i->w[0], OAPV_ERR_INVALID_WIDTH);
    oapv_assert_rv(param->h == imgb_i->h[0], OAPV_ERR_INVALID_HEIGHT);
    oapv_assert_rv((param->qp >= MIN_QUANT && param->qp <= MAX_QUANT(10)) || param->qp == OAPVE_PARAM_QP_AUTO, OAPV_ERR_INVALID_QP);

    // check width restriction for 422
    if(OAPV_CS_GET_FORMAT(imgb_i->cs) == OAPV_CF_YCBCR422 && imgb_i->w[0] & 0x1) {
        return OAPV_ERR_INVALID_WIDTH; // odd width is spec-out in YCbCr422
    }

    // set functions related to preset
    if(param->preset == OAPV_PRESET_PLACEBO) {
        ctx->fn_enc_blk = enc_block_rdo_placebo;
    }
    else if(param->preset == OAPV_PRESET_SLOW) {
        ctx->fn_enc_blk = enc_block_rdo_slow;
    }
    else if(param->preset == OAPV_PRESET_MEDIUM) {
        ctx->fn_enc_blk = enc_block_rdo_medium;
    }
    else {
        ctx->fn_enc_blk = enc_block;
    }
    // set dimensions
    ctx->w = oapv_div_round_up(param->w, OAPV_MB_W) * OAPV_MB_W;
    ctx->h = oapv_div_round_up(param->h, OAPV_MB_H) * OAPV_MB_H;

    // set QP values
    ctx->qp_offset[Y_C] = 0;
    ctx->qp_offset[U_C] = param->qp_offset_c1;
    ctx->qp_offset[V_C] = param->qp_offset_c2;
    ctx->qp_offset[X_C] = param->qp_offset_c3;

    for(i = 0; i < N_C; i++) {
        ctx->qp[i] = oapv_clip3(MIN_QUANT, MAX_QUANT(10), param->qp + ctx->qp_offset[i]);
    }
    // color information
    ctx->cfi = color_format_to_chroma_format_idc(OAPV_CS_GET_FORMAT(imgb_i->cs));
    ctx->bit_depth_inp = OAPV_CS_GET_BIT_DEPTH(imgb_i->cs);
    ctx->num_c = get_num_comp(ctx->cfi);

    // check whether input frame type is suitable to profile definition
    ret = enc_check_profile(param->profile_idc, ctx->cfi, ctx->bit_depth_inp);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    // check internal bit-depth and companding option
    if(param->profile_idc == OAPV_PROFILE_4444_16C12 && ctx->bit_depth_inp == 16) {
        ctx->bit_depth = 12; // use 12bit internal bit-depth
        ctx->use_companding = 1;
    }
    else {
        ctx->bit_depth = ctx->bit_depth_inp;
        ctx->use_companding = 0;
    }

    // shift parameter for each color component
    ctx->c_sft[Y_C][0] = 0;
    ctx->c_sft[Y_C][1] = 0;
    for(i = 1; i < ctx->num_c; i++) {
        ctx->c_sft[i][0] = get_chroma_sft_w(ctx->cfi);
        ctx->c_sft[i][1] = get_chroma_sft_h(ctx->cfi);
    }

    if(OAPV_CS_GET_FORMAT(imgb_i->cs) == OAPV_CF_PLANAR2) {
        ctx->fn_blk_from_pic[Y_C] = oapv_blk_from_pic_p21x_y;
        ctx->fn_blk_from_pic[U_C] = oapv_blk_from_pic_p21x_uv;
        ctx->fn_blk_from_pic[V_C] = oapv_blk_from_pic_p21x_uv;

        ctx->fn_blk_to_pic[Y_C] = oapv_blk_to_pic_p21x_y;
        ctx->fn_blk_to_pic[U_C] = oapv_blk_to_pic_p21x_uv;
        ctx->fn_blk_to_pic[V_C] = oapv_blk_to_pic_p21x_uv;
        ctx->fn_imgb_pad = imgb_pad_p210;
    }
    else {
        if(ctx->use_companding){
            for(int i = 0; i < ctx->num_c; i++) {
                ctx->fn_blk_from_pic[i] = oapv_blk_from_pic_16C12;
                ctx->fn_blk_to_pic[i] = oapv_blk_to_pic_12E16;
            }
        }
        else{
            for(int i = 0; i < ctx->num_c; i++) {
                ctx->fn_blk_from_pic[i] = oapv_blk_from_pic_16;
                ctx->fn_blk_to_pic[i] = oapv_blk_to_pic_16;
            }
        }
        ctx->fn_imgb_pad = imgb_pad;
    }

    // reject caller buffers too small for the aligned padding extents
    int is_planar2 = (OAPV_CS_GET_FORMAT(imgb_i->cs) == OAPV_CF_PLANAR2);
    for(int c = 0; c < imgb_i->np; c++) {
        int need_w = ctx->w >> (is_planar2 ? 0 : ctx->c_sft[c][0]);
        int need_h = ctx->h >> (is_planar2 ? 0 : ctx->c_sft[c][1]);
        if((s64)imgb_i->s[c] < (s64)need_w * (int)sizeof(pel)) {
            return OAPV_ERR_INVALID_WIDTH;
        }
        if((s64)imgb_i->bsize[c] < (s64)need_h * imgb_i->s[c]) {
            return OAPV_ERR_INVALID_HEIGHT;
        }
    }

    // padding input picture, if needs
    ctx->fn_imgb_pad(imgb_i, ctx->w, ctx->h, ctx->c_sft);

    // calculate tile info
    ret = enc_set_tile_info(ctx->tile, ctx->w, ctx->h, param->tile_w, param->tile_h, &ctx->num_tile_cols, &ctx->num_tile_rows, &ctx->num_tiles);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    // set bitstream buffer for each tile
    int buf_size = ctx->cdesc.max_bs_buf_size / ctx->num_tiles;
    ctx->tile[0].bs_buf_max = buf_size;
    for(i = 1; i < ctx->num_tiles; i++) {
        ctx->tile[i].bs_buf = ctx->tile[i - 1].bs_buf + buf_size;
        ctx->tile[i].bs_buf_max = buf_size;
    }
    // set cores
    for(i = 0; i < ctx->threads; i++) {
        ctx->core[i]->ctx = ctx;
        ctx->core[i]->thread_idx = i;
    }
    // reconstruction picture
    if(imgb_r != NULL) {
        for(int c = 0; c < ctx->num_c; c++) {
            imgb_r->w[c] = imgb_i->w[c];
            imgb_r->h[c] = imgb_i->h[c];
            imgb_r->x[c] = imgb_i->x[c];
            imgb_r->y[c] = imgb_i->y[c];
            // recon plane spans the aligned frame extents
            imgb_r->aw[c] = ctx->w >> (is_planar2 ? 0 : ctx->c_sft[c][0]);
            imgb_r->ah[c] = ctx->h >> (is_planar2 ? 0 : ctx->c_sft[c][1]);
        }
        // reject recon buffers too small for the reconstruction picture
        ret = oapv_imgb_is_valid(imgb_r);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
        ctx->imgb_r = imgb_r;
        imgb_addref(ctx->imgb_r);
    }
    for(i = 0; i < ctx->num_tiles; i++) {
        ctx->tile[i].stat = ENC_TILE_STAT_NOT_ENCODED;
    }

    ctx->param = param;
    ctx->imgb_i = imgb_i;
    imgb_addref(ctx->imgb_i); // increase reference count of input frame
    return OAPV_OK;
}

static int enc_frm_finish(oapve_ctx_t *ctx, oapve_stat_t *stat)
{
    imgb_release(ctx->imgb_i);
    if(ctx->imgb_r) {
        imgb_release(ctx->imgb_r);
        ctx->imgb_r = NULL;
    }
    return OAPV_OK;
}

static int enc_frame(oapve_ctx_t *ctx, oapv_bs_t *bs)
{
    int        ret = OAPV_OK;

    oapv_bs_t  bs_fh;
    oapv_mcpy(&bs_fh, bs, sizeof(oapv_bs_t));

    /* write frame header */
    oapve_set_frame_header(ctx, &ctx->fh);
    oapve_vlc_frame_header(bs, ctx, &ctx->fh);

    u8 *bs_tile_pos = oapv_bsw_sink(bs);
    // sink returns NULL when the output buffer cannot hold the header
    oapv_assert_gv(bs_tile_pos != NULL, ret, OAPV_ERR_OUT_OF_BS_BUF, ERR);

    /* rc init */
    u64 cost_sum = 0;
    if(ctx->param->rc_type != OAPV_RC_CQP) {
        oapve_rc_get_tile_cost_thread(ctx, &cost_sum);

        double bits_pic = ((double)ctx->param->bitrate * 1000) / ((double)ctx->param->fps_num / ctx->param->fps_den);
        for(int i = 0; i < ctx->num_tiles; i++) {
            ctx->tile[i].rc.target_bits_left = bits_pic * ctx->tile[i].rc.cost / cost_sum;
            ctx->tile[i].rc.target_bits = ctx->tile[i].rc.target_bits_left;
        }

        ctx->rc_param.lambda = oapve_rc_estimate_pic_lambda(ctx, cost_sum);
        if (ctx->param->qp == OAPVE_PARAM_QP_AUTO || ctx->rc_param.is_updated != 0) {
            ctx->rc_param.qp = oapve_rc_estimate_pic_qp(ctx->rc_param.lambda);
        }
        else {
            ctx->rc_param.qp = ctx->param->qp;
        }

        for(int c = 0; c < ctx->num_c; c++) {
            ctx->qp[c] = oapv_clip3(MIN_QUANT, MAX_QUANT(10), ctx->rc_param.qp + ctx->qp_offset[c]);
        }
    }

    oapv_tpool_t *tpool = ctx->tpool;
    int           res, tidx = 0, thread_num1 = 0;
    int           parallel_task = (ctx->threads > ctx->num_tiles) ? ctx->num_tiles : ctx->threads;

    /* encode tiles ************************************/
    for(tidx = 0; tidx < (parallel_task - 1); tidx++) {
        tpool->run(ctx->thread_id[tidx], enc_thread_tile,
                   (void *)ctx->core[tidx]);
    }
    ret = enc_thread_tile((void *)ctx->core[tidx]);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    for(thread_num1 = 0; thread_num1 < parallel_task - 1; thread_num1++) {
        res = tpool->join(ctx->thread_id[thread_num1], &ret);
        oapv_assert_gv(res == TPOOL_SUCCESS, ret, OAPV_ERR_FAILED_SYSCALL, ERR);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
    }
    /****************************************************/

    for(int i = 0; i < ctx->num_tiles; i++) {
        oapv_assert_gv(bs_tile_pos + ctx->tile[i].bs_size <= bs->end, ret, OAPV_ERR_OUT_OF_BS_BUF, ERR);
        oapv_mcpy(bs_tile_pos, ctx->tile[i].bs_buf, ctx->tile[i].bs_size);
        bs_tile_pos = bs_tile_pos + ctx->tile[i].bs_size;
        ctx->fh.tile_size[i] = ctx->tile[i].bs_size - OAPV_TILE_SIZE_LEN;
    }
    BSW_MOVE_CUR(bs, bs_tile_pos); // move bs to at the end of tiles

    /* rewrite frame header */
    if(ctx->fh.tile_size_present_in_fh_flag) {
        oapve_vlc_frame_header(&bs_fh, ctx, &ctx->fh);
        oapv_bsw_sink(&bs_fh); // make sure write bits to bs buffer
    }
    if(ctx->param->rc_type != 0) {
        oapve_rc_update_after_pic(ctx, cost_sum);
    }
    return ret;

ERR:
    return ret;
}

static int enc_platform_init(oapve_ctx_t *ctx)
{
    // default settings
    ctx->fn_sad = oapv_tbl_fn_sad_16b;
    ctx->fn_ssd = oapv_tbl_fn_ssd_16b;
    ctx->fn_diff = oapv_tbl_fn_diff_16b;
    ctx->fn_itx_part = oapv_tbl_fn_itx_part;
    ctx->fn_itx = oapv_tbl_fn_itx;
    ctx->fn_itx_adj = oapv_tbl_fn_itx_adj;
    ctx->fn_txb = oapv_tbl_fn_tx;
    ctx->fn_quant = oapv_tbl_fn_quant;
    ctx->fn_dquant = oapv_tbl_fn_dquant;
    ctx->fn_had8x8 = oapv_dc_removed_had8x8;
#if X86_SSE
    int check_cpu, support_sse, support_avx2;

    check_cpu = oapv_check_cpu_info_x86();
    support_sse = (check_cpu >> 0) & 1;
    support_avx2 = (check_cpu >> 2) & 1;

    if(support_avx2) {
        ctx->fn_sad = oapv_tbl_fn_sad_16b_avx;
        ctx->fn_ssd = oapv_tbl_fn_ssd_16b_avx;
        ctx->fn_diff = oapv_tbl_fn_diff_16b_avx;
        ctx->fn_itx_part = oapv_tbl_fn_itx_part_avx;
        ctx->fn_itx = oapv_tbl_fn_itx_avx;
        ctx->fn_itx_adj = oapv_tbl_fn_itx_adj_avx;
        ctx->fn_txb = oapv_tbl_fn_txb_avx;
        ctx->fn_quant = oapv_tbl_fn_quant_avx;
        ctx->fn_dquant = oapv_tbl_fn_dquant_avx;
        ctx->fn_had8x8 = oapv_dc_removed_had8x8_sse;
    }
    else if(support_sse) {
        ctx->fn_ssd = oapv_tbl_fn_ssd_16b_sse;
        ctx->fn_had8x8 = oapv_dc_removed_had8x8_sse;
    }
#elif ARM_NEON
    ctx->fn_sad = oapv_tbl_fn_sad_16b_neon;
    ctx->fn_ssd = oapv_tbl_fn_ssd_16b_neon;
    ctx->fn_diff = oapv_tbl_fn_diff_16b_neon;
    ctx->fn_itx = oapv_tbl_fn_itx_neon;
    ctx->fn_txb = oapv_tbl_fn_txb_neon;
    ctx->fn_quant = oapv_tbl_fn_quant_neon;
    ctx->fn_had8x8 = oapv_dc_removed_had8x8;
#endif
    return OAPV_OK;
}

oapve_t oapve_create(oapve_cdesc_t *cdesc, int *err)
{
    oapve_ctx_t *ctx;
    int          ret;

    DUMP_CREATE(1);

    if(!((cdesc->threads > 0 && cdesc->threads <= OAPV_MAX_THREADS) || cdesc->threads == OAPV_CDESC_THREADS_AUTO)) {
        if(err) *err = OAPV_ERR_INVALID_ARGUMENT;
        return NULL;
    }

    /* memory allocation for ctx and core structure */
    ctx = (oapve_ctx_t *)enc_ctx_alloc();
    if(ctx != NULL) {
        oapv_mcpy(&ctx->cdesc, cdesc, sizeof(oapve_cdesc_t));
        ret = enc_platform_init(ctx);
        oapv_assert_g(ret == OAPV_OK, ERR);

        ret = enc_ready(ctx);
        oapv_assert_g(ret == OAPV_OK, ERR);

        /* set default value for ctx */
        ctx->magic = OAPVE_MAGIC_CODE;
        ctx->id = (oapve_t)ctx;
        if(err) {
            *err = OAPV_OK;
        }
        return (ctx->id);
    }
    else {
        ret = OAPV_ERR;
    }
ERR:
    if(ctx) {
        enc_ctx_free(ctx);
    }
    if(err) {
        *err = ret;
    }
    return NULL;
}

void oapve_delete(oapve_t eid)
{
    oapve_ctx_t *ctx;

    ctx = enc_id_to_ctx(eid);
    oapv_assert_r(ctx);

    DUMP_DELETE();
    enc_flush(ctx);
    enc_ctx_free(ctx);
}

int oapve_encode(oapve_t eid, oapv_frms_t *ifrms, oapvm_t mid, oapv_bitb_t *bitb, oapve_stat_t *stat, oapv_frms_t *rfrms)
{
    oapv_bs_t    bsw;
    oapve_ctx_t *ctx;
    oapv_frm_t  *frm;
    oapv_bs_t   *bs;
    int          i, ret;
    u8          *bs_pos_pbu_beg, *bs_pos_au_beg;

    ctx = enc_id_to_ctx(eid);
    oapv_assert_rv(ctx != NULL && bitb->addr && bitb->bsize > 0, OAPV_ERR_INVALID_ARGUMENT);

    bs = &bsw;

    oapv_bsw_init(bs, bitb->addr, bitb->bsize, NULL);
    oapv_mset(stat, 0, sizeof(oapve_stat_t));

    bs_pos_au_beg = oapv_bsw_sink(bs);

    if(ctx->au_bs_fmt == OAPV_CFG_VAL_AU_BS_FMT_RBAU) {
        oapv_bsw_write(bs, 0, 32); // raw bitstream byte size (skip)
    }
    oapv_bsw_write(bs, 0x61507631, 32); // signature ('aPv1')

    for(i = 0; i < ifrms->num_frms; i++) {
        // prepare for encoding a frame
        frm = &ifrms->frm[i];
        ret = enc_frm_prepare(ctx, &ctx->cdesc.param[i], frm->imgb, (rfrms != NULL) ? rfrms->frm[i].imgb : NULL);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

        // write headers
        bs_pos_pbu_beg = oapv_bsw_sink(bs);            /* store pbu pos to calculate size */
        DUMP_SAVE(0);
        oapv_bsw_write(bs, 0, 32); /* skip pbu_size syntax (later re-write) */
        oapve_vlc_pbu_header(bs, frm->pbu_type, frm->group_id);
        // encode a frame
        ret = enc_frame(ctx, bs);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

        // rewrite pbu_size
        int pbu_size = ((u8 *)oapv_bsw_sink(bs)) - bs_pos_pbu_beg - 4;
        DUMP_SAVE(1);
        DUMP_LOAD(0);
        oapv_bsw_write_direct(bs_pos_pbu_beg, pbu_size, 32);
        DUMP_HLS(pbu_size, pbu_size);
        DUMP_LOAD(1);

        stat->frm_size[i] = pbu_size + 4 /* PUB size length*/;
        fh_to_finfo(&ctx->fh, frm->pbu_type, frm->group_id, &stat->aui.frm_info[i]);

        // add frame hash value of reconstructed frame into metadata list
        if(ctx->use_frm_hash) {
            if(frm->pbu_type == OAPV_PBU_TYPE_PRIMARY_FRAME ||
               frm->pbu_type == OAPV_PBU_TYPE_NON_PRIMARY_FRAME) {
                oapv_assert_rv(mid != NULL, OAPV_ERR_INVALID_ARGUMENT);
                ret = oapv_set_md5_pld(mid, frm->group_id, ctx->imgb_r);
                oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
            }
        }

        // finishing of encoding a frame
        ret = enc_frm_finish(ctx, stat);
        oapv_assert_rv(ret == OAPV_OK, ret);
    }
    stat->aui.num_frms = ifrms->num_frms;

    // encoding metadata
    oapvm_ctx_t *md_list = mid;
    if(md_list != NULL) {
        int num_md = md_list->num;
        for(i = 0; i < num_md; i++) {
            int group_id = md_list->md_arr[i].group_id;
            bs_pos_pbu_beg = oapv_bsw_sink(bs);            /* store pbu pos to calculate size */
            DUMP_SAVE(0);
            oapv_bsw_write(bs, 0, 32); /* skip pbu_size syntax (later re-write) */
            oapve_vlc_pbu_header(bs, OAPV_PBU_TYPE_METADATA, group_id);
            oapve_vlc_metadata(&md_list->md_arr[i], bs);

            // rewrite pbu_size
            int pbu_size = ((u8 *)oapv_bsw_sink(bs)) - bs_pos_pbu_beg - 4;
            DUMP_SAVE(1);
            DUMP_LOAD(0);
            oapv_bsw_write_direct(bs_pos_pbu_beg, pbu_size, 32);
            DUMP_HLS(pbu_size, pbu_size);
            DUMP_LOAD(1);
        }
    }

    if(ctx->au_bs_fmt == OAPV_CFG_VAL_AU_BS_FMT_RBAU) {
        u32 au_size = (u32)((u8 *)oapv_bsw_sink(bs) - bs_pos_au_beg) - 4;
        oapv_bsw_write_direct(bs_pos_au_beg, au_size, 32);
    }

    oapv_bsw_deinit(bs); /* de-init BSW */
    stat->write = bsw_get_write_byte(bs);

    return OAPV_OK;
}

int oapve_config(oapve_t eid, int cfg, void *buf, int *size)
{
    oapve_ctx_t *ctx;
    int          t0;

    ctx = enc_id_to_ctx(eid);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);

    switch(cfg) {
    /* set config **********************************************************/
    case OAPV_CFG_SET_QP:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        t0 = *((int *)buf);
        oapv_assert_rv(t0 >= MIN_QUANT && t0 <= MAX_QUANT(10),
                       OAPV_ERR_INVALID_ARGUMENT);
        ctx->param->qp = t0;
        break;
    case OAPV_CFG_SET_FPS_NUM:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        t0 = *((int *)buf);
        oapv_assert_rv(t0 > 0, OAPV_ERR_INVALID_ARGUMENT);
        ctx->param->fps_num = t0;
        break;
    case OAPV_CFG_SET_FPS_DEN:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        t0 = *((int *)buf);
        oapv_assert_rv(t0 > 0, OAPV_ERR_INVALID_ARGUMENT);
        ctx->param->fps_den = t0;
        break;
    case OAPV_CFG_SET_BPS:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        t0 = *((int *)buf);
        oapv_assert_rv(t0 > 0, OAPV_ERR_INVALID_ARGUMENT);
        ctx->param->bitrate = t0;
        break;
    case OAPV_CFG_SET_USE_FRM_HASH:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        ctx->use_frm_hash = (*((int *)buf)) ? 1 : 0;
        break;
    case OAPV_CFG_SET_AU_BS_FMT:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        t0 = *((int *)buf);
        oapv_assert_rv(t0 == OAPV_CFG_VAL_AU_BS_FMT_RBAU || t0 == OAPV_CFG_VAL_AU_BS_FMT_NONE, OAPV_ERR_INVALID_ARGUMENT);
        ctx->au_bs_fmt = t0;
        break;
    /* get config *******************************************************/
    case OAPV_CFG_GET_QP:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->qp;
        break;
    case OAPV_CFG_GET_WIDTH:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->w;
        break;
    case OAPV_CFG_GET_HEIGHT:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->h;
        break;
    case OAPV_CFG_GET_FPS_NUM:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->fps_num;
        break;
    case OAPV_CFG_GET_FPS_DEN:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->fps_den;
        break;
    case OAPV_CFG_GET_BPS:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->param->bitrate;
        break;
    case OAPV_CFG_GET_AU_BS_FMT:
        oapv_assert_rv(*size == sizeof(int), OAPV_ERR_INVALID_ARGUMENT);
        *((int *)buf) = ctx->au_bs_fmt;
        break;
    default:
        oapv_trace("unknown config value (%d)\n", cfg);
        oapv_assert_rv(0, OAPV_ERR_UNSUPPORTED);
    }

    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// enc of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
static oapvd_ctx_t *dec_id_to_ctx(oapvd_t id)
{
    oapvd_ctx_t *ctx;
    oapv_assert_rv(id, NULL);
    ctx = (oapvd_ctx_t *)id;
    oapv_assert_rv(ctx->magic == OAPVD_MAGIC_CODE, NULL);
    return ctx;
}

static oapvd_ctx_t *dec_ctx_alloc(void)
{
    oapvd_ctx_t *ctx;

    ctx = (oapvd_ctx_t *)oapv_malloc_fast(sizeof(oapvd_ctx_t));

    oapv_assert_rv(ctx != NULL, NULL);
    oapv_mset_x64a(ctx, 0, sizeof(oapvd_ctx_t));

    return ctx;
}

static void dec_ctx_free(oapvd_ctx_t *ctx)
{
    oapv_mfree_fast(ctx);
}

static oapvd_core_t *dec_core_alloc(void)
{
    oapvd_core_t *core;

    core = (oapvd_core_t *)oapv_malloc_fast(sizeof(oapvd_core_t));

    oapv_assert_rv(core, NULL);
    oapv_mset_x64a(core, 0, sizeof(oapvd_core_t));

    return core;
}

static void dec_core_free(oapvd_core_t *core)
{
    oapv_mfree_fast(core);
}

static int dec_block(oapvd_ctx_t *ctx, oapvd_core_t *core, int log2_w, int log2_h, int c)
{
    int bit_depth = ctx->bit_depth;

    // DC prediction
    core->coef[0] = core->dc_diff + core->prev_dc[c];
    core->prev_dc[c] = core->coef[0];
    // Inverse quantization
    ctx->fn_dquant[0](core->coef, core->q_mat[c], log2_w, log2_h, core->dq_shift[c]);
    // Inverse transform
    ctx->fn_itx[0](core->coef, ITX_SHIFT1, ITX_SHIFT2(bit_depth), 1 << log2_w);
    return OAPV_OK;
}

static int dec_set_tile_info(oapvd_tile_t* tile, int w_pel, int h_pel, int tile_w, int tile_h, int num_tile_cols, int num_tiles)
{

    for (int i = 0; i < num_tiles; i++)
    {
        int tx = (i % (num_tile_cols)) * tile_w;
        int ty = (i / (num_tile_cols)) * tile_h;
        tile[i].x = tx;
        tile[i].y = ty;
        tile[i].w = tx + tile_w > w_pel ? w_pel - tx : tile_w;
        tile[i].h = ty + tile_h > h_pel ? h_pel - ty : tile_h;
    }
    return OAPV_OK;
}

static int dec_frm_prepare(oapvd_ctx_t *ctx, oapv_tile_info_t * part, oapv_imgb_t *imgb)
{
    int i, ret;

    oapv_assert_rv(imgb != NULL, OAPV_ERR_MALFORMED_BITSTREAM);

    // the input image buffer must match the frame format signaled in the
    // bitstream; a mismatch (e.g. caused by a resolution change without
    // reallocation) is rejected as an invalid argument
    if (imgb->w[0] != ctx->fh.fi.frame_width || imgb->h[0] != ctx->fh.fi.frame_height) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    // the color space of the input buffer must correspond to the bitstream's
    // chroma format (note: OAPV_CF_PLANAR2 maps to chroma_format_idc 2 so the
    // YCbCr422 -> P210 output path is accepted)
    if (color_format_to_chroma_format_idc(OAPV_CS_GET_FORMAT(imgb->cs)) != ctx->fh.fi.chroma_format_idc) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }

    // validate buffer capacity for each component
    // calculate required buffer size based on frame header information
    int aligned_w = oapv_align_value(ctx->fh.fi.frame_width, OAPV_MB_W);
    int aligned_h = oapv_align_value(ctx->fh.fi.frame_height, OAPV_MB_H);
    int byte_depth = (ctx->fh.fi.bit_depth + 7) / 8; // bytes per pixel

    for(int c = 0; c < imgb->np; c++) {
        int comp_w = aligned_w >> (c > 0 ? get_chroma_sft_w(ctx->fh.fi.chroma_format_idc) : 0);
        int comp_h = aligned_h >> (c > 0 ? get_chroma_sft_h(ctx->fh.fi.chroma_format_idc) : 0);
        int required_stride = comp_w * byte_depth;
        int required_bsize = required_stride * comp_h;

        if(imgb->bsize[c] < required_bsize) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
    }

    ctx->imgb = imgb;
    imgb_addref(ctx->imgb); // increase reference count

    ctx->bit_depth = ctx->fh.fi.bit_depth;
    ctx->cfi = ctx->fh.fi.chroma_format_idc;
    ctx->num_c = get_num_comp(ctx->cfi);
    ctx->c_sft[Y_C][0] = 0;
    ctx->c_sft[Y_C][1] = 0;

    for(int c = 1; c < ctx->num_c; c++) {
        ctx->c_sft[c][0] = get_chroma_sft_w(color_format_to_chroma_format_idc(OAPV_CS_GET_FORMAT(imgb->cs)));
        ctx->c_sft[c][1] = get_chroma_sft_h(color_format_to_chroma_format_idc(OAPV_CS_GET_FORMAT(imgb->cs)));
    }

    ctx->w = oapv_align_value(ctx->fh.fi.frame_width, OAPV_MB_W);
    ctx->h = oapv_align_value(ctx->fh.fi.frame_height, OAPV_MB_H);

    if(ctx->fh.fi.profile_idc == OAPV_PROFILE_444_16C12 || ctx->fh.fi.profile_idc == OAPV_PROFILE_4444_16C12) {
        ctx->disable_companding = 0;
    }

    if(OAPV_CS_GET_FORMAT(imgb->cs) == OAPV_CF_PLANAR2) {
        ctx->fn_blk_to_pic[Y_C] = oapv_blk_to_pic_p21x_y;
        ctx->fn_blk_to_pic[U_C] = oapv_blk_to_pic_p21x_uv;
        ctx->fn_blk_to_pic[V_C] = oapv_blk_to_pic_p21x_uv;
    }
    else {
        if(ctx->fh.fi.profile_idc == OAPV_PROFILE_444_16C12 || ctx->fh.fi.profile_idc == OAPV_PROFILE_4444_16C12) {
            if(ctx->disable_companding){
                for(i = 0; i < ctx->num_c; i++) {
                    ctx->fn_blk_to_pic[i] = oapv_blk_to_pic_16;
                }
            }
            else{
                for(i = 0; i < ctx->num_c; i++) {
                    ctx->fn_blk_to_pic[i] = oapv_blk_to_pic_12E16;
                }
            }
        }
        else{
            for(i = 0; i < ctx->num_c; i++) {
                ctx->fn_blk_to_pic[i] = oapv_blk_to_pic_16;
            }
        }
    }

    int tile_w = ctx->fh.tile_width_in_mbs * OAPV_MB_W;
    int tile_h = ctx->fh.tile_height_in_mbs * OAPV_MB_H;

    ctx->num_tile_cols = (ctx->w + (tile_w - 1)) / tile_w;
    ctx->num_tile_rows = (ctx->h + (tile_h - 1)) / tile_h;

    ret = oapv_validate_tile_topology(ctx->num_tile_cols, ctx->num_tile_rows, &ctx->num_tiles);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    dec_set_tile_info(ctx->tile, ctx->w, ctx->h, tile_w, tile_h, ctx->num_tile_cols, ctx->num_tiles);

    for(i = 0; i < ctx->num_tiles; i++) {
        ctx->tile[i].bs_beg = NULL;
    }
    ctx->tile[0].bs_beg = oapv_bsr_sink(&ctx->bs);

    if(part != NULL) {
        for(i = 0; i < ctx->num_tiles; i++) {
            ctx->tile[i].stat = DEC_TILE_STAT_DO(DEC_TILE_STAT_SKIP); /* bypass decoding */
        }
        for(i = 0; i < part->num_tiles; i++) {
            int idx = part->pos_tiles[i].idx;
            oapv_assert_rv(idx >= 0 && idx < ctx->num_tiles, OAPV_ERR_MALFORMED_BITSTREAM);
            ctx->tile[idx].stat = DEC_TILE_STAT_DO(DEC_TILE_STAT_DECODE);
        }
    }
    else {
        for(i = 0; i < ctx->num_tiles; i++) {
            ctx->tile[i].stat = DEC_TILE_STAT_DO(DEC_TILE_STAT_DECODE);
        }
    }

    return OAPV_OK;
}

static void dec_frm_finish(oapvd_ctx_t *ctx)
{
    imgb_release(ctx->imgb); // decrease reference count
    ctx->imgb = NULL;
}

static int dec_tile_comp(oapvd_tile_t *tile, oapvd_ctx_t *ctx, oapvd_core_t *core, oapv_bs_t *bs, int c, int pic_s, void *pic)
{
    int  mb_h, mb_w, y, x, j, i;
    int  le, ri, to, bo;
    int  ret;
    s16 *pic_t;

    mb_h = OAPV_MB_H >> ctx->c_sft[c][1];
    mb_w = OAPV_MB_W >> ctx->c_sft[c][0];

    le = tile->x >> ctx->c_sft[c][0];        // left position of tile
    ri = (tile->w >> ctx->c_sft[c][0]) + le; // right pixel position of tile
    to = tile->y >> ctx->c_sft[c][1];        // top pixel position of tile
    bo = (tile->h >> ctx->c_sft[c][1]) + to; // bottom pixel position of tile

    for(y = to; y < bo; y += mb_h) {
        for(x = le; x < ri; x += mb_w) {
            for(j = y; j < (y + mb_h); j += OAPV_BLK_H) {
                for(i = x; i < (x + mb_w); i += OAPV_BLK_W) {
                    // clear coefficient buffers in a macroblock
                    oapv_mset_x128(core->coef, 0, sizeof(s16)*OAPV_MB_D);

                    // parse DC coefficient
                    ret = oapvd_vlc_dc_coef(bs, &core->dc_diff, &core->kparam_dc[c]);
                    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

                    // parse AC coefficient
                    ret = oapvd_vlc_ac_coef(bs, core->coef, &core->kparam_ac[c]);
                    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
                    DUMP_COEF(core->dc_diff, core->coef + 1, OAPV_BLK_D - 1, i, j, c);

                    // decode a block
                    ret = dec_block(ctx, core, OAPV_LOG2_BLK_W, OAPV_LOG2_BLK_H, c);
                    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

                    // copy decoded block to image buffer
                    pic_t = (s16 *)((u8 *)pic + j * pic_s) + i;
                    ctx->fn_blk_to_pic[c](OAPV_BLK_W, OAPV_BLK_H, core->coef, (OAPV_BLK_W << 1), pic_t, i, pic_s, ctx->bit_depth);
                }
            }
        }
    }

    /* byte align */
    oapv_bsr_align8(bs);
    /* check actual read size of 'tile()' is equal or smaller than 'tile_data_size' in tile header */
    oapv_assert_rv(BSR_GET_READ_BYTE(bs) <= tile->th.tile_data_size[c], OAPV_ERR_MALFORMED_BITSTREAM);

    return OAPV_OK;
}

static int dec_tile(oapvd_core_t *core, oapvd_tile_t *tile)
{
    int          ret, midx, x, y, c;
    oapvd_ctx_t *ctx = core->ctx;
    oapv_bs_t    bs; // bs for 'tile()' syntax

    oapv_bsr_init(&bs, tile->bs_beg + OAPV_TILE_SIZE_LEN, tile->tile_size, NULL);
    ret = oapvd_vlc_tile_header(&bs, ctx->num_c, &tile->th, tile->tile_size, ctx->bit_depth);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    for(c = 0; c < ctx->num_c; c++) {
        core->qp[c] = tile->th.tile_qp[c];
        u8 dq_scale = oapv_tbl_dq_scale[core->qp[c] % 6];
        core->dq_shift[c] = ctx->bit_depth - 2 - (core->qp[c] / 6);

        core->kparam_dc[c] = OAPV_KPARAM_DC_MAX;
        core->kparam_ac[c] = OAPV_KPARAM_AC_MIN;
        core->prev_dc[c] = 0;

        midx = 0;
        for(y = 0; y < OAPV_BLK_H; y++) {
            for(x = 0; x < OAPV_BLK_W; x++) {
                core->q_mat[c][midx++] = dq_scale * ctx->fh.q_matrix[c][y][x]; // 7bit + 8bit
            }
        }
    }

    for(c = 0; c < ctx->num_c; c++) {
        int  tc, pic_s;
        s16 *pic;
        oapv_bs_t bsc; // bs for 'tile_data()' syntax

        oapv_bsr_init(&bsc, BSR_GET_CUR(&bs), tile->th.tile_data_size[c], NULL);

        if(OAPV_CS_GET_FORMAT(ctx->imgb->cs) == OAPV_CF_PLANAR2) {
            tc = c > 0 ? 1 : 0;
            pic = ctx->imgb->a[tc];
            pic += (c > 1) ? 1 : 0;
            pic_s = ctx->imgb->s[tc];
        }
        else {
            pic = ctx->imgb->a[c];
            pic_s = ctx->imgb->s[c];
        }

        ret = dec_tile_comp(tile, ctx, core, &bsc, c, pic_s, pic);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

        // move bs buffer to next 'tile_data()' component
        BSR_MOVE_BYTE_ALIGN(&bs, tile->th.tile_data_size[c]);
    }

    oapvd_vlc_tile_dummy_data(&bs);
    return OAPV_OK;
}

static int dec_thread_tile(void *arg)
{
    oapv_bs_t     bs;
    int           i, ret, run, tidx = 0, thread_ret = OAPV_OK;

    oapvd_core_t *core = (oapvd_core_t *)arg;
    oapvd_ctx_t  *ctx = core->ctx;
    oapvd_tile_t *tile = ctx->tile;

    while(1) {
        // find not decoded tile
        oapv_tpool_enter_cs(ctx->sync_obj);
        for(i = 0; i < ctx->num_tiles; i++) {
            if(DEC_TILE_STAT_IS_DO(tile[i].stat)) {
                tile[i].stat = DEC_TILE_STAT_ON(tile[i].stat);
                tidx = i;
                break;
            }
        }
        oapv_tpool_leave_cs(ctx->sync_obj);
        if(i == ctx->num_tiles) {
            break; // end of worker thread
        }

        // wait until to know bistream start position
        run = 1;
        while(run) {
            oapv_tpool_enter_cs(ctx->sync_obj);
            if(tile[tidx].bs_beg != NULL) {
                run = 0;
            }
            oapv_tpool_leave_cs(ctx->sync_obj);
        }
        /* read tile size */
        oapv_assert_gv(tile[tidx].bs_beg + OAPV_TILE_SIZE_LEN <= ctx->bs.end, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
        oapv_bsr_init(&bs, tile[tidx].bs_beg, OAPV_TILE_SIZE_LEN, NULL);
        ret = oapvd_vlc_tile_size(&bs, &tile[tidx].tile_size);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

        /* check the tile size is smaller than input bitstream size */
        oapv_assert_gv(tile[tidx].bs_beg + tile[tidx].tile_size + OAPV_TILE_SIZE_LEN <= ctx->bs.end, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);

        oapv_tpool_enter_cs(ctx->sync_obj);
        if(tidx + 1 < ctx->num_tiles) {
            tile[tidx + 1].bs_beg = tile[tidx].bs_beg + OAPV_TILE_SIZE_LEN + tile[tidx].tile_size;
        }
        else {
            ctx->tile_end = tile[tidx].bs_beg + OAPV_TILE_SIZE_LEN + tile[tidx].tile_size;
        }
        oapv_tpool_leave_cs(ctx->sync_obj);

        if(DEC_TILE_STAT_IS_DECODE(tile[i].stat)) {
            ret = dec_tile(core, &tile[tidx]);
        }

        oapv_tpool_enter_cs(ctx->sync_obj);
        if (OAPV_SUCCEEDED(ret)) {
            tile[tidx].stat = DEC_TILE_STAT_DONE(tile[tidx].stat);
        }
        else {
            tile[tidx].stat = DEC_TILE_STAT_ERR(tile[tidx].stat);
            thread_ret = ret;
        }
        oapv_tpool_leave_cs(ctx->sync_obj);
    }
    return thread_ret;

ERR:
    oapv_tpool_enter_cs(ctx->sync_obj);
    tile[tidx].stat = DEC_TILE_STAT_ERR(tile[tidx].stat);
    if (tidx + 1 < ctx->num_tiles)
    {
        tile[tidx + 1].bs_beg = tile[tidx].bs_beg;
    }
    oapv_tpool_leave_cs(ctx->sync_obj);
    return OAPV_ERR_MALFORMED_BITSTREAM;
}

static void dec_flush(oapvd_ctx_t *ctx)
{
    if(ctx->threads >= 2) {
        if(ctx->tpool) {
            // thread controller instance is present
            // terminate the created thread
            for(int i = 0; i < ctx->threads - 1; i++) {
                if(ctx->thread_id[i]) {
                    // valid thread instance
                    ctx->tpool->release(&ctx->thread_id[i]);
                }
            }
            // deinitialize the tpool
            oapv_tpool_deinit(ctx->tpool);
            oapv_mfree(ctx->tpool);
            ctx->tpool = NULL;
        }
    }

    if(ctx->sync_obj != NULL) {
        oapv_tpool_sync_obj_delete(&(ctx->sync_obj));
    }

    for(int i = 0; i < ctx->threads; i++) {
        dec_core_free(ctx->core[i]);
    }
}

static int dec_ready(oapvd_ctx_t *ctx)
{
    int i, ret = OAPV_OK;

    if (ctx->cdesc.threads == OAPV_CDESC_THREADS_AUTO) {
        int num_cores = oapv_get_num_cpu_cores();
        ctx->threads = oapv_min(OAPV_MAX_THREADS, num_cores);
    }
    else {
        ctx->threads = ctx->cdesc.threads;
    }
    oapv_assert_gv(ctx->threads > 0 && ctx->threads <= OAPV_MAX_THREADS, ret, OAPV_ERR_INVALID_ARGUMENT, ERR);

    if(ctx->core[0] == NULL) {
        // create cores
        for(i = 0; i < ctx->threads; i++) {
            ctx->core[i] = dec_core_alloc();
            oapv_assert_gv(ctx->core[i], ret, OAPV_ERR_OUT_OF_MEMORY, ERR);
            ctx->core[i]->ctx = ctx;
        }
    }

    // initialize the threads to NULL
    for(i = 0; i < ctx->threads; i++) {
        ctx->thread_id[i] = 0;
    }

    // get the context synchronization handle
    ctx->sync_obj = oapv_tpool_sync_obj_create();
    oapv_assert_gv(ctx->sync_obj != NULL, ret, OAPV_ERR_UNKNOWN, ERR);

    if(ctx->threads >= 2) {
        ctx->tpool = oapv_malloc(sizeof(oapv_tpool_t));
        oapv_tpool_init(ctx->tpool, ctx->threads - 1);
        for(i = 0; i < ctx->threads - 1; i++) {
            ctx->thread_id[i] = ctx->tpool->create(ctx->tpool, i);
            oapv_assert_gv(ctx->thread_id[i] != NULL, ret, OAPV_ERR_UNKNOWN, ERR);
        }
    }
    return OAPV_OK;

ERR:
    dec_flush(ctx);

    return ret;
}

static int dec_platform_init(oapvd_ctx_t *ctx)
{
    // default settings
    ctx->fn_itx = oapv_tbl_fn_itx;
    ctx->fn_dquant = oapv_tbl_fn_dquant;

#if X86_SSE
    int check_cpu, support_sse, support_avx2;

    check_cpu = oapv_check_cpu_info_x86();
    support_sse = (check_cpu >> 0) & 1;
    support_avx2 = (check_cpu >> 2) & 1;

    if(support_avx2) {
        ctx->fn_itx = oapv_tbl_fn_itx_avx;
        ctx->fn_dquant = oapv_tbl_fn_dquant_avx;
    }
    else if(support_sse) {
        ctx->fn_itx = oapv_tbl_fn_itx;
        ctx->fn_dquant = oapv_tbl_fn_dquant;
    }
#elif ARM_NEON
    ctx->fn_itx = oapv_tbl_fn_itx_neon;
    ctx->fn_dquant = oapv_tbl_fn_dquant;
#endif
    return OAPV_OK;
}

oapvd_t oapvd_create(oapvd_cdesc_t *cdesc, int *err)
{
    oapvd_ctx_t *ctx;
    int          ret;

    DUMP_CREATE(0);
    ctx = NULL;

    if(!((cdesc->threads > 0 && cdesc->threads <= OAPV_MAX_THREADS) || cdesc->threads == OAPV_CDESC_THREADS_AUTO)) {
        if(err) *err = OAPV_ERR_INVALID_ARGUMENT;
        return NULL;
    }

    /* memory allocation for ctx and core structure */
    ctx = (oapvd_ctx_t *)dec_ctx_alloc();
    oapv_assert_gv(ctx != NULL, ret, OAPV_ERR_OUT_OF_MEMORY, ERR);
    oapv_mcpy(&ctx->cdesc, cdesc, sizeof(oapvd_cdesc_t));

    /* initialize platform-specific variables */
    ret = dec_platform_init(ctx);
    oapv_assert_g(ret == OAPV_OK, ERR);

    /* ready for decoding */
    ret = dec_ready(ctx);
    oapv_assert_g(ret == OAPV_OK, ERR);

    ctx->magic = OAPVD_MAGIC_CODE;
    ctx->id = (oapvd_t)ctx;
    if(err) {
        *err = OAPV_OK;
    }
    return (ctx->id);

ERR:
    if(ctx) {
        dec_ctx_free(ctx);
    }
    if(err) {
        *err = ret;
    }
    return NULL;
}

void oapvd_delete(oapvd_t did)
{
    oapvd_ctx_t *ctx;
    ctx = dec_id_to_ctx(did);
    oapv_assert_r(ctx);

    DUMP_DELETE();
    dec_flush(ctx);
    dec_ctx_free(ctx);
}

int oapvd_decode(oapvd_t did, oapv_bitb_t *bitb, oapv_frms_t *ofrms, oapvm_t mid, oapvd_stat_t *stat)
{
    oapvd_ctx_t *ctx;
    oapv_pbuh_t  pbuh;
    int          ret = OAPV_OK;
    u32          pbu_size;
    u32          signature;
    u32          cur_read_size = 0;
    int          nfrms = 0;

    ctx = dec_id_to_ctx(did);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    // required in/out pointers must be valid (mid is optional)
    oapv_assert_rv(bitb && bitb->addr && ofrms && stat, OAPV_ERR_INVALID_ARGUMENT);
    oapv_mset(stat, 0, sizeof(oapvd_stat_t));

    // read signature ('aPv1')
    oapv_assert_rv(bitb->ssize > 4, OAPV_ERR_MALFORMED_BITSTREAM);
    if(bitb->bsize > 0) {
        oapv_assert_rv(bitb->ssize <= bitb->bsize, OAPV_ERR_INVALID_ARGUMENT);
    }
    ret = oapv_bsr_read_direct(bitb->addr, 32, &signature);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    oapv_assert_rv(signature == 0x61507631, OAPV_ERR_MALFORMED_BITSTREAM);
    cur_read_size += 4;
    stat->read += 4;

    // decode PBUs
    do {
        oapv_bs_t   *bs;
        u32 remain = bitb->ssize - cur_read_size;
        oapv_assert_gv((remain >= 8), ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
        oapv_bsr_init(&ctx->bs, (u8 *)bitb->addr + cur_read_size, remain, NULL);
        bs = &ctx->bs;

        ret = oapvd_vlc_pbu_size(bs, &pbu_size); // read pbu_size (4 byte)
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
        remain -= 4; // size of pbu_size syntax
        oapv_assert_gv(pbu_size >= 4 && pbu_size <= remain, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);

        ret = oapvd_vlc_pbu_header(bs, &pbuh);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

        if(pbuh.pbu_type == OAPV_PBU_TYPE_PRIMARY_FRAME ||
           pbuh.pbu_type == OAPV_PBU_TYPE_NON_PRIMARY_FRAME ||
           pbuh.pbu_type == OAPV_PBU_TYPE_PREVIEW_FRAME ||
           pbuh.pbu_type == OAPV_PBU_TYPE_DEPTH_FRAME ||
           pbuh.pbu_type == OAPV_PBU_TYPE_ALPHA_FRAME) {

            oapv_assert_gv(nfrms < OAPV_MAX_NUM_FRAMES, ret, OAPV_ERR_REACHED_MAX, ERR);

            ret = oapvd_vlc_frame_header(bs, &ctx->fh);
            oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

            ret = dec_frm_prepare(ctx, NULL, ofrms->frm[nfrms].imgb);
            oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

            int           thread_ret;
            oapv_tpool_t *tpool = ctx->tpool;
            int           parallel_task = 1;
            int           tidx = 0;

            parallel_task = (ctx->threads > ctx->num_tiles) ? ctx->num_tiles : ctx->threads;

            /* decode tiles ************************************/
            for(tidx = 0; tidx < (parallel_task - 1); tidx++) {
                tpool->run(ctx->thread_id[tidx], dec_thread_tile,
                           (void *)ctx->core[tidx]);
            }
            ret = dec_thread_tile((void *)ctx->core[tidx]);
            for(tidx = 0; tidx < parallel_task - 1; tidx++) {
                tpool->join(ctx->thread_id[tidx], &thread_ret);
                if(OAPV_FAILED(thread_ret)) {
                    ret = thread_ret;
                }
            }
            /****************************************************/

            /* READ FILLER HERE !!! */

            if(OAPV_SUCCEEDED(ret) && ctx->use_frm_hash) {
                ret = oapv_imgb_set_md5(ctx->imgb);
            }
            else {
                oapv_imgb_clr_md5(ctx->imgb);
            }
            // following function should be called even error cases,
            // because input imgb's ref count needs to decreased.
            // after this function, ctx->imgb cannot be accessed.
            dec_frm_finish(ctx);

            // check thread's return value
            oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

            fh_to_finfo(&ctx->fh, pbuh.pbu_type, pbuh.group_id, &stat->aui.frm_info[nfrms]);

            ofrms->frm[nfrms].pbu_type = pbuh.pbu_type;
            ofrms->frm[nfrms].group_id = pbuh.group_id;
            stat->frm_size[nfrms] = pbu_size + 4; /* byte size of 'pbu_size' syntax */
            nfrms++;

            // go to the end of frame data for next PDU
            oapv_bsr_move(bs, ctx->tile_end);
        }
        else if(pbuh.pbu_type == OAPV_PBU_TYPE_METADATA) {
            ret = oapvd_vlc_metadata(bs, pbu_size, mid, pbuh.group_id);
            oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
        }
        else if(pbuh.pbu_type == OAPV_PBU_TYPE_FILLER) {
            ret = oapvd_vlc_filler(bs, (pbu_size - 4));
            oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
        }
        cur_read_size += pbu_size + 4 /* byte size of 'pbu_size' syntax */;
        stat->read += BSR_GET_READ_BYTE(bs);
    } while(cur_read_size < bitb->ssize);
    stat->aui.num_frms = nfrms;
    oapv_assert_gv(ofrms->num_frms == nfrms, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
    return ret;

ERR:
    return ret;
}

int oapvd_config(oapvd_t did, int cfg, void *buf, int *size)
{
    oapvd_ctx_t *ctx;

    ctx = dec_id_to_ctx(did);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);

    switch(cfg) {
    /* set config ************************************************************/
    case OAPV_CFG_SET_USE_FRM_HASH:
        ctx->use_frm_hash = (*((int *)buf)) ? 1 : 0;
        break;

    case OAPV_CFG_SET_DISABLE_COMPANDING:
        ctx->disable_companding = (*((int *)buf)) ? 1 : 0;
        break;
    default:
        oapv_assert_rv(0, OAPV_ERR_UNSUPPORTED);
    }
    return OAPV_OK;
}

int oapvd_info(void *au, int au_size, oapv_au_info_t *aui)
{
    int ret, frm_count = 0;
    u32 signature;
    u32 cur_read_size = 0;
    oapv_bs_t bs;

    DUMP_SET(0);

    // read signature ('aPv1')
    oapv_assert_rv(au_size > 4, OAPV_ERR_MALFORMED_BITSTREAM);
    ret = oapv_bsr_read_direct(au, 32, &signature);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    oapv_assert_rv(signature == 0x61507631, OAPV_ERR_MALFORMED_BITSTREAM);
    cur_read_size += 4;

    // parse PBUs
    do {
        u32 pbu_size = 0;
        u32 remain = au_size - cur_read_size;
        oapv_assert_rv(remain >= 8, OAPV_ERR_MALFORMED_BITSTREAM);
        oapv_bsr_init(&bs, (u8 *)au + cur_read_size, remain, NULL);

        ret = oapvd_vlc_pbu_size(&bs, &pbu_size); // read pbu_size (4 byte)
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
        remain -= 4; // size of pbu_size syntax
        oapv_assert_rv(pbu_size <= remain, OAPV_ERR_MALFORMED_BITSTREAM);

        /* pbu header */
        oapv_pbuh_t pbuh;
        ret = oapvd_vlc_pbu_header(&bs, &pbuh); // read pbu_header() (4 byte)
        oapv_assert_rv(OAPV_SUCCEEDED(ret), OAPV_ERR_MALFORMED_BITSTREAM);
        if(pbuh.pbu_type == OAPV_PBU_TYPE_AU_INFO) {
            // parse access_unit_info in PBU
            oapv_aui_t ai;

            ret = oapvd_vlc_au_info(&bs, &ai);
            oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

            aui->num_frms = ai.num_frames;
            for(int i = 0; i < ai.num_frames; i++) {
                fi_to_finfo(&ai.frame_info[i], ai.pbu_type[i], ai.group_id[i], &aui->frm_info[i]);
            }
            return OAPV_OK; // founded access_unit_info, no need to read more PBUs
        }
        if(OAPV_PBU_TYPE_IS_FRAME(pbuh.pbu_type)) {
            // parse frame_info in PBU
            oapv_fi_t fi;

            oapv_assert_rv(frm_count < OAPV_MAX_NUM_FRAMES, OAPV_ERR_REACHED_MAX)
            ret = oapvd_vlc_frame_info(&bs, &fi);
            oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

            fi_to_finfo(&fi, pbuh.pbu_type, pbuh.group_id, &aui->frm_info[frm_count]);
            frm_count++;
        }
        aui->num_frms = frm_count;
        cur_read_size += pbu_size + 4; /* 4byte is for pbu_size syntax itself */
    } while(cur_read_size < au_size);
    DUMP_SET(1);
    return OAPV_OK;
}

int oapvd_info_pbu(void *pbu, int pbu_size, oapv_pbu_info_t *pbu_info)
{
    oapv_bs_t bs;
    oapv_assert_rv(pbu_size >= 4, OAPV_ERR_INVALID_ARGUMENT);

    oapv_bsr_init(&bs, pbu, pbu_size, NULL);

    /* parse pbu_header() */
    pbu_info->pbu_type = oapv_bsr_read(&bs, 8);
    pbu_info->group_id = oapv_bsr_read(&bs, 16);

    return OAPV_OK;
}

int oapvd_info_frame(void *pbu, int pbu_size, oapv_frm_info_t *frm_info, oapv_tile_info_t *tile_info)
{
    oapv_bs_t    bs;
    oapv_pbuh_t  pbuh;
    oapv_fh_t    fh;
    int          i, j, ret = OAPV_OK;

    oapv_assert_rv(pbu_size >= (OAPV_PBU_HEADER_BYTE + OAPV_FRAME_INFO_BYTE), OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsr_init(&bs, pbu, pbu_size, NULL);

    DUMP_SET(0);
    // decode PBU header
    ret = oapvd_vlc_pbu_header(&bs, &pbuh);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    // check frame type PBU
    oapv_assert_gv(OAPV_PBU_TYPE_IS_FRAME(pbuh.pbu_type), ret, OAPV_ERR_INVALID_ARGUMENT, ERR);

    // decode frame header
    ret = oapvd_vlc_frame_header(&bs, &fh);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    fh_to_finfo(&fh, pbuh.pbu_type, pbuh.group_id, frm_info);

    if(tile_info != NULL) {
        int pic_w, pic_h, tile_w, tile_h, tile_cols, tile_rows;

        oapv_mset(tile_info, 0, sizeof(oapv_tile_info_t)); // clear

        tile_w = fh.tile_width_in_mbs * OAPV_MB_W;
        tile_h = fh.tile_height_in_mbs * OAPV_MB_H;

        pic_w = ((fh.fi.frame_width + (OAPV_MB_W - 1)) >> OAPV_LOG2_MB_W) << OAPV_LOG2_MB_W;
        pic_h = ((fh.fi.frame_height + (OAPV_MB_H - 1)) >> OAPV_LOG2_MB_H) << OAPV_LOG2_MB_H;

        tile_cols = (pic_w + (tile_w - 1)) / tile_w;
        tile_rows = (pic_h + (tile_h - 1)) / tile_h;

        ret = oapv_validate_tile_topology(tile_cols, tile_rows, NULL);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

        oapv_tile_pos_t * tpos = tile_info->pos_tiles;

        for(i = 0; i < tile_rows; i++) {
            for(j = 0; j < tile_cols; j++) {
                tpos->x_mb = fh.tile_width_in_mbs * j;
                tpos->y_mb = fh.tile_height_in_mbs * i;

                if(tpos->x_mb + fh.tile_width_in_mbs > (pic_w >> OAPV_LOG2_MB_W)) {
                    tpos->w_mb = (pic_w >> OAPV_LOG2_MB_W) - tpos->x_mb;
                }
                else {
                    tpos->w_mb = fh.tile_width_in_mbs;
                }
                if(tpos->h_mb + fh.tile_height_in_mbs > (pic_h >> OAPV_LOG2_MB_H)) {
                    tpos->h_mb = (pic_h >> OAPV_LOG2_MB_H) - tpos->y_mb;
                }
                else {
                    tpos->h_mb = fh.tile_height_in_mbs;
                }
                tpos->idx = i * tile_cols + j;
                tpos++;
            }
        }
        tile_info->num_tiles = tile_cols * tile_rows;
    }

ERR:
    DUMP_SET(1);
    return ret;
}

int oapvd_decode_frame(oapvd_t did, oapv_bitb_t *bitb, oapv_imgb_t *imgb, oapvd_stat_t *stat, oapv_tile_info_t * part)
{
    oapvd_ctx_t *ctx;
    oapv_pbuh_t  pbuh;
    int          ret = OAPV_OK;

    ctx = dec_id_to_ctx(did);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    oapv_mset(stat, 0, sizeof(oapvd_stat_t));

    oapv_bs_t   *bs;
    oapv_assert_gv((bitb->ssize >= 8), ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
    if(bitb->bsize > 0) {
        oapv_assert_gv((bitb->ssize <= bitb->bsize), ret, OAPV_ERR_INVALID_ARGUMENT, ERR);
    }
    oapv_bsr_init(&ctx->bs, (u8 *)bitb->addr, bitb->ssize, NULL);
    bs = &ctx->bs;

    // parse PBU header
    ret = oapvd_vlc_pbu_header(bs, &pbuh);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
    // check frame type PBU
    oapv_assert_gv(OAPV_PBU_TYPE_IS_FRAME(pbuh.pbu_type), ret, OAPV_ERR_INVALID_ARGUMENT, ERR);

    // parse frame header
    ret = oapvd_vlc_frame_header(bs, &ctx->fh);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    // be ready to decode start
    ret = dec_frm_prepare(ctx, part, imgb);
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    int           thread_ret;
    oapv_tpool_t *tpool = ctx->tpool;
    int           parallel_task = 1;
    int           tidx = 0;

    if(part != NULL) {
        oapv_assert_gv(part->num_tiles > 0 && part->num_tiles <= ctx->num_tiles, ret, OAPV_ERR_INVALID_ARGUMENT, ERR);
        parallel_task = (ctx->threads > part->num_tiles) ? part->num_tiles : ctx->threads;
    }
    else {
        parallel_task = (ctx->threads > ctx->num_tiles) ? ctx->num_tiles : ctx->threads;
    }

    /* decode tiles ************************************/
    for(tidx = 0; tidx < (parallel_task - 1); tidx++) {
        tpool->run(ctx->thread_id[tidx], dec_thread_tile,
                   (void *)ctx->core[tidx]);
    }
    ret = dec_thread_tile((void *)ctx->core[tidx]);
    for(tidx = 0; tidx < parallel_task - 1; tidx++) {
        tpool->join(ctx->thread_id[tidx], &thread_ret);
        if(OAPV_FAILED(thread_ret)) {
            ret = thread_ret;
        }
    }
    /****************************************************/

    /* READ FILLER HERE !!! */
    if(OAPV_SUCCEEDED(ret) && ctx->use_frm_hash) {
        ret = oapv_imgb_set_md5(imgb);
    }
    else {
        oapv_imgb_clr_md5(imgb);
    }
    // following function should be called even error cases,
    // because input imgb's ref count needs to decreased.
    // after this function, ctx->imgb cannot be accessed.
    dec_frm_finish(ctx);

    // check thread's return value
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

    // go to the end of frame data
    oapv_bsr_move(bs, ctx->tile_end);
    // set stat
    fh_to_finfo(&ctx->fh, pbuh.pbu_type, pbuh.group_id, &stat->aui.frm_info[0]);
    stat->read = stat->frm_size[0] = BSR_GET_READ_BYTE(bs);
    stat->aui.num_frms = 1;
    return ret;

ERR:
    return ret;

    return OAPV_OK;
}

int oapvd_decode_auinfo(oapvd_t did, oapv_bitb_t *bitb, oapv_au_info_t *aui)
{
    int        ret;
    oapv_bs_t  bs;
    oapv_aui_t ai;

    if(bitb->bsize > 0) {
        oapv_assert_rv(bitb->ssize <= bitb->bsize, OAPV_ERR_INVALID_ARGUMENT);
    }
    oapv_bsr_init(&bs, bitb->addr, bitb->ssize, NULL);

    DUMP_SET(0);
    ret = oapvd_vlc_au_info(&bs, &ai);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    oapv_mset(aui, 0, sizeof(oapv_au_info_t)); // clear

    aui->num_frms = ai.num_frames;
    for(int i = 0; i < ai.num_frames; i++) {
        fi_to_finfo(&ai.frame_info[i], ai.pbu_type[i], ai.group_id[i], &aui->frm_info[i]);
    }
    DUMP_SET(1);
    return OAPV_OK;
}

int oapvd_decode_metadata(oapvd_t did, oapv_bitb_t *bitb, oapvm_payload_t *pld)
{
    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

const char *oapv_version(unsigned int *ver_num)
{
    static char oapv_version_string[16];
    snprintf(oapv_version_string, sizeof(oapv_version_string), "%d.%d.%d.%d",
        OAPV_VER_APISET, OAPV_VER_MAJOR, OAPV_VER_MINOR, OAPV_VER_PATCH);

    if(ver_num != NULL)
        *ver_num = OAPV_VER_NUM;

    return (char*)oapv_version_string;
}
