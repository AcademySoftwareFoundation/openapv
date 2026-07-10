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
#include "oapv_metadata.h"

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////
#define BSW_FLUSH_4BYTE(bs) {                     \
        if ((bs)->cur + 4 <= (bs)->end) {         \
            *(bs)->cur++ = ((bs)->code >> 24) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 16) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 8) & 0xFF;  \
            *(bs)->cur++ = ((bs)->code) & 0xFF;       \
        } else {                                  \
            (bs)->cur += 4;                       \
        }                                         \
        (bs)->code = 0;                           \
        (bs)->leftbits = 32;                      \
    }

#define BSW_FLUSH_8BYTE(bs) {                     \
        if ((bs)->cur + 8 <= (bs)->end) {         \
            *(bs)->cur++ = ((bs)->code >> 56) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 48) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 40) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 32) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 24) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 16) & 0xFF; \
            *(bs)->cur++ = ((bs)->code >> 8) & 0xFF;  \
            *(bs)->cur++ = ((bs)->code) & 0xFF;       \
        } else {                                  \
            (bs)->cur += 8;                       \
        }                                         \
        (bs)->code = 0;                           \
        (bs)->leftbits = 64;                      \
    }

#define BSW_WRITE_32BITS(bs, code32, nbits) { \
        (code32) <<= (32 - (nbits)); \
        if((nbits) < (bs)->leftbits) { \
            (bs)->code |= ((code32) >> (32 - (bs)->leftbits)); \
            (bs)->leftbits -= (nbits); \
        } \
        else { \
            (bs)->code |= ((code32) >> (32 - (bs)->leftbits)); \
            (code32) <<= (bs)->leftbits; \
            (nbits) -= (bs)->leftbits; \
            BSW_FLUSH_4BYTE(bs); \
            if((nbits) > 0) { \
                (bs)->code |= ((code32) >> (32 - (bs)->leftbits)); \
                (bs)->leftbits -= (nbits); \
            } \
        } \
    }

#define BSW_WRITE_64BITS(bs, code64, nbits) { \
        (code64) <<= (64 - (nbits)); \
        if((nbits) < (bs)->leftbits) { \
            (bs)->code |= ((code64) >> (64 - (bs)->leftbits)); \
            (bs)->leftbits -= (nbits); \
        } \
        else { \
            (bs)->code |= ((code64) >> (64 - (bs)->leftbits)); \
            (code64) <<= (bs)->leftbits; \
            (nbits) -= (bs)->leftbits; \
            BSW_FLUSH_8BYTE(bs); \
            if((nbits) > 0) { \
                (bs)->code |= ((code64) >> (64 - (bs)->leftbits)); \
                (bs)->leftbits -= (nbits); \
            } \
        } \
    }


#define ADD_BITS_TO_CODE(val, nb, code) ((code) << (nb) | (val))

static const u8 enc_prefix_vlc[3][2] = {{1, 0xFF}, {0, 0}, {0, 1}}; // 0xFF is don't care

static void enc_vlc_write(oapv_bs_t *bs, int val, int k)
{
    u64 code = 0; /* this value can exceed 32bit in custom QP matrix with all '1' */
    u32 symbol = val;
    int nb = 0;
    int vlc_idx = oapv_min(val >> k, 2);  // 'val' is always positive or zero

    while(symbol >= (1 << k)) {
        symbol -= (1 << k);
        if(nb < 2) {
            code = ADD_BITS_TO_CODE(enc_prefix_vlc[vlc_idx][nb], 1, code);
        }
        else {
            code = ADD_BITS_TO_CODE(0, 1, code);
            k++;
        }
        nb++;
    }
    if(nb < 2) {
        code = ADD_BITS_TO_CODE(enc_prefix_vlc[vlc_idx][nb], 1, code);
    }
    else {
        code = ADD_BITS_TO_CODE(1, 1, code);
    }
    nb++;
    if(k > 0) {
        code = ADD_BITS_TO_CODE(symbol, k, code);
        nb += k;
    }
    // write to bitstream buffer
    BSW_WRITE_64BITS(bs, code, nb);
}

static u64 enc_vlc_write_to_code(oapv_bs_t *bs, int val, int k, int *nbits)
{
    u64 code64 = 0;
    u32 symbol = val;
    int nb = 0;
    int vlc_idx = oapv_min(val >> k, 2);  // 'val' is always positive or zero

    while(symbol >= (1 << k)) {
        symbol -= (1 << k);
        if(nb < 2) {
            code64 = ADD_BITS_TO_CODE(enc_prefix_vlc[vlc_idx][nb], 1, code64);
        }
        else {
            code64 = ADD_BITS_TO_CODE(0, 1, code64);
            k++;
        }
        nb++;
    }
    if(nb < 2) {
        code64 = ADD_BITS_TO_CODE(enc_prefix_vlc[vlc_idx][nb], 1, code64);
    }
    else {
        code64 = ADD_BITS_TO_CODE(1, 1, code64);

    }
    nb++;
    if(k > 0) {
        code64 = ADD_BITS_TO_CODE(symbol, k, code64);
        nb += k;
    }
    *nbits = nb;
    return code64;
}

static int enc_vlc_quantization_matrix(oapv_bs_t *bs, oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    for(int cidx = 0; cidx < ctx->num_c; cidx++) {
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 8; x++) {
                oapv_bsw_write(bs, fh->q_matrix[cidx][y][x], 8);
                DUMP_HLS(fh->q_matrix, fh->q_matrix[cidx][y][x]);
            }
        }
    }
    return 0;
}

