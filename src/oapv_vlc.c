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

#define OAPV_FLUSH_SWAP(cur, code, lb) \
    {                                  \
        *cur++ = (code >> 24) & 0xFF;  \
        *cur++ = (code >> 16) & 0xFF;  \
        *cur++ = (code >> 8) & 0xFF;   \
        *cur++ = (code) & 0xFF;        \
        code = 0;                      \
        lb = 32;                       \
    }

#define OAPV_FLUSH(bs)                        \
    {                                         \
        *bs->cur++ = (bs->code >> 24) & 0xFF; \
        *bs->cur++ = (bs->code >> 16) & 0xFF; \
        *bs->cur++ = (bs->code >> 8) & 0xFF;  \
        *bs->cur++ = (bs->code) & 0xFF;       \
        bs->code = 0;                         \
        bs->leftbits = 32;                    \
    }

#define OAPV_READ_FLUSH(bs, byte)       \
    {                                   \
        bs->code = 0;                   \
        bs->code |= *(bs->cur++) << 24; \
        bs->code |= *(bs->cur++) << 16; \
        bs->code |= *(bs->cur++) << 8;  \
        bs->code |= *(bs->cur++);       \
        bs->leftbits = 32;              \
    }

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

static inline void enc_vlc_write(oapv_bs_t *bs, int coef, int k)
{
    const s32 simple_vlc_table[3][2] = { {
                                             1,
                                         },
                                         { 0, 0 },
                                         { 0, 1 } };
    u32       symbol = coef;
    u32       simple_vlc_val = oapv_clip3(0, 2, symbol >> k);
    int       bit_cnt = 0;
    if(bs->is_bin_count) {
        bs->bin_count += coef;
        return;
    }
    if(symbol >= (u32)(1 << k)) {
        symbol -= (1 << k);
        int val = simple_vlc_table[simple_vlc_val][bit_cnt];
        bs->leftbits--;
        bs->code |= ((val & 0x1) << bs->leftbits);
        if(bs->leftbits == 0) {
            OAPV_FLUSH(bs);
        }
        bit_cnt++;
    }
    if(symbol >= (u32)(1 << k) && simple_vlc_val > 0) {
        symbol -= (1 << k);
        int val = simple_vlc_table[simple_vlc_val][bit_cnt];
        bs->leftbits--;
        bs->code |= ((val & 0x1) << bs->leftbits);
        if(bs->leftbits == 0) {
            OAPV_FLUSH(bs);
        }
        bit_cnt++;
    }
    while(symbol >= (u32)(1 << k)) {
        symbol -= (1 << k);
        bs->leftbits--;
        if(bs->leftbits == 0) {
            OAPV_FLUSH(bs);
        }
        if(bit_cnt >= 2) {
            k++;
        }
        bit_cnt++;
    }
    if(bit_cnt < 2) {
        int val = simple_vlc_table[simple_vlc_val][bit_cnt];
        bs->leftbits--;
        bs->code |= ((val & 0x1) << bs->leftbits);
        if(bs->leftbits == 0) {
            OAPV_FLUSH(bs);
        }
    }
    else {
        bs->leftbits--;
        bs->code |= ((1 & 0x1) << bs->leftbits);
        if(bs->leftbits == 0) {
            OAPV_FLUSH(bs);
        }
    }
    if(k > 0) {
        int leftbits;
        leftbits = bs->leftbits;
        symbol <<= (32 - k);
        bs->code |= (symbol >> (32 - leftbits));
        if(k < leftbits) {
            bs->leftbits -= k;
        }
        else {
            bs->leftbits = 0;
            OAPV_FLUSH(bs);
            bs->code = (leftbits < 32 ? symbol << leftbits : 0);
            bs->leftbits = 32 - (k - leftbits);
        }
    }
}

