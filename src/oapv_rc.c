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

#include "oapv_rc.h"

int oapve_rc_get_tile_cost(oapve_ctx_t* ctx, oapve_core_t* core, oapve_tile_t* tile)
{
    int sum = 0;
    tile->rc.number_pixel = 0;
    for (int c = Y_C; c < ctx->num_comp; c++)
    {
        int step_w = 8 << ctx->comp_sft[c][0];
        int step_h = 8 << ctx->comp_sft[c][1];
        for (int y = 0; y < tile->h; y += step_h)
        {
            for (int x = 0; x < tile->w; x += step_w)
            {
                int tx = tile->x + x;
                int ty = tile->y + y;

                ctx->fn_imgb_to_blk_rc(ctx->imgb, c, tx, ty, 8, 8, core->coef);
                sum += ctx->fn_had8x8(core->coef, 8);
                tile->rc.number_pixel += 64;
            }
        }
    }

    tile->rc.cost = sum;

    return OAPV_OK;
}

int get_tile_cost_thread(void* arg)
{
    oapve_core_t* core = (oapve_core_t*)arg;
    oapve_ctx_t* ctx = core->ctx;
    oapve_tile_t* tile = ctx->tile;
    int tidx = 0, ret = OAPV_OK, i;

    while (1) {
        // find not processed tile
        oapv_tpool_enter_cs(ctx->sync_obj);
        for (i = 0; i < ctx->num_tiles; i++)
        {
            if (tile[i].stat == ENC_TILE_STAT_NOT_ENCODED)
            {
                tile[i].stat = ENC_TILE_STAT_ON_ENCODING;
                tidx = i;
                break;
            }
        }
        oapv_tpool_leave_cs(ctx->sync_obj);
        if (i == ctx->num_tiles)
        {
            break;
        }

        ret = oapve_rc_get_tile_cost(ctx, core, &tile[tidx]);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);

        oapv_tpool_enter_cs(ctx->sync_obj);
        tile[tidx].stat = ENC_TILE_STAT_ENCODED;
        oapv_tpool_leave_cs(ctx->sync_obj);
    }
ERR:
    return ret;
}

int oapve_rc_get_tile_cost_thread(oapve_ctx_t* ctx, u64* sum)
{
    for (int i = 0; i < ctx->num_tiles; i++) {
        ctx->tile[i].stat = ENC_TILE_STAT_NOT_ENCODED;
    }

    oapv_tpool_t* tpool = ctx->tpool;
    int parallel_task = (ctx->cdesc.threads > ctx->num_tiles) ? ctx->num_tiles : ctx->cdesc.threads;

    // run new threads
    int tidx = 0;
    for (tidx = 0; tidx < (parallel_task - 1); tidx++) {
        tpool->run(ctx->thread_id[tidx], get_tile_cost_thread, (void*)ctx->core[tidx]);
    }
    // use main thread
    int ret = get_tile_cost_thread((void*)ctx->core[tidx]);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    for (int thread_num1 = 0; thread_num1 < parallel_task - 1; thread_num1++) {
        int res = tpool->join(ctx->thread_id[thread_num1], &ret);
        oapv_assert_rv(res == TPOOL_SUCCESS, ret);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    }

    *sum = 0;
    for (int i = 0; i < ctx->num_tiles; i++)
    {
        *sum += ctx->tile[i].rc.cost;
        ctx->tile[i].stat = ENC_TILE_STAT_NOT_ENCODED;
    }

    return ret;
}

static double rc_calculate_lambda(double alpha, double beta, double cost_pixel, double bits_pixel)
{
    return ((alpha / 256.0) * pow(cost_pixel / bits_pixel, beta));
}

double oapve_rc_estimate_pic_lambda(oapve_ctx_t* ctx, double cost)
{
    int num_pixel = ctx->w * ctx->h;
    for (int c = 1; c < ctx->num_comp; c++)
    {
        num_pixel += (ctx->w * ctx->h) >> (ctx->comp_sft[c][0] + ctx->comp_sft[c][1]);
    }

    double alpha = ctx->rc_param.alpha;
    double beta = ctx->rc_param.beta;
    double bpp = ((double)ctx->param->bitrate * 1000) / ((double)num_pixel * ((double)ctx->param->fps_num / ctx->param->fps_den));

    double est_lambda = rc_calculate_lambda(alpha, beta, pow(cost / (double)num_pixel, OAPV_RC_BETA), bpp);
    est_lambda = oapv_clip3(0.1, 10000.0, est_lambda);
    const int lambda_prec = 1000000;

    est_lambda = (double)((s64)(est_lambda * (double)lambda_prec + 0.5)) / (double)lambda_prec;

    return est_lambda;
}

int oapve_rc_estimate_pic_qp(double lambda)
{
    int qp = (int)(4.2005 * log(lambda) + 13.7122 + 0.5) + OAPV_RC_QP_OFFSET;
    qp = oapv_clip3(MIN_QUANT, MAX_QUANT(10), qp);
    return qp;
}

void oapve_rc_get_qp(oapve_ctx_t* ctx, oapve_tile_t* tile, int frame_qp, int* qp)
{
    double   alpha = ctx->rc_param.alpha;
    double   beta = ctx->rc_param.beta;

    double cost_pixel = tile->rc.cost / (double)tile->rc.number_pixel;
    cost_pixel = pow(cost_pixel, OAPV_RC_BETA);

    double bit_pixel =  (double)tile->rc.target_bits / (double)tile->rc.number_pixel;
    double est_lambda = rc_calculate_lambda(alpha, beta, cost_pixel, bit_pixel);

    int min_qp = frame_qp - 2 - OAPV_RC_QP_OFFSET;
    int max_qp = frame_qp + 2 - OAPV_RC_QP_OFFSET;

    double max_lambda = exp(((double)(max_qp + 0.49) - 13.7122) / 4.2005);
    double min_lambda = exp(((double)(min_qp - 0.49) - 13.7122) / 4.2005);

    const int LAMBDA_PREC = 1000000;
    est_lambda = oapv_clip3(min_lambda, max_lambda, est_lambda);
    est_lambda = (double)((s64)(est_lambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
    *qp = (int)(4.2005 * log(est_lambda) + 13.7122 + 0.5);
    *qp = oapv_clip3(min_qp, max_qp, *qp);
    *qp += OAPV_RC_QP_OFFSET;
    *qp = oapv_clip3(MIN_QUANT, MAX_QUANT(10), *qp);
}

void oapve_rc_update_after_pic(oapve_ctx_t* ctx, double cost)
{
    int num_pixel = ctx->w * ctx->h;
    for (int c = 1; c < ctx->num_comp; c++)
    {
        num_pixel += (ctx->w * ctx->h) >> (ctx->comp_sft[c][0] + ctx->comp_sft[c][1]);
    }

    int total_bits = 0;
    for (int i = 0; i < ctx->num_tiles; i++)
    {
        total_bits += ctx->fh.tile_size[i] * 8;
    }

    double ln_bpp = log(pow(cost / (double)num_pixel, OAPV_RC_BETA));
    double diff_lambda = (ctx->rc_param.beta) * (log((double)total_bits) - log(((double)ctx->param->bitrate * 1000 / ((double)ctx->param->fps_num / ctx->param->fps_den))));

    diff_lambda = oapv_clip3(-0.125, 0.125, 0.25 * diff_lambda);
    ctx->rc_param.alpha = (ctx->rc_param.alpha) * exp(diff_lambda);
    ctx->rc_param.beta = (ctx->rc_param.beta) + diff_lambda / ln_bpp;
}