static int enc_vlc_tile_info(oapv_bs_t *bs, oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    oapv_bsw_write(bs, fh->tile_width_in_mbs, 20);
    DUMP_HLS(fh->tile_width_in_mbs, fh->tile_width_in_mbs);
    oapv_bsw_write(bs, fh->tile_height_in_mbs, 20);
    DUMP_HLS(fh->tile_height_in_mbs, fh->tile_height_in_mbs);
    oapv_bsw_write(bs, fh->tile_size_present_in_fh_flag, 1);
    DUMP_HLS(fh->tile_size_present_in_fh_flag, fh->tile_size_present_in_fh_flag);
    if(fh->tile_size_present_in_fh_flag) {
        for(int i = 0; i < ctx->num_tiles; i++) {
            oapv_bsw_write(bs, fh->tile_size[i], 32);
            DUMP_HLS(fh->tile_size, fh->tile_size[i]);
        }
    }
    return 0;
}

int oapve_vlc_dc_coef(oapv_bs_t *bs, int dc_diff, int *kparam_dc)
{
    u64 code64 = 0; /* this value can exceed 32bit in custom QP matrix with all '1' */
    int nbits;
    int abs_dc_diff = oapv_abs32(dc_diff);

    code64 = enc_vlc_write_to_code(bs, abs_dc_diff, *kparam_dc, &nbits);

    if(abs_dc_diff) {
        int sign_dc_diff = oapv_get_sign32(dc_diff);
        code64 = ADD_BITS_TO_CODE(sign_dc_diff, 1, code64);
        *kparam_dc = KPARAM_DC(abs_dc_diff);
        nbits++;
    }
    else {
        *kparam_dc = OAPV_KPARAM_DC_MIN;
    }
    BSW_WRITE_64BITS(bs, code64, nbits);

    return OAPV_OK;
}

void oapve_vlc_ac_coef(oapv_bs_t* bs, s16* coef, int * kparam_ac)
{
    int       scan_pos, first_ac = 1;
    int       sign, level, run = 0;
    s16       c;
    const u8 *scanp = oapv_tbl_scan;
    int       k_run = OAPV_KPARAM_RUN_MIN;
    int       k_ac = *kparam_ac;
    u64       code64;
    int       nbits;

    for (scan_pos = 1; scan_pos < OAPV_BLK_D; scan_pos++) {
        c = coef[scanp[scan_pos]];
        if(c) {
            // run coding
            code64 = oapve_tbl_vlc_code[run][k_run][0];
            nbits = oapve_tbl_vlc_code[run][k_run][1];
            k_run = KPARAM_RUN(run); // update kparam for run
            run = 0; // reset run

            BSW_WRITE_64BITS(bs, code64, nbits);

            // level and sign coding
            level = oapv_abs16(c);
            if(level < 101) { // early termination
                code64  = oapve_tbl_vlc_code[level - 1][k_ac][0];
                nbits = oapve_tbl_vlc_code[level - 1][k_ac][1];
            }
            else {
                code64 = enc_vlc_write_to_code(bs, level - 1, k_ac, &nbits);
            }
            k_ac = KPARAM_AC(level);
            if (first_ac) {
                first_ac = 0;
                *kparam_ac = k_ac;
            }
            sign  = oapv_get_sign16(c);
            code64 = ADD_BITS_TO_CODE(sign, 1, code64);
            nbits++;

            BSW_WRITE_64BITS(bs, code64, nbits);
        }
        else { // zero coefficient value
            run++;
        }
    }
    if(run > 0) { // last position can be zero
        code64 = oapve_tbl_vlc_code[run][k_run][0];
        nbits = oapve_tbl_vlc_code[run][k_run][1];

        BSW_WRITE_64BITS(bs, code64, nbits);
    }
}

void oapve_set_frame_header(oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    oapve_param_t *param = ctx->param;

    oapv_mset(fh, 0, sizeof(oapv_fh_t));
    fh->fi.profile_idc = param->profile_idc;
    fh->fi.level_idc = param->level_idc;
    fh->fi.band_idc = param->band_idc;
    fh->fi.frame_width = param->w;
    fh->fi.frame_height = param->h;
    fh->fi.chroma_format_idc = ctx->cfi;
    fh->fi.bit_depth = ctx->bit_depth;
    fh->fi.use_companding = ctx->use_companding;

    fh->tile_width_in_mbs = param->tile_w / OAPV_MB_W;
    fh->tile_height_in_mbs = param->tile_h / OAPV_MB_H;
    fh->color_description_present_flag = param->color_description_present_flag;
    fh->color_primaries = param->color_primaries;
    fh->transfer_characteristics = param->transfer_characteristics;
    fh->matrix_coefficients = param->matrix_coefficients;
    fh->full_range_flag = param->full_range_flag;

    fh->use_q_matrix = param->use_q_matrix;
    if(fh->use_q_matrix == 0) {
        for(int cidx = 0; cidx < ctx->num_c; cidx++) {
            for(int y = 0; y < OAPV_BLK_H; y++) {
                for(int x = 0; x < OAPV_BLK_W; x++) {
                    fh->q_matrix[cidx][y][x] = 16;
                }
            }
        }
    }
    else {
        int mod = (1 << OAPV_LOG2_BLK) - 1;
        for(int c=  0; c <OAPV_MAX_CC; c++) {
            for(int i = 0; i < OAPV_BLK_D; i++) {
                fh->q_matrix[c][i >> OAPV_LOG2_BLK][i & mod] = param->q_matrix[c][i];
            }
        }
    }
    fh->tile_size_present_in_fh_flag = 0;
}

void oapve_set_tile_header(oapve_ctx_t *ctx, oapv_th_t *th, int tile_idx, int qp)
{
    oapv_mset(th, 0, sizeof(oapv_th_t));

    for(int c = 0; c < ctx->num_c; c++) {
        th->tile_qp[c] = oapv_clip3(MIN_QUANT, MAX_QUANT(10), qp + ctx->qp_offset[c]);
    }
    th->tile_index = tile_idx;

    for(int i = 0; i < N_C; i++) {
        // this setting is required to prevent underflow at dummy writing tile header due to '-1'.
        th->tile_data_size[i] = 1;
    }
}