static void inline bsr_skip_code_opt(oapv_bs_t *bs, int size)
{

    if(size == 32) {
        bs->code = 0;
        bs->leftbits = 0;
    }
    else {
        bs->code <<= size;
        bs->leftbits -= size;
    }
}
static int dec_vlc_read_1bit_read(oapv_bs_t *bs, int k)
{
    u32 symbol = 0;
    int t0 = -1;
    int parse_exp_golomb = 1;
    if(bs->leftbits == 0) {
        OAPV_READ_FLUSH(bs, 4);
    }
    t0 = (u32)(bs->code >> 31);
    bs->code <<= 1;
    bs->leftbits -= 1;
    if(t0 == 0) {
        symbol += (1 << k);
        parse_exp_golomb = 0;
    }
    else {
        symbol += (2 << k);
        parse_exp_golomb = 1;
    }
    if(parse_exp_golomb) {
        while(1) {
            if(bs->leftbits == 0) {
                OAPV_READ_FLUSH(bs, 4);
            }
            t0 = (u32)(bs->code >> 31);
            bs->code <<= 1;
            bs->leftbits -= 1;
            if(t0 == 1) {
                break;
            }
            else {
                symbol += (1 << k);
                k++;
            }
        }
    }
    if(k > 0) {
        u32 code = 0;
        if(bs->leftbits < k) {
            code = bs->code >> (32 - k);
            k -= bs->leftbits;
            OAPV_READ_FLUSH(bs, 4);
        }
        code |= bs->code >> (32 - k);
        bsr_skip_code_opt(bs, k);
        symbol += code;
    }
    return symbol;
}

static int dec_vlc_read(oapv_bs_t *bs, int k)
{
    u32 symbol = 0;
    int t0 = -1;
    int parse_exp_golomb = 1;
    if(bs->leftbits == 0) {
        OAPV_READ_FLUSH(bs, 4);
    }
    t0 = (u32)(bs->code >> 31);
    bs->code <<= 1;
    bs->leftbits -= 1;
    if(t0 == 1) {
        parse_exp_golomb = 0;
    }
    else {
        if(bs->leftbits == 0) {
            OAPV_READ_FLUSH(bs, 4);
        }
        t0 = (u32)(bs->code >> 31);
        bs->code <<= 1;
        bs->leftbits -= 1;
        if(t0 == 0) {
            symbol += (1 << k);
            parse_exp_golomb = 0;
        }
        else {
            symbol += (2 << k);
            parse_exp_golomb = 1;
        }
    }
    if(parse_exp_golomb) {
        while(1) {
            if(bs->leftbits == 0) {
                OAPV_READ_FLUSH(bs, 4);
            }
            t0 = (u32)(bs->code >> 31);
            bs->code <<= 1;
            bs->leftbits -= 1;
            if(t0 == 1) {
                break;
            }
            else {
                symbol += (1 << k);
                k++;
            }
        }
    }
    if(k > 0) {
        u32 code = 0;
        if(bs->leftbits < k) {
            code = bs->code >> (32 - k);
            k -= bs->leftbits;
            OAPV_READ_FLUSH(bs, 4);
        }
        code |= bs->code >> (32 - k);
        bsr_skip_code_opt(bs, k);
        symbol += code;
    }
    return symbol;
}