int oapve_vlc_frame_info(oapv_bs_t *bs, oapv_fi_t *fi)
{
    oapv_bsw_write(bs, fi->profile_idc, 8);
    DUMP_HLS(fi->profile_idc, fi->profile_idc);
    oapv_bsw_write(bs, fi->level_idc, 8);
    DUMP_HLS(fi->level_idc, fi->level_idc);
    oapv_bsw_write(bs, fi->band_idc, 3);
    DUMP_HLS(fi->band_idc, fi->band_idc);
    oapv_bsw_write(bs, 0, 5); // reserved_zero_5bits
    DUMP_HLS(reserved_zero, 0);
    oapv_bsw_write(bs, fi->frame_width, 24);
    DUMP_HLS(fi->frame_width, fi->frame_width);
    oapv_bsw_write(bs, fi->frame_height, 24);
    DUMP_HLS(fi->frame_height, fi->frame_height);
    oapv_bsw_write(bs, fi->chroma_format_idc, 4);
    DUMP_HLS(fi->chroma_format_idc, fi->chroma_format_idc);
    oapv_bsw_write(bs, fi->bit_depth - 8, 4);
    DUMP_HLS(fi->bit_depth, fi->bit_depth - 8);
    oapv_bsw_write(bs, fi->capture_time_distance, 8);
    DUMP_HLS(fi->capture_time_distance, fi->capture_time_distance);
    oapv_bsw_write1(bs, fi->use_companding);
    DUMP_HLS(fi->use_companding, fi->use_companding);
    oapv_bsw_write(bs, 0, 7); // reserved_zero_7bits
    DUMP_HLS(reserved_zero, 0);
    return OAPV_OK;
}

int oapve_vlc_frame_header(oapv_bs_t *bs, oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);

    oapve_vlc_frame_info(bs, &fh->fi);
    oapv_bsw_write(bs, 0, 8); // reserved_zero_8bits
    DUMP_HLS(reserved_zero, 0);
    oapv_bsw_write1(bs, fh->color_description_present_flag);
    DUMP_HLS(fh->color_description_present_flag, fh->color_description_present_flag);
    if(fh->color_description_present_flag) {
        oapv_bsw_write(bs, fh->color_primaries, 8);
        DUMP_HLS(fh->color_primaries, fh->color_primaries);
        oapv_bsw_write(bs, fh->transfer_characteristics, 8);
        DUMP_HLS(fh->transfer_characteristics, fh->transfer_characteristics);
        oapv_bsw_write(bs, fh->matrix_coefficients, 8);
        DUMP_HLS(fh->matrix_coefficients, fh->matrix_coefficients);
        oapv_bsw_write1(bs, fh->full_range_flag);
        DUMP_HLS(fh->full_range_flag, fh->full_range_flag);
    }
    oapv_bsw_write1(bs, fh->use_q_matrix);
    DUMP_HLS(fh->use_q_matrix, fh->use_q_matrix);
    if(fh->use_q_matrix) {
        enc_vlc_quantization_matrix(bs, ctx, fh);
    }
    enc_vlc_tile_info(bs, ctx, fh);

    oapv_bsw_write(bs, 0, 8); // reserved_zero_8bits
    DUMP_HLS(reserved_zero, 0);
    return OAPV_OK;
}

int oapve_vlc_tile_size(oapv_bs_t *bs, int tile_size)
{
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_bsw_write(bs, tile_size, 32);
    DUMP_HLS(tile_size, tile_size);
    return OAPV_OK;
}

int oapve_vlc_tile_header(oapve_ctx_t *ctx, oapv_bs_t *bs, oapv_th_t *th)
{
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    th->tile_header_size = 5;                    // tile_header_size + tile_index + reserved_zero_8bits
    th->tile_header_size += (ctx->num_c * 5); // tile_data_size + tile_qp

    oapv_bsw_write(bs, th->tile_header_size, 16);
    DUMP_HLS(th->tile_header_size, th->tile_header_size);
    oapv_bsw_write(bs, th->tile_index, 16);
    DUMP_HLS(th->tile_index, th->tile_index);
    for(int c = 0; c < ctx->num_c; c++) {
        oapv_bsw_write(bs, th->tile_data_size[c], 32);
        DUMP_HLS(th->tile_data_size, th->tile_data_size[c]);
    }
    for(int c = 0; c < ctx->num_c; c++) {
        oapv_bsw_write(bs, th->tile_qp[c], 8);
        DUMP_HLS(th->tile_qp, th->tile_qp[c]);
    }
    oapv_bsw_write(bs, th->reserved_zero_8bits, 8);
    DUMP_HLS(th->reserved_zero_8bits, th->reserved_zero_8bits);

    return OAPV_OK;
}

int oapve_vlc_au_info(oapv_bs_t *bs, oapve_ctx_t *ctx, oapv_frms_t *frms, oapv_bs_t **bs_fi_pos)
{
    oapv_bsw_write(bs, frms->num_frms, 16);
    DUMP_HLS(num_frames, frms->num_frms);
    for(int fidx = 0; fidx < frms->num_frms; fidx++) {
        oapv_bsw_write(bs, frms->frm[fidx].pbu_type, 8);
        DUMP_HLS(pbu_type, frms->frm[fidx].pbu_type);
        oapv_bsw_write(bs, frms->frm[fidx].group_id, 16);
        DUMP_HLS(group_id, frms->frm[fidx].group_id);
        oapv_bsw_write(bs, 0, 8);
        DUMP_HLS(reserved_zero_8bits, 0);
        memcpy(*(bs_fi_pos + sizeof(oapv_bs_t) * fidx), bs, sizeof(oapv_bs_t)); /* store fi pos in au to re-write */
        oapve_vlc_frame_info(bs, &ctx->fh.fi);
    }

    oapv_bsw_write(bs, 0, 8);
    DUMP_HLS(reserved_zero_8bits, 0);
    while(!bsw_is_align8(bs)) {
        oapv_bsw_write1(bs, 0);
    }
    return OAPV_OK;
}

int oapve_vlc_pbu_size(oapv_bs_t *bs, int pbu_size)
{
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_bsw_write(bs, pbu_size, 32);
    DUMP_HLS(pbu_size, pbu_size);
    return OAPV_OK;
}

int oapve_vlc_pbu_header(oapv_bs_t *bs, int pbu_type, int group_id)
{
    DUMP_HLS(pbu_size, 0);
    oapv_bsw_write(bs, pbu_type, 8);
    DUMP_HLS(pbu_type, pbu_type);
    oapv_bsw_write(bs, group_id, 16);
    DUMP_HLS(group_id, group_id);
    oapv_bsw_write(bs, 0, 8); // reserved_zero_8bit
    DUMP_HLS(reserved_zero, 0);

    return OAPV_OK;
}

int oapve_vlc_metadata(oapv_md_t *md, oapv_bs_t *bs)
{
    u8 *bs_pos_md;
    bs_pos_md = oapv_bsw_sink(bs);

    oapv_bsw_write(bs, 0, 32); // raw bitstream byte size (skip)
    DUMP_SAVE(0);
    DUMP_HLS(metadata_size, 0);

    oapv_mdp_t *mdp = md->md_payload;

    while(mdp != NULL) {
        u32 mdp_pltype = mdp->pld_type;
        while(mdp_pltype >= 255) {
            oapv_bsw_write(bs, 0xFF, 8);
            DUMP_HLS(payload_type, 0xFF);
            mdp_pltype -= 255;
        }
        oapv_bsw_write(bs, mdp_pltype, 8);
        DUMP_HLS(payload_type, mdp_pltype);

        u32 mdp_size = mdp->pld_size;
        while(mdp_size >= 255) {
            oapv_bsw_write(bs, 0xFF, 8);
            DUMP_HLS(payload_size, 0xFF);
            mdp_size -= 255;
        }
        oapv_bsw_write(bs, mdp_size, 8);
        DUMP_HLS(payload_size, mdp_size);

        for(u32 i = 0; i < mdp->pld_size; i++) {
            u8 *payload_data = (u8 *)mdp->pld_data;
            oapv_bsw_write(bs, payload_data[i], 8);
            DUMP_HLS(payload_data, payload_data[i]);
        }

        mdp = mdp->next;
    }
    u32 md_size = (u32)((u8 *)oapv_bsw_sink(bs) - bs_pos_md) - 4;
    oapv_bsw_write_direct(bs_pos_md, md_size, 32);
    DUMP_SAVE(1);
    DUMP_LOAD(0);
    DUMP_HLS(metadata_size, md_size);
    DUMP_LOAD(1);

    return OAPV_OK;
}

static __inline int get_vlc_rate(int val, int k)
{
    if (val < 100 && k < 5)
    {
        return oapve_tbl_vlc_code[val][k][1];
    }

    int code_len = 0;
    code_len++;
    if (val < (1 << k))
    {
        code_len += k;
    }
    else
    {
        val -= (1 << k);
        code_len++;
        if (val < (1 << k))
        {
            code_len += k;
        }
        else
        {
            val -= (1 << k);
            while (val >= (1 << k))
            {
                code_len++;
                val -= (1 << k);
                k++;
            }
            code_len += k + 1;
        }
    }

    return code_len;
}

double oapve_vlc_get_level_cost(int coef, int k, double lambda)
{
    s32 rate = 0;
    rate = get_vlc_rate(coef, k);
    if (coef)
        rate += 1; // sign
    return (rate * lambda);
}

double oapve_vlc_get_run_cost(int run, int k, double lambda)
{
    s32 rate = 0;
    rate = get_vlc_rate(run, k);
    return (rate * lambda);
}

int oapve_vlc_get_coef_rate(oapve_core_t* core, s16* coef, int c)
{
    int rate = 0;
    int rice_run = 0;
    int prev_run = 0;

    // DC
    int level = oapv_abs32(coef[0] - core->prev_dc[c]);
    int rice_level = core->kparam_dc[c];

    rate += get_vlc_rate(level, rice_level);
    if(level) {
        rate++;
    }

    u32 num_coeff = OAPV_BLK_D, scan_pos, run = 0;
    const u8* scanp = oapv_tbl_scan;

    // AC
    rice_level = core->kparam_ac[c];
    for(scan_pos = 1; scan_pos < num_coeff; scan_pos++) {
        int coef_cur = coef[scanp[scan_pos]];
        if(coef_cur) {
            level = oapv_abs16(coef_cur);
            rice_run = oapv_min(prev_run >> 2, 2);
            rice_level = oapv_clip3(OAPV_KPARAM_AC_MIN, OAPV_KPARAM_AC_MAX, rice_level);

            rate += get_vlc_rate(run, rice_run);
            rate += get_vlc_rate(level - 1, rice_level);
            if(level) {
                rate++;
            }

            prev_run = run;
            run = 0;
            rice_level = level >> 2;
        }
        else {
            run++;
        }
    }
    if(run != 0) {
        rice_run = oapv_min(prev_run >> 2, 2);
        rate += get_vlc_rate(run, rice_run);
    }

    return rate;
}

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
// Refill the bit buffer with one byte. On buffer end, do NOT read past it:
// set is_eob and fill 'code' with all 1-bits on purpose. The VLC exp-golomb
// loops terminate on a 1-bit, so 1-bits make them exit at once instead of
// spinning on 0-bits and reading further past the end. Callers see is_eob
// (BSR_IS_UNEXPECTED_EOB) and return OAPV_ERR_MALFORMED_BITSTREAM, so the
// garbage value produced here is discarded.
#define BSR_FLUSH_1BYTE(bs) {                           \
        if((bs)->cur < (bs)->end) {                     \
            (bs)->code = ((u64)(*((bs)->cur++))) << 56; \
            (bs)->leftbits = 8;                         \
        }                                               \
        else {                                          \
            (bs)->code = ~(u64)0;                       \
            (bs)->leftbits = 8;                         \
            (bs)->is_eob = 1;                           \
        }                                               \
    }