void oapve_set_frame_header(oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    oapve_param_t * param = ctx->param;

    oapv_mset(fh, 0, sizeof(oapv_fh_t));
    fh->fi.profile_idc = param->profile_idc;
    fh->fi.level_idc = param->level_idc;
    fh->fi.band_idc = param->band_idc;
    fh->fi.frame_width = param->w;
    fh->fi.frame_height = param->h;
    fh->fi.chroma_format_idc = ctx->cfi;
    fh->fi.bit_depth = ctx->bit_depth;
    fh->tile_width_in_mbs = param->tile_w_mb;
    fh->tile_height_in_mbs = param->tile_h_mb;

    fh->color_description_present_flag = param->color_description_present_flag;
    fh->color_primaries = param->color_primaries;
    fh->transfer_characteristics = param->transfer_characteristics;
    fh->matrix_coefficients = param->matrix_coefficients;
    fh->full_range_flag = param->full_range_flag;

    fh->use_q_matrix = param->use_q_matrix;
    if(fh->use_q_matrix == 0) {
        for(int cidx = 0; cidx < ctx->num_comp; cidx++) {
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

static int enc_vlc_quantization_matrix(oapv_bs_t *bs, oapve_ctx_t *ctx, oapv_fh_t *fh)
{
    for(int cidx = 0; cidx < ctx->num_comp; cidx++) {
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
    oapv_bsw_write(bs, 0, 8); // reserved_zero_8bits
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

void oapve_set_tile_header(oapve_ctx_t *ctx, oapv_th_t *th, int tile_idx, int qp)
{
    oapv_mset(th, 0, sizeof(oapv_th_t));

    for(int c = 0; c < ctx->num_comp; c++) {
        th->tile_qp[c] = oapv_clip3(MIN_QUANT, MAX_QUANT(10), qp + ctx->qp_offset[c]);
    }
    th->tile_index = tile_idx;

    for(int i = 0; i < N_C; i++) {
        // this setting is required to prevent underflow at dummy writing tile header due to '-1'.
        th->tile_data_size[i] = 1;
    }
}

int oapve_vlc_tile_header(oapve_ctx_t *ctx, oapv_bs_t *bs, oapv_th_t *th)
{
    oapv_assert_rv(bsw_is_align8(bs), OAPV_ERR_MALFORMED_BITSTREAM);
    th->tile_header_size = 5;                    // tile_header_size + tile_index + reserved_zero_8bits
    th->tile_header_size += (ctx->num_comp * 5); // tile_data_size + tile_qp

    oapv_bsw_write(bs, th->tile_header_size, 16);
    DUMP_HLS(th->tile_header_size, th->tile_header_size);
    oapv_bsw_write(bs, th->tile_index, 16);
    DUMP_HLS(th->tile_index, th->tile_index);
    for(int c = 0; c < ctx->num_comp; c++) {
        oapv_bsw_write(bs, th->tile_data_size[c], 32);
        DUMP_HLS(th->tile_data_size, th->tile_data_size[c]);
    }
    for(int c = 0; c < ctx->num_comp; c++) {
        oapv_bsw_write(bs, th->tile_qp[c], 8);
        DUMP_HLS(th->tile_qp, th->tile_qp[c]);
    }
    oapv_bsw_write(bs, th->reserved_zero_8bits, 8);
    DUMP_HLS(th->reserved_zero_8bits, th->reserved_zero_8bits);

    return OAPV_OK;
}

void oapve_vlc_run_length_cc(oapve_ctx_t *ctx, oapve_core_t *core, oapv_bs_t *bs, s16 *coef, int log2_w, int log2_h, int num_sig, int ch_type)
{
    u32        num_coeff, scan_pos;
    u32        sign, level, prev_level, run;
    const u16 *scanp;
    s16        coef_cur;

    scanp = oapv_tbl_scan;
    num_coeff = 1 << (log2_w + log2_h);
    run = 0;
    int first_ac = 1;
    prev_level = core->prev_1st_ac_ctx[ch_type];

    int prev_run = 0;

    int rice_level = 0;
    scan_pos = 0;

    // for DC
    {
        coef_cur = coef[scanp[scan_pos]];
        level = oapv_abs16(coef_cur);
        sign = (coef_cur > 0) ? 0 : 1;

        rice_level = oapv_clip3(OAPV_MIN_DC_LEVEL_CTX, OAPV_MAX_DC_LEVEL_CTX, core->prev_dc_ctx[ch_type] >> 1);

        enc_vlc_write(bs, level, rice_level);

        if(level)
            oapv_bsw_write1(bs, sign);

        core->prev_dc_ctx[ch_type] = level;
    }

    for(scan_pos = 1; scan_pos < num_coeff; scan_pos++) {
        coef_cur = coef[scanp[scan_pos]];

        if(coef_cur) {
            level = oapv_abs16(coef_cur);
            sign = (coef_cur > 0) ? 0 : 1;

            /* Run coding */
            int rice_run = 0;
            rice_run = prev_run / 4;
            if(rice_run > 2)
                rice_run = 2;
            enc_vlc_write(bs, run, rice_run);

            /* Level coding */
            rice_level = oapv_clip3(OAPV_MIN_AC_LEVEL_CTX, OAPV_MAX_AC_LEVEL_CTX, prev_level >> 2);
            enc_vlc_write(bs, level - 1, rice_level);

            /* Sign coding */
            oapv_bsw_write1(bs, sign);

            if(first_ac) {
                first_ac = 0;
                core->prev_1st_ac_ctx[ch_type] = level;
            }

            if(scan_pos == num_coeff - 1) {
                break;
            }
            prev_run = run;
            run = 0;

            {
                prev_level = level;
            }

            num_sig--;
        }
        else {
            run++;
        }
    }

    if(coef[scanp[num_coeff - 1]] == 0) {
        int rice_run = 0;
        rice_run = prev_run / 4;
        if(rice_run > 2)
            rice_run = 2;
        enc_vlc_write(bs, run, rice_run);
    }
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
    oapv_bsw_write(bs, pbu_type, 8);
    DUMP_HLS(pbu_type, pbu_type);
    oapv_bsw_write(bs, group_id, 16);
    DUMP_HLS(group_id, group_id);
    oapv_bsw_write(bs, 0, 8); // reserved_zero_8bit
    DUMP_HLS(reserved_zero, 0);

    return OAPV_OK;
}

/****** ENABLE_DECODER ******/
int oapve_vlc_metadata(oapv_md_t *md, oapv_bs_t *bs)
{
    oapv_bsw_write(bs, md->md_size, 32);
    DUMP_HLS(metadata_size, md->md_size);
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
    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
int oapvd_vlc_au_size(oapv_bs_t *bs, u32 *au_size)
{
    u32 size;
    size = oapv_bsr_read(bs, 32);
    oapv_assert_rv(size > 0 && size < 0xFFFFFFFF, OAPV_ERR_MALFORMED_BITSTREAM);
    *au_size = size;
    return OAPV_OK;
}

int oapvd_vlc_pbu_size(oapv_bs_t *bs, u32 *pbu_size)
{
    u32 size;
    size = oapv_bsr_read(bs, 32);
    DUMP_HLS(pbu_size, size);
    oapv_assert_rv(size > 0 && size < 0xFFFFFFFF, OAPV_ERR_MALFORMED_BITSTREAM);
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

    reserved_zero = oapv_bsr_read(bs, 8);
    DUMP_HLS(reserved_zero, reserved_zero);
    oapv_assert_rv(reserved_zero == 0, OAPV_ERR_MALFORMED_BITSTREAM);

    // check frame width in case of 422 format.
    if(fi->chroma_format_idc == 2) {
        // frame_width should be multiple of 2
        oapv_assert_rv((fi->frame_width & 0x1) == 0, OAPV_ERR_MALFORMED_BITSTREAM);
    }

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
    /* byte align */
    oapv_bsr_align8(bs);
    return OAPV_OK;
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

    oapv_assert_rv(tile_cols <= OAPV_MAX_TILE_COLS && tile_rows <= OAPV_MAX_TILE_ROWS, OAPV_ERR_MALFORMED_BITSTREAM)

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
    *tile_size = size;
    return OAPV_OK;
}

int oapvd_vlc_tile_header(oapv_bs_t *bs, oapvd_ctx_t *ctx, oapv_th_t *th)
{
    th->tile_header_size = oapv_bsr_read(bs, 16);
    DUMP_HLS(th->tile_header_size, th->tile_header_size);
    th->tile_index = oapv_bsr_read(bs, 16);
    DUMP_HLS(th->tile_index, th->tile_index);
    for(int c = 0; c < ctx->num_comp; c++) {
        th->tile_data_size[c] = oapv_bsr_read(bs, 32);
        DUMP_HLS(th->tile_data_size, th->tile_data_size[c]);
        oapv_assert_rv(th->tile_data_size[c] > 0, OAPV_ERR_MALFORMED_BITSTREAM);
    }
    for(int c = 0; c < ctx->num_comp; c++) {
        th->tile_qp[c] = oapv_bsr_read(bs, 8);
        DUMP_HLS(th->tile_qp, th->tile_qp[c]);
    }
    th->reserved_zero_8bits = oapv_bsr_read(bs, 8);
    DUMP_HLS(th->reserved_zero_8bits, th->reserved_zero_8bits);
    /* byte align */
    oapv_bsr_align8(bs);

    oapv_assert_rv(th->reserved_zero_8bits == 0, OAPV_ERR_MALFORMED_BITSTREAM);
    return OAPV_OK;
}

int oapve_vlc_dc_coeff(oapve_ctx_t *ctx, oapve_core_t *core, oapv_bs_t *bs, int dc_diff, int c)
{
    int rice_level = 0;
    int abs_dc_diff = oapv_abs32(dc_diff);
    int sign_dc_diff = (dc_diff > 0) ? 0 : 1;

    rice_level = oapv_clip3(OAPV_MIN_DC_LEVEL_CTX, OAPV_MAX_DC_LEVEL_CTX, core->prev_dc_ctx[c] >> 1);
    enc_vlc_write(bs, abs_dc_diff, rice_level);

    if(abs_dc_diff)
        oapv_bsw_write1(bs, sign_dc_diff);

    core->prev_dc_ctx[c] = abs_dc_diff;
    return OAPV_OK;
}
void oapve_vlc_ac_coeff(oapve_ctx_t *ctx, oapve_core_t *core, oapv_bs_t *bs, s16 *coef, int num_sig, int ch_type)
{
    ALIGNED_16(s16 coef_temp[64]);
    u32        num_coeff, scan_pos;
    u32        sign, level, prev_level, run;
    const u16 *scanp;
    s16        coef_cur;

    scanp = oapv_tbl_scan;
    num_coeff = OAPV_BLK_D;
    run = 0;
    int first_ac = 1;
    prev_level = core->prev_1st_ac_ctx[ch_type];
    int prev_run = 0;
    int rice_run = 0;
    int rice_level = 0;
    int lb = bs->leftbits;
    u32 code = bs->code;
    u8 *cur = bs->cur;

    for(scan_pos = 1; scan_pos < num_coeff; scan_pos++) {
        coef_temp[scan_pos] = coef[scanp[scan_pos]];
    }

    const s32 simple_vlc_table[3][2] = { {
                                             1,
                                         },
                                         { 0, 0 },
                                         { 0, 1 } };
    for(scan_pos = 1; scan_pos < num_coeff - 1; scan_pos++) {
        coef_cur = coef_temp[scan_pos];
        if(coef_cur) {
            level = oapv_abs16(coef_cur);
            sign = (coef_cur > 0) ? 0 : 1;
            rice_run = prev_run >> 2;
            if(rice_run > 2)
                rice_run = 2;
            if(run == 0 && rice_run == 0) {
                lb--; // bs->leftbits--;
                code |= (1 << lb);
                if(lb == 0) {
                    OAPV_FLUSH_SWAP(cur, code, lb);
                }
            }
            else {
                int leftbits;
                leftbits = lb;
                u32 code_from_lut = CODE_LUT_100[run][rice_run][0];
                int len_from_lut = CODE_LUT_100[run][rice_run][1];
                code |= (code_from_lut >> (32 - leftbits));
                if(len_from_lut < leftbits) {
                    lb -= len_from_lut;
                }
                else {
                    lb = 0;
                    OAPV_FLUSH_SWAP(cur, code, lb);
                    code = (leftbits < 32 ? code_from_lut << leftbits : 0);
                    lb = 32 - (len_from_lut - leftbits);
                }
            }
            rice_level = prev_level >> 2;
            if(rice_level > 4)
                rice_level = OAPV_MAX_AC_LEVEL_CTX;
            if(level - 1 == 0 && rice_level == 0) {
                lb--;
                code |= (1 << lb);
                if(lb == 0) {
                    OAPV_FLUSH_SWAP(cur, code, lb);
                }
            }
            else {
                if(level - 1 > 98) {
                    {
                        int k = rice_level;
                        u32 symbol = level - 1;
                        u32 simple_vlc_val = oapv_clip3(0, 2, symbol >> k);
                        int bit_cnt = 0;
                        if(symbol >= (u32)(1 << k)) {
                            symbol -= (1 << k);
                            int val = simple_vlc_table[simple_vlc_val][bit_cnt];
                            lb--;
                            code |= ((val & 0x1) << lb);
                            if(lb == 0) {
                                OAPV_FLUSH_SWAP(cur, code, lb);
                            }
                            bit_cnt++;
                        }
                        if(symbol >= (u32)(1 << k) && simple_vlc_val > 0) {
                            symbol -= (1 << k);
                            int val = simple_vlc_table[simple_vlc_val][bit_cnt];
                            lb--;
                            code |= ((val & 0x1) << lb);
                            if(lb == 0) {
                                OAPV_FLUSH_SWAP(cur, code, lb);
                            }
                            bit_cnt++;
                        }
                        while(symbol >= (u32)(1 << k)) {
                            symbol -= (1 << k);
                            lb--;
                            if(lb == 0) {
                                OAPV_FLUSH_SWAP(cur, code, lb);
                            }
                            if(bit_cnt >= 2) {
                                k++;
                            }
                            bit_cnt++;
                        }
                        if(bit_cnt < 2) {
                            int val = simple_vlc_table[simple_vlc_val][bit_cnt];
                            lb--;
                            code |= ((val & 0x1) << lb);
                            if(lb == 0) {
                                OAPV_FLUSH_SWAP(cur, code, lb);
                            }
                        }
                        else {
                            lb--;
                            code |= ((1 & 0x1) << lb);
                            if(lb == 0) {
                                OAPV_FLUSH_SWAP(cur, code, lb);
                            }
                        }
                        if(k > 0) {
                            int leftbits;
                            leftbits = lb;
                            symbol <<= (32 - k);
                            code |= (symbol >> (32 - leftbits));
                            if(k < leftbits) {
                                lb -= k;
                            }
                            else {
                                lb = 0;
                                OAPV_FLUSH_SWAP(cur, code, lb);
                                code = (leftbits < 32 ? symbol << leftbits : 0);
                                lb = 32 - (k - leftbits);
                            }
                        }
                    }
                }
                else {
                    int leftbits;
                    leftbits = lb;
                    u32 code_from_lut = CODE_LUT_100[level - 1][rice_level][0];
                    int len_from_lut = CODE_LUT_100[level - 1][rice_level][1];
                    code |= (code_from_lut >> (32 - leftbits));
                    if(len_from_lut < leftbits) {
                        lb -= len_from_lut;
                    }
                    else {
                        lb = 0;
                        OAPV_FLUSH_SWAP(cur, code, lb);
                        code = (leftbits < 32 ? code_from_lut << leftbits : 0);
                        lb = 32 - (len_from_lut - leftbits);
                    }
                }
            }
            {
                lb--;
                code |= ((sign & 0x1) << lb);
                if(lb == 0) {
                    OAPV_FLUSH_SWAP(cur, code, lb);
                }
            }
            if(first_ac) {
                first_ac = 0;
                core->prev_1st_ac_ctx[ch_type] = level;
            }
            prev_run = run;
            run = 0;
            prev_level = level;
        }
        else {
            run++;
        }
    }
    bs->cur = cur;
    bs->code = code;
    bs->leftbits = lb;
    coef_cur = coef_temp[scan_pos];
    if(coef_cur) {
        level = oapv_abs16(coef_cur);
        sign = (coef_cur > 0) ? 0 : 1;
        /* Run coding */
        rice_run = prev_run >> 2;
        if(rice_run > 2)
            rice_run = 2;
        if(run == 0 && rice_run == 0) {
            bs->leftbits--;
            bs->code |= (1 << bs->leftbits);
            if(bs->leftbits == 0) {
                OAPV_FLUSH(bs);
            }
        }
        else {
            int leftbits;
            leftbits = bs->leftbits;
            u32 code_from_lut = CODE_LUT_100[run][rice_run][0];
            int len_from_lut = CODE_LUT_100[run][rice_run][1];
            bs->code |= (code_from_lut >> (32 - leftbits));
            if(len_from_lut < leftbits) {
                bs->leftbits -= len_from_lut;
            }
            else {
                bs->leftbits = 0;
                OAPV_FLUSH(bs);
                bs->code = (leftbits < 32 ? code_from_lut << leftbits : 0);
                bs->leftbits = 32 - (len_from_lut - leftbits);
            }
        }
        /* Level coding */
        rice_level = prev_level >> 2;
        if(rice_level > 4)
            rice_level = OAPV_MAX_AC_LEVEL_CTX;
        if(level - 1 == 0 && rice_level == 0) {
            bs->leftbits--;
            bs->code |= (1 << bs->leftbits);
            if(bs->leftbits == 0) {
                OAPV_FLUSH(bs);
            }
        }
        else {
            if(level - 1 > 98) {
                enc_vlc_write(bs, level - 1, rice_level);
            }
            else {
                int leftbits;
                leftbits = bs->leftbits;
                u32 code_from_lut = CODE_LUT_100[level - 1][rice_level][0];
                int len_from_lut = CODE_LUT_100[level - 1][rice_level][1];
                bs->code |= (code_from_lut >> (32 - leftbits));
                if(len_from_lut < leftbits) {
                    bs->leftbits -= len_from_lut;
                }
                else {
                    bs->leftbits = 0;
                    OAPV_FLUSH(bs);
                    bs->code = (leftbits < 32 ? code_from_lut << leftbits : 0);
                    bs->leftbits = 32 - (len_from_lut - leftbits);
                }
            }
        }
        /* Sign coding */
        {
            bs->leftbits--;
            bs->code |= ((sign & 0x1) << bs->leftbits);
            if(bs->leftbits == 0) {
                OAPV_FLUSH(bs);
            }
        }
        if(first_ac) {
            first_ac = 0;
            core->prev_1st_ac_ctx[ch_type] = level;
        }
    }
    else {
        run++;
    }
    if(coef_temp[num_coeff - 1] == 0) {
        int rice_run = 0;
        rice_run = prev_run >> 2;
        if(rice_run > 2)
            rice_run = 2;

        if(run == 0 && rice_run == 0) {
            bs->leftbits--;
            bs->code |= (1 << bs->leftbits);
            if(bs->leftbits == 0) {
                OAPV_FLUSH(bs);
            }
        }
        else {
            int leftbits;
            leftbits = bs->leftbits;
            u32 code_from_lut = CODE_LUT_100[run][rice_run][0];
            int len_from_lut = CODE_LUT_100[run][rice_run][1];
            bs->code |= (code_from_lut >> (32 - leftbits));
            if(len_from_lut < leftbits) {
                bs->leftbits -= len_from_lut;
            }
            else {
                bs->leftbits = 0;
                OAPV_FLUSH(bs);
                bs->code = (leftbits < 32 ? code_from_lut << leftbits : 0);
                bs->leftbits = 32 - (len_from_lut - leftbits);
            }
        }
    }
}

int oapvd_vlc_dc_coeff(oapvd_ctx_t *ctx, oapvd_core_t *core, oapv_bs_t *bs, int *dc_diff, int c)
{
    int rice_level = 0;
    int abs_dc_diff;
    int sign_dc_diff = 0;

    rice_level = oapv_clip3(OAPV_MIN_DC_LEVEL_CTX, OAPV_MAX_DC_LEVEL_CTX, core->prev_dc_ctx[c] >> 1);
    abs_dc_diff = dec_vlc_read(bs, rice_level);
    if(abs_dc_diff)
        sign_dc_diff = oapv_bsr_read1(bs);

    *dc_diff = sign_dc_diff ? -abs_dc_diff : abs_dc_diff;
    core->prev_dc_ctx[c] = abs_dc_diff;

    return OAPV_OK;
}

int oapvd_vlc_ac_coeff(oapvd_ctx_t *ctx, oapvd_core_t *core, oapv_bs_t *bs, s16 *coef, int c)
{
    int        sign, level, prev_level, run;
    int        scan_pos_offset, num_coeff, i;
    const u16 *scanp;

    scanp = oapv_tbl_scan;
    num_coeff = OAPV_BLK_D;
    scan_pos_offset = 1;
    run = 0;

    int first_ac = 1;
    prev_level = core->prev_1st_ac_ctx[c];
    int prev_run = 0;

    do {
        int rice_run = 0;
        rice_run = prev_run / 4;
        if(rice_run > 2)
            rice_run = 2;
        if(rice_run == 0) {
            if(bs->leftbits == 0) {
                OAPV_READ_FLUSH(bs, 4);
            }
            u32 t0 = (u32)(bs->code >> 31);
            bs->code <<= 1;
            bs->leftbits -= 1;
            if(t0)
                run = 0;
            else
                run = dec_vlc_read_1bit_read(bs, rice_run);
        }
        else {
            run = dec_vlc_read(bs, rice_run);
        }

        oapv_assert_rv((scan_pos_offset + run) <= 64, OAPV_ERR_MALFORMED_BITSTREAM);
        for (i = scan_pos_offset; i < scan_pos_offset + run; i++){
            coef[scanp[i]] = 0;
        }

        if(scan_pos_offset + run == num_coeff) {
            break;
        }

        scan_pos_offset += run;

        /* Level parsing */
        int rice_level = 0;
        if(scan_pos_offset == 0) {
            rice_level = oapv_clip3(OAPV_MIN_DC_LEVEL_CTX, OAPV_MAX_DC_LEVEL_CTX, core->prev_dc_ctx[c] >> 1);
        }
        else {
            rice_level = oapv_clip3(OAPV_MIN_AC_LEVEL_CTX, OAPV_MAX_AC_LEVEL_CTX, prev_level >> 2);
        }

        if(rice_level == 0) {
            if(bs->leftbits == 0) {
                OAPV_READ_FLUSH(bs, 4);
            }
            u32 t0 = (u32)(bs->code >> 31);
            bs->code <<= 1;
            bs->leftbits -= 1;
            if(t0)
                level = 0;
            else
                level = dec_vlc_read_1bit_read(bs, rice_level);
        }
        else {
            level = dec_vlc_read(bs, rice_level);
        }
        level++;

        if(scan_pos_offset != 0) {
            prev_level = level;
        }
        prev_run = run;

        if(scan_pos_offset == 0) {
            core->prev_dc_ctx[c] = level;
        }
        else if(first_ac) {
            first_ac = 0;
            core->prev_1st_ac_ctx[c] = level;
        }

        /* Sign parsing */
        if(bs->leftbits == 0) {
            OAPV_READ_FLUSH(bs, 4);
        }
        sign = (u32)(bs->code >> 31);
        bs->code <<= 1;
        bs->leftbits -= 1;
        coef[scanp[scan_pos_offset]] = sign ? -(s16)level : (s16)level;

        if(scan_pos_offset >= num_coeff - 1) {
            break;
        }
        scan_pos_offset++;
    } while(1);

    return OAPV_OK;
}

int oapvd_vlc_tile_dummy_data(oapv_bs_t *bs)
{
    while(bs->cur <= bs->end) {
        oapv_bsr_read(bs, 8);
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
    u8 *bs_start_pos = bs->cur;
    u8 *payload_data = NULL;

    while(metadata_size > 0) {
        u32 payload_type = 0, payload_size = 0;
        t0 = 0;
        do {
            t0 = oapv_bsr_read(bs, 8);
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
            DUMP_HLS(payload_size, t0);
            oapv_assert_gv(metadata_size > 0, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
            metadata_size -= 1;
            if(t0 == 0xFF) {
                payload_size += 255;
            }
        } while(t0 == 0xFF);
        payload_size += t0;
        oapv_assert_gv(payload_size <= metadata_size, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);

        if(payload_size > 0) {

            payload_data = oapv_malloc(payload_size);
            oapv_assert_gv(payload_data != NULL, ret, OAPV_ERR_OUT_OF_MEMORY, ERR);
            if(payload_type == OAPV_METADATA_FILLER) {
                for(u32 i = 0; i < payload_size; i++) {
                    t0 = oapv_bsr_read(bs, 8);
                    DUMP_HLS(payload_data, t0);
                    oapv_assert_gv(t0 == 0xFF, ret, OAPV_ERR_MALFORMED_BITSTREAM, ERR);
                    payload_data[i] = 0xFF;
                }
            }
            else {
                for(u32 i = 0; i < payload_size; i++) {
                    t0 = oapv_bsr_read(bs, 8);
                    DUMP_HLS(payload_data, t0);
                    payload_data[i] = t0;
                }
            }
        }
        ret = oapvm_set(mid, group_id, payload_type, payload_data, payload_size,
                        payload_type == OAPV_METADATA_USER_DEFINED ? payload_data : NULL);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
        metadata_size -= payload_size;
    }
    const u32 target_read_size = (pbu_size - 8);
    ret = oapvd_vlc_filler(bs, target_read_size - (bs->cur - bs_start_pos));
    oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
    return OAPV_OK;

ERR:
    // TO-DO: free memory
    return ret;
}

int oapvd_vlc_filler(oapv_bs_t *bs, u32 filler_size)
{
    int val;
    while(filler_size > 0) {
        val = oapv_bsr_read(bs, 8);
        if(val != 0xFF) {
            return OAPV_ERR_MALFORMED_BITSTREAM;
        }
        filler_size--;
    }
    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