#define BSR_READ_1BIT(bs, bit) {                \
        (bit) = ((bs)->code >> 63) & 0x1;       \
        (bs)->code <<= 1;                       \
        (bs)->leftbits -= 1;                    \
    }

static int dec_vlc_read_kparam0(oapv_bs_t *bs)
{
    int symbol;
    int flag, k;

    symbol = 2;
    k = 0;

    while(1) {
        if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
        BSR_READ_1BIT(bs, flag);

        if(flag) {
            break;
        }
        else {
            k++;
        }
    }
    oapv_assert_rv(k < 32, -1); /* prevent too large (impossible) k value */

    if(k > 0) {
        symbol += ((u32)0xFFFFFFFF) >> (32 - k);

        while(bs->leftbits < k) {
            symbol += bs->code >> (64 - k);
            k -= bs->leftbits;
            BSR_FLUSH_1BYTE(bs);
        }
        symbol += bs->code >> (64 - k);
        bs->code <<= k;
        bs->leftbits -= k;
    }
    return symbol;
}

static int dec_vlc_read_1bit_read(oapv_bs_t *bs)
{
    int symbol;
    int flag, k;

    if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
    BSR_READ_1BIT(bs, flag);

    symbol = (1 + flag);
    k = 0;
    if(flag) { // parse_exp_golomb
        while(1) {
            if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
            BSR_READ_1BIT(bs, flag);

            if(flag) {
                break;
            }
            else {
                k++;
            }
        }
    }

    oapv_assert_rv(k < 32, -1); /* prevent too large (impossible) k value */

    if(k > 0) {
        symbol += ((u32)0xFFFFFFFF) >> (32 - k);

        while(bs->leftbits < k) {
            symbol += bs->code >> (64 - k);
            k -= bs->leftbits;
            BSR_FLUSH_1BYTE(bs);
        }
        symbol += bs->code >> (64 - k);
        bs->code <<= k;
        bs->leftbits -= k;
    }
    return symbol;
}

static int dec_vlc_read(oapv_bs_t *bs, int k)
{
    int symbol;
    int flag;
    int parse_exp_golomb = 0;

    if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
    BSR_READ_1BIT(bs, flag);

    if(flag == 0) {
        if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
        BSR_READ_1BIT(bs, flag);

        symbol = (1 + flag) << k;
        parse_exp_golomb = flag;
    }
    else {
        symbol = 0;
    }
    if(parse_exp_golomb) {
        while(1) {
            if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
            BSR_READ_1BIT(bs, flag);

            if(flag == 1) {
                break;
            }
            else {
                symbol += (1 << k);
                k++;
            }
        }
    }

    oapv_assert_rv(k < 32, -1); /* prevent too large (impossible) k value */

    if(k > 0) {
        while(bs->leftbits < k) {
            symbol += bs->code >> (64 - k);
            k -= bs->leftbits;
            BSR_FLUSH_1BYTE(bs);
        }
        symbol += bs->code >> (64 - k);
        bs->code <<= k;
        bs->leftbits -= k;
    }
    return symbol;
}

static int dec_vlc_q_matrix(oapv_bs_t *bs, oapv_fh_t *fh)
{
    int num_comp = get_num_comp(fh->fi.chroma_format_idc);
    for(int cidx = 0; cidx < num_comp; cidx++) {
        for(int y = 0; y < OAPV_BLK_H; y++) {
            for(int x = 0; x < OAPV_BLK_W; x++) {
                fh->q_matrix[cidx][y][x] = oapv_bsr_read(bs, 8);
                DUMP_HLS(fh->q_matrix, fh->q_matrix[cidx][y][x]);
                oapv_assert_rv(fh->q_matrix[cidx][y][x] > 0, OAPV_ERR_MALFORMED_BITSTREAM);
            }
        }
    }
    return OAPV_OK;
}

static int dec_vlc_tile_info(oapv_bs_t *bs, oapv_fh_t *fh)
{
    int ret;
    int pic_w, pic_h, tile_w, tile_h, tile_cols, tile_rows;

    fh->tile_width_in_mbs = oapv_bsr_read(bs, 20);
    DUMP_HLS(fh->tile_width_in_mbs, fh->tile_width_in_mbs);
    oapv_assert_rv(fh->tile_width_in_mbs > 0, OAPV_ERR_MALFORMED_BITSTREAM);

    fh->tile_height_in_mbs = oapv_bsr_read(bs, 20);
    DUMP_HLS(fh->tile_height_in_mbs, fh->tile_height_in_mbs);
    oapv_assert_rv(fh->tile_height_in_mbs > 0, OAPV_ERR_MALFORMED_BITSTREAM);

    /* set various value */
    pic_w = ((fh->fi.frame_width + (OAPV_MB_W - 1)) >> OAPV_LOG2_MB_W) << OAPV_LOG2_MB_W;
    pic_h = ((fh->fi.frame_height + (OAPV_MB_H - 1)) >> OAPV_LOG2_MB_H) << OAPV_LOG2_MB_H;

    tile_w = fh->tile_width_in_mbs * OAPV_MB_W;
    tile_h = fh->tile_height_in_mbs * OAPV_MB_H;

    tile_cols = (pic_w + (tile_w - 1)) / tile_w;
    tile_rows = (pic_h + (tile_h - 1)) / tile_h;

    ret = oapv_validate_tile_topology(tile_cols, tile_rows, NULL);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    fh->tile_size_present_in_fh_flag = oapv_bsr_read1(bs);
    DUMP_HLS(fh->tile_size_present_in_fh_flag, fh->tile_size_present_in_fh_flag);

    if(fh->tile_size_present_in_fh_flag) {
        for(int i = 0; i < tile_cols * tile_rows; i++) {
            fh->tile_size[i] = oapv_bsr_read(bs, 32);
            DUMP_HLS(fh->tile_size, fh->tile_size[i]);
            oapv_assert_rv(fh->tile_size[i] > 0, OAPV_ERR_MALFORMED_BITSTREAM);
        }
    }
    return OAPV_OK;
}

int oapvd_vlc_dc_coef(oapv_bs_t *bs, int *dc_diff, int *kparam_dc)
{
    int abs_dc_diff;
    int sign;

    abs_dc_diff = dec_vlc_read(bs, *kparam_dc);
    if(abs_dc_diff < 0) // dec_vlc_read error sentinel
        return OAPV_ERR_MALFORMED_BITSTREAM;
    if(abs_dc_diff) {
        if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
        BSR_READ_1BIT(bs, sign);
        *dc_diff = oapv_set_sign16(abs_dc_diff, sign);
        *kparam_dc = KPARAM_DC(abs_dc_diff);
    }
    else {
        *dc_diff = 0;
        *kparam_dc = OAPV_KPARAM_DC_MIN;
    }

    if(BSR_IS_UNEXPECTED_EOB(bs))
        return OAPV_ERR_MALFORMED_BITSTREAM;
    return OAPV_OK;
}

int oapvd_vlc_ac_coef(oapv_bs_t *bs, s16 *coef, int *kparam_ac)
{
    int        level, run, k_ac, k_run, flag;
    int        scan_pos_offset;
    const u8  *scanp;

    scanp = oapv_tbl_scan;
    scan_pos_offset = 1;

    int first_ac = 1;
    k_run = OAPV_KPARAM_RUN_MIN;
    k_ac = *kparam_ac;

    do {
        // run parsing
        if(k_run == 0) { // early termination
            if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
            BSR_READ_1BIT(bs, flag);

            if(flag) {
                run = 0;
            }
            else {
                if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
                BSR_READ_1BIT(bs, flag);

                if(flag == 0) {
                    run = 1;
                }
                else {
                    run = dec_vlc_read_kparam0(bs);
                }
            }
        }
        else {
            run = dec_vlc_read(bs, k_run);
        }

        oapv_assert_rv(run >= 0 && run <= OAPV_BLK_D - scan_pos_offset, OAPV_ERR_MALFORMED_BITSTREAM);

        // here, no need to set 'zero-run' in coef; it's already initialized to zero.

        scan_pos_offset += run;
        if(scan_pos_offset >= OAPV_BLK_D) {
            oapv_assert_rv(scan_pos_offset == OAPV_BLK_D, OAPV_ERR_MALFORMED_BITSTREAM); // bitstream error
            break; // reached the end of coefficients without level value
        }
        k_run = KPARAM_RUN(run); // backup

        // level parsing
        if(k_ac == 0) {
            if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
            BSR_READ_1BIT(bs, flag);

            if(flag) {
                level = 1;
            }
            else {
                level = dec_vlc_read_1bit_read(bs) + 1;
            }
        }
        else {
            level = dec_vlc_read(bs, k_ac) + 1;
        }
        k_ac = KPARAM_AC(level);

        if(first_ac) {
            first_ac = 0;
            *kparam_ac = k_ac; // backup
        }

        // sign parsing
        if(bs->leftbits == 0) BSR_FLUSH_1BYTE(bs);
        BSR_READ_1BIT(bs, flag);

        coef[scanp[scan_pos_offset++]] = oapv_set_sign16(level, flag);

        if(scan_pos_offset >= OAPV_BLK_D) {
            break;
        }
    } while(1);

    if(BSR_IS_UNEXPECTED_EOB(bs))
        return OAPV_ERR_MALFORMED_BITSTREAM;
    return OAPV_OK;
}

int oapvd_vlc_au_size(oapv_bs_t *bs, u32 *au_size)
{
    u32 size;
    size = oapv_bsr_read(bs, 32);
    oapv_assert_rv(size > 0 && size < 0xFFFFFFFF, OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    *au_size = size;
    return OAPV_OK;
}

int oapvd_vlc_pbu_size(oapv_bs_t *bs, u32 *pbu_size)
{
    u32 size;
    size = oapv_bsr_read(bs, 32);
    DUMP_HLS(pbu_size, size);
    oapv_assert_rv(size > 0 && size < 0xFFFFFFFF, OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    *pbu_size = size;
    return OAPV_OK;
}

int oapvd_vlc_pbu_header(oapv_bs_t *bs, oapv_pbuh_t *pbuh)
{
    int reserved_zero;
    pbuh->pbu_type = oapv_bsr_read(bs, 8);
    DUMP_HLS(pbu_type, pbuh->pbu_type);
    oapv_assert_rv(pbuh->pbu_type != 0, OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!(pbuh->pbu_type >= 3 && pbuh->pbu_type <= 24), OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!(pbuh->pbu_type >= 28 && pbuh->pbu_type <= 64), OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!(pbuh->pbu_type >= 68), OAPV_ERR_MALFORMED_BITSTREAM);

    pbuh->group_id = oapv_bsr_read(bs, 16);
    DUMP_HLS(group_id, pbuh->group_id);
    oapv_assert_rv(pbuh->group_id >= 0 && pbuh->group_id < 0xFFFF, OAPV_ERR_MALFORMED_BITSTREAM);

    reserved_zero = oapv_bsr_read(bs, 8);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    return OAPV_OK;
}

int oapvd_vlc_frame_info(oapv_bs_t *bs, oapv_fi_t *fi)
{
    int reserved_zero;

    fi->profile_idc = oapv_bsr_read(bs, 8);
    DUMP_HLS(fi->profile_idc, fi->profile_idc);

    fi->level_idc = oapv_bsr_read(bs, 8);
    DUMP_HLS(fi->level_idc, fi->level_idc);

    fi->band_idc = oapv_bsr_read(bs, 3);
    DUMP_HLS(fi->band_idc, fi->band_idc);

    reserved_zero = oapv_bsr_read(bs, 5);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    fi->frame_width = oapv_bsr_read(bs, 24);
    DUMP_HLS(fi->frame_width, fi->frame_width);
    oapv_assert_rv(fi->frame_width > 0, OAPV_ERR_MALFORMED_BITSTREAM);

    fi->frame_height = oapv_bsr_read(bs, 24);
    DUMP_HLS(fi->frame_height, fi->frame_height);
    oapv_assert_rv(fi->frame_height > 0, OAPV_ERR_MALFORMED_BITSTREAM);

    fi->chroma_format_idc = oapv_bsr_read(bs, 4);
    DUMP_HLS(fi->chroma_format_idc, fi->chroma_format_idc);
    oapv_assert_rv(fi->chroma_format_idc >= 0 && fi->chroma_format_idc <= 4, OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(fi->chroma_format_idc != 1, OAPV_ERR_MALFORMED_BITSTREAM);

    fi->bit_depth = oapv_bsr_read(bs, 4);
    DUMP_HLS(fi->bit_depth, fi->bit_depth);
    oapv_assert_rv(fi->bit_depth >= 2 && fi->bit_depth <= 8, OAPV_ERR_MALFORMED_BITSTREAM);
    fi->bit_depth += 8;

    fi->capture_time_distance = oapv_bsr_read(bs, 8);
    DUMP_HLS(fi->capture_time_distance, fi->capture_time_distance);

    fi->use_companding = oapv_bsr_read1(bs);
    DUMP_HLS(fi->use_companding, fi->use_companding);
    if(fi->profile_idc != OAPV_PROFILE_4444_16C12) {
        oapv_assert_rv(fi->use_companding == 0, OAPV_ERR_MALFORMED_BITSTREAM);
    }

    reserved_zero = oapv_bsr_read(bs, 7);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    // check frame width in case of 422 format.
    if(fi->chroma_format_idc == 2) {
        // frame_width should be multiple of 2
        oapv_assert_rv((fi->frame_width & 0x1) == 0, OAPV_ERR_INVALID_WIDTH);
    }

    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    return OAPV_OK;
}

int oapvd_vlc_au_info(oapv_bs_t *bs, oapv_aui_t *aui)
{
    int ret;
    int reserved_zero_8bits;

    aui->num_frames = oapv_bsr_read(bs, 16);
    DUMP_HLS(num_frames, aui->num_frames);
    oapv_assert_rv(aui->num_frames <= OAPV_MAX_NUM_FRAMES, OAPV_ERR_REACHED_MAX);
    for(int fidx = 0; fidx < aui->num_frames; fidx++) {
        aui->pbu_type[fidx] = oapv_bsr_read(bs, 8);
        DUMP_HLS(pbu_type, aui->pbu_type[fidx]);
        aui->group_id[fidx] = oapv_bsr_read(bs, 16);
        DUMP_HLS(group_id, aui->group_id[fidx]);
        reserved_zero_8bits = oapv_bsr_read(bs, 8);
        DUMP_HLS(reserved_zero_8bits, reserved_zero_8bits);
        oapv_assert_rv(reserved_zero_8bits == 0, OAPV_ERR_MALFORMED_BITSTREAM);
        ret = oapvd_vlc_frame_info(bs, &aui->frame_info[fidx]);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    }
    reserved_zero_8bits = oapv_bsr_read(bs, 8);
    DUMP_HLS(reserved_zero_8bits, reserved_zero_8bits);
    oapv_assert_rv(reserved_zero_8bits == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);

    /* byte align */
    oapv_bsr_align8(bs);
    return OAPV_OK;
}

int oapvd_vlc_frame_header(oapv_bs_t *bs, oapv_fh_t *fh)
{
    int ret, reserved_zero;
    ret = oapvd_vlc_frame_info(bs, &fh->fi);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    reserved_zero = oapv_bsr_read(bs, 8);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    fh->color_description_present_flag = oapv_bsr_read1(bs);
    DUMP_HLS(fh->color_description_present_flag, fh->color_description_present_flag);
    if(fh->color_description_present_flag) {
        fh->color_primaries = oapv_bsr_read(bs, 8);
        DUMP_HLS(fh->color_primaries, fh->color_primaries);
        fh->transfer_characteristics = oapv_bsr_read(bs, 8);
        DUMP_HLS(fh->transfer_characteristics, fh->transfer_characteristics);
        fh->matrix_coefficients = oapv_bsr_read(bs, 8);
        DUMP_HLS(fh->matrix_coefficients, fh->matrix_coefficients);
        fh->full_range_flag = oapv_bsr_read1(bs);
        DUMP_HLS(fh->full_range_flag, fh->full_range_flag);
    }
    else {
        // default value settings
        fh->color_primaries = 2; // unspecified
        fh->transfer_characteristics = 2; // unspecified
        fh->matrix_coefficients = 2; // unspecified
        fh->full_range_flag = 0; // limited range
    }
    fh->use_q_matrix = oapv_bsr_read1(bs);
    DUMP_HLS(fh->use_q_matrix, fh->use_q_matrix);
    if(fh->use_q_matrix) {
        ret = dec_vlc_q_matrix(bs, fh);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    }

    ret = dec_vlc_tile_info(bs, fh);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    reserved_zero = oapv_bsr_read(bs, 8);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);

    /* byte align */
    oapv_bsr_align8(bs);

    if(fh->use_q_matrix == 0) {
        int num_comp = get_num_comp(fh->fi.chroma_format_idc);
        for(int cidx = 0; cidx < num_comp; cidx++) {
            for(int y = 0; y < OAPV_BLK_H; y++) {
                for(int x = 0; x < OAPV_BLK_W; x++) {
                    fh->q_matrix[cidx][y][x] = 16;
                }
            }
        }
    }

    return OAPV_OK;
}

int oapvd_vlc_tile_size(oapv_bs_t *bs, u32 *tile_size)
{
    u32 size = oapv_bsr_read(bs, 32);
    DUMP_HLS(tile_size, size);
    oapv_assert_rv(size > 0, OAPV_ERR_MALFORMED_BITSTREAM);
    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    *tile_size = size;
    return OAPV_OK;
}

int oapvd_vlc_tile_header(oapv_bs_t *bs, int num_comp, oapv_th_t *th, u32 tile_size, int bit_depth)
{
    u32 read_size = 0;

    th->tile_header_size = oapv_bsr_read(bs, 16);
    DUMP_HLS(th->tile_header_size, th->tile_header_size);

    // check 'tile_header_size' syntax value
    // # of bytes = tile_header_size (2) + tile_index (2) + reserved_zero_8bits (1) + num_comp * (tile_data_size(4) + tile_qp(1))
    oapv_assert_rv(th->tile_header_size == (5 + (num_comp*5)), OAPV_ERR_MALFORMED_BITSTREAM)
    read_size += th->tile_header_size;

    th->tile_index = oapv_bsr_read(bs, 16);
    DUMP_HLS(th->tile_index, th->tile_index);
    for(int c = 0; c < num_comp; c++) {
        th->tile_data_size[c] = oapv_bsr_read(bs, 32);
        DUMP_HLS(th->tile_data_size, th->tile_data_size[c]);
        int ret = oapv_validate_payload_bounds(tile_size, read_size, th->tile_data_size[c]);
        oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
        read_size += th->tile_data_size[c];
    }
    for(int c = 0; c < num_comp; c++) {
        th->tile_qp[c] = oapv_bsr_read(bs, 8);
        oapv_assert_rv(th->tile_qp[c] >= MIN_QUANT && th->tile_qp[c] <= MAX_QUANT(bit_depth),
                       OAPV_ERR_MALFORMED_BITSTREAM);
        DUMP_HLS(th->tile_qp, th->tile_qp[c]);
    }
    th->reserved_zero_8bits = oapv_bsr_read(bs, 8);
    DUMP_HLS(th->reserved_zero_8bits, th->reserved_zero_8bits);
    oapv_assert_rv(th->reserved_zero_8bits == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    oapv_assert_rv(!BSR_IS_UNEXPECTED_EOB(bs), OAPV_ERR_MALFORMED_BITSTREAM);

    /* byte align */
    oapv_bsr_align8(bs);
    return OAPV_OK;
}

int oapvd_vlc_tile_dummy_data(oapv_bs_t *bs)
{
    while(BSR_GET_LEFT_BYTE(bs) > 0) {
        oapv_bsr_read(bs, 8);
        if(BSR_IS_UNEXPECTED_EOB(bs))
            return OAPV_ERR_MALFORMED_BITSTREAM;
    }
    return OAPV_OK;
}

int oapvd_vlc_metadata(oapv_bs_t *bs, u32 pbu_size, oapvm_t mid, int group_id)
{
    int ret;
    u32 t0;
    u32 metadata_size;
    metadata_size = oapv_bsr_read(bs, 32);
    DUMP_HLS(metadata_size, metadata_size);
    oapv_assert_gv(pbu_size >= 8 && metadata_size <= (pbu_size - 8), ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
    u8 *bs_start_pos = oapv_bsr_sink(bs);
    u8 *payload_data = NULL;

    while(metadata_size > 0) {
        u32 payload_type = 0, payload_size = 0;
        t0 = 0;
        do {
            t0 = oapv_bsr_read(bs, 8);
            oapv_assert_gv(!BSR_IS_UNEXPECTED_EOB(bs), ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
            DUMP_HLS(payload_type, t0);
            oapv_assert_gv(metadata_size > 0, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
            metadata_size -= 1;
            if(t0 == 0xFF) {
                payload_type += 255;
            }
        } while(t0 == 0xFF);
        payload_type += t0;

        t0 = 0;
        do {
            t0 = oapv_bsr_read(bs, 8);
            oapv_assert_gv(!BSR_IS_UNEXPECTED_EOB(bs), ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
            DUMP_HLS(payload_size, t0);
            oapv_assert_gv(metadata_size > 0, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
            metadata_size -= 1;
            if(t0 == 0xFF) {
                payload_size += 255;
            }
        } while(t0 == 0xFF);
        payload_size += t0;
        oapv_assert_gv(payload_size <= metadata_size, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);

        oapv_assert_gv(BSR_GET_LEFT_BYTE(bs) >= payload_size, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
        payload_data = oapv_bsr_sink(bs);
#if ENC_DEC_DUMP
        for(int i = 0; i < payload_size; i++) {
            DUMP_HLS(payload_data, payload_data[i]);
        }
#endif
        if (payload_size == 0) {
            payload_data = NULL;
        }
        BSR_MOVE_BYTE_ALIGN(bs, payload_size);
        ret = oapvm_set(mid, group_id, payload_type, payload_data, payload_size);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
        metadata_size -= payload_size;
    }
    const u32 target_read_size = (pbu_size - 8);
    int filler_size = target_read_size - ((u8*)oapv_bsr_sink(bs) - bs_start_pos);
    oapv_assert_gv(filler_size >= 0, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
    if(filler_size > 0) {
        ret = oapvd_vlc_filler(bs, filler_size);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
    }
    return OAPV_OK;

ERR:
    return ret;
}

int oapvd_vlc_filler(oapv_bs_t *bs, u32 filler_size)
{
    int val;
    while(filler_size > 0) {
        val = oapv_bsr_read(bs, 8);
        if(BSR_IS_UNEXPECTED_EOB(bs))
            return OAPV_ERR_MALFORMED_BITSTREAM;
        if(val != 0xFF)
            return OAPV_ERR_MALFORMED_BITSTREAM;
        filler_size--;
    }
    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
