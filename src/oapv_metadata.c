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

static oapvm_ctx_t *meta_id_to_ctx(oapvm_t id)
{
    oapvm_ctx_t *ctx;
    oapv_assert_rv(id, NULL);
    ctx = (oapvm_ctx_t *)id;
    oapv_assert_rv(ctx->magic == OAPVM_MAGIC_CODE, NULL);
    return ctx;
}
#define div_255_fast(x)  (((x) + (((x) + 257) >> 8)) >> 8)

static inline u32 meta_get_byte_pld_type(oapv_mdp_t *mdp)
{
    return (mdp->pld_type < 65536 ? div_255_fast(mdp->pld_type) : mdp->pld_type / 255) + 1;
}

static inline u32 meta_get_byte_pld_size(oapv_mdp_t *mdp)
{
    return (mdp->pld_size < 65536 ? div_255_fast(mdp->pld_size) : mdp->pld_size / 255) + 1;
}

static inline u32 meta_get_byte_pld_all(oapv_mdp_t *mdp)
{
    return meta_get_byte_pld_type(mdp) + meta_get_byte_pld_size(mdp) + mdp->pld_size;
}

static oapv_mdp_t *meta_mdp_find_ud(oapv_md_t *md, unsigned char *uuid, oapv_mdp_t **prev_mdp)
{
    oapv_mdp_t *mdp = md->md_payload;
    *prev_mdp = NULL;
    while(mdp != NULL) {
        if(mdp->pld_type == OAPV_METADATA_USER_DEFINED) {
            oapv_md_usd_t *usd = (oapv_md_usd_t *)mdp->pld_data;
            if(oapv_mcmp(uuid, usd->uuid, 16) == 0) {
                return mdp;
            }
        }
        *prev_mdp = mdp;
        mdp = mdp->next;
    }

    return NULL;
}

static oapv_mdp_t *meta_mdp_find_non_ud(oapv_md_t *md, int mdt, oapv_mdp_t **prev_mdp)
{
    oapv_mdp_t *mdp = md->md_payload;
    *prev_mdp = NULL;
    while(mdp != NULL) {
        if(mdp->pld_type == mdt) {
            break;
        }
        *prev_mdp = mdp;
        mdp = mdp->next;
    }

    return mdp;
}
static oapv_mdp_t *meta_find_mdp(oapv_md_t *md, int type, unsigned char *uuid)
{
    oapv_mdp_t *prev_mdp;
    return (type == OAPV_METADATA_USER_DEFINED) ? meta_mdp_find_ud(md, uuid, &prev_mdp) : meta_mdp_find_non_ud(md, type, &prev_mdp);
}

static oapv_mdp_t *meta_md_find_mdp_with_prev(oapv_md_t *md, int type, unsigned char *uuid, oapv_mdp_t **prev_mdp)
{
    return (type == OAPV_METADATA_USER_DEFINED) ? meta_mdp_find_ud(md, uuid, prev_mdp) : meta_mdp_find_non_ud(md, type, prev_mdp);
}

static int meta_rm_mdp(oapv_md_t *md, int mdt, unsigned char *uuid)
{
    oapv_mdp_t *mdp, *mdp_prev;
    mdp = meta_md_find_mdp_with_prev(md, mdt, uuid, &mdp_prev);
    oapv_assert_rv(mdp != NULL, OAPV_ERR_NOT_FOUND);
    if(mdp_prev == NULL) {
        md->md_payload = mdp->next;
    }
    else {
        mdp_prev->next = mdp->next;
    }

    oapv_mfree(mdp->pld_data);
    oapv_mfree(mdp);
    md->mdp_num--;
    return OAPV_OK;
}

static oapv_md_t *meta_find_md(oapvm_ctx_t *ctx, int group_id)
{
    for(int n = 0; n < ctx->num; n++) {
        if(group_id == ctx->md_arr[n].group_id) {
            return &ctx->md_arr[n];
        }
    }
    return NULL;
}


static int meta_verify_mdp_data(int type, int size, u8 *data)
{
    if(type == OAPV_METADATA_ITU_T_T35) {
        if(size == 0) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        if(*data == 0xFF) {
            if(size == 1) {
                return OAPV_ERR_INVALID_ARGUMENT;
            }
        }
    }
    else if(type == OAPV_METADATA_MDCV) {
        if(size != 24) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
    }
    else if(type == OAPV_METADATA_CLL) {
        if(size != 4) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
    }
    else if(type == OAPV_METADATA_USER_DEFINED) {
        if(size < 16) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
    }
    else if(type == OAPV_METADATA_FILLER) {
        for(int i = 0; i < size; i++) {
            if(data[i] != 0xFF) {
                return OAPV_ERR_INVALID_ARGUMENT;
            }
        }
    }
    else {
        return OAPV_OK; // undefined payload type
    }
    return OAPV_OK;
}

static void meta_free_md(oapv_md_t *md)
{
    oapv_mdp_t *mdp = md->md_payload;
    while(mdp != NULL) {
        oapv_mfree(mdp->pld_data);
        oapv_mdp_t *mdp_t = mdp;
        mdp = mdp->next;
        oapv_mfree(mdp_t);
    }
}

int oapvm_write_mdcv(oapvm_payload_mdcv_t *mdcv, void *data, int *size)
{
    int i, t;
    u32 tu32;
    oapv_bs_t bs;
    oapv_bsw_init(&bs, data, 24, NULL); // MDCV payload has 24 bytes

    for(i = 0; i < 3; i++) {
        t = mdcv->primary_chromaticity_x[i];
        oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
        oapv_bsw_write(&bs, t, 16);

        t = mdcv->primary_chromaticity_y[i];
        oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
        oapv_bsw_write(&bs, t, 16);
    }
    t = mdcv->white_point_chromaticity_x;
    oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsw_write(&bs, t, 16);

    t = mdcv->white_point_chromaticity_y;
    oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsw_write(&bs, t, 16);

    oapv_assert_rv(mdcv->max_mastering_luminance <= 0xFFFFFFFF, OAPV_ERR_INVALID_ARGUMENT);
    tu32 = mdcv->max_mastering_luminance;
    oapv_bsw_write(&bs, tu32, 32);

    oapv_assert_rv(mdcv->min_mastering_luminance <= 0xFFFFFFFF, OAPV_ERR_INVALID_ARGUMENT);
    tu32 = mdcv->min_mastering_luminance;
    oapv_bsw_write(&bs, tu32, 32);

    oapv_bsw_deinit(&bs);

    *size = 24; // MDCV payload has 24 bytes
    return OAPV_OK;
}

int oapvm_read_mdcv(void *data, int size, oapvm_payload_mdcv_t *mdcv)
{
    int i;
    oapv_bs_t bs;
    oapv_assert_rv(size >= 24, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsr_init(&bs, data, size, NULL); // MDCV payload has 24 bytes

    for(i = 0; i < 3; i++) {
        mdcv->primary_chromaticity_x[i] = oapv_bsr_read(&bs, 16);
    }
    for(i = 0; i < 3; i++) {
        mdcv->primary_chromaticity_y[i] = oapv_bsr_read(&bs, 16);
    }
    mdcv->white_point_chromaticity_x = oapv_bsr_read(&bs, 16);
    mdcv->white_point_chromaticity_y = oapv_bsr_read(&bs, 16);

    mdcv->max_mastering_luminance = oapv_bsr_read(&bs, 32);
    mdcv->min_mastering_luminance = oapv_bsr_read(&bs, 32);
    return OAPV_OK;
}

int oapvm_write_cll(oapvm_payload_cll_t *cll, void *data, int *size)
{
    int t;
    oapv_bs_t bs;
    oapv_bsw_init(&bs, data, 4, NULL); // CLL payload has 4 bytes

    t = cll->max_cll;
    oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsw_write(&bs, t, 16);

    t = cll->max_fall;
    oapv_assert_rv(t >=0 && t <= 0xFFFF, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsw_write(&bs, t, 16);

    oapv_bsw_deinit(&bs);

    *size = 4; // CLL payload has 4 bytes
    return OAPV_OK;
}

int oapvm_read_cll(void *data, int size, oapvm_payload_cll_t *cll)
{
    oapv_bs_t bs;
    oapv_assert_rv(size >= 4, OAPV_ERR_INVALID_ARGUMENT);
    oapv_bsr_init(&bs, data, size, NULL); // CLL payload has 4 bytes

    cll->max_cll = oapv_bsr_read(&bs, 16);
    cll->max_fall = oapv_bsr_read(&bs, 16);
    return OAPV_OK;
}

int oapvm_set(oapvm_t mid, int group_id, int type, void *data, int size)
{
    void        *pld_data_new = NULL;
    oapv_mdp_t  *mdp_new = NULL;
    int          ret = OAPV_OK;
    u8          *uuid = NULL;

    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    oapv_assert_rv((data != NULL && size > 0) || (data == NULL && size == 0), OAPV_ERR_INVALID_ARGUMENT);

    ret = meta_verify_mdp_data(type, size, (u8 *)data);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);

    if(type == OAPV_METADATA_USER_DEFINED) {
        uuid = (u8 *)data;
    }

    if(size > 0) {
        pld_data_new = oapv_malloc(size);
        oapv_assert_rv(pld_data_new != NULL, OAPV_ERR_OUT_OF_MEMORY);
        oapv_mcpy(pld_data_new, data, size);
    }
    else {
        pld_data_new = NULL;
    }

    oapv_md_t *md = meta_find_md(ctx, group_id);
    if(md == NULL) {
        oapv_assert_gv(ctx->num < OAPV_MAX_NUM_METAS, ret, OAPV_ERR_REACHED_MAX, ERR);
        md = &ctx->md_arr[ctx->num];
        md->group_id = group_id;
        md->mdp_num = 0;
        md->md_payload = NULL;
        ctx->num++;
    }

    oapv_mdp_t *mdp_t = meta_find_mdp(md, type, uuid);
    if(mdp_t == NULL) { // add new one
        mdp_new = oapv_malloc(sizeof(oapv_mdp_t));
        oapv_assert_gv(mdp_new != NULL, ret, OAPV_ERR_OUT_OF_MEMORY, ERR);
        mdp_new->pld_size = size;
        mdp_new->pld_type = type;
        mdp_new->pld_data = pld_data_new;
        mdp_new->next = md->md_payload; // add to head

        md->md_payload = mdp_new;
        md->mdp_num++;
    }
    else { // replace the exist one
        mdp_t->pld_size = size;
        oapv_mfree(mdp_t->pld_data);
        mdp_t->pld_data = pld_data_new;
    }
    return OAPV_OK;

ERR:
    if(mdp_new)
        oapv_mfree(mdp_new);
    if(pld_data_new)
        oapv_mfree(pld_data_new);
    return ret;
}

int oapvm_get(oapvm_t mid, int group_id, int type, void **data, int *size, unsigned char *uuid)
{
    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    oapv_md_t   *md = meta_find_md(ctx, group_id);
    oapv_assert_g(md != NULL, ERR);
    oapv_mdp_t *mdp = meta_find_mdp(md, type, uuid);
    oapv_assert_g(mdp != NULL, ERR);

    *data = mdp->pld_data;
    *size = mdp->pld_size;
    return OAPV_OK;

ERR:
    return OAPV_ERR_NOT_FOUND;
}

int oapvm_rem(oapvm_t mid, int group_id, int type, unsigned char *uuid)
{
    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    oapv_md_t   *md = meta_find_md(ctx, group_id);
    oapv_assert_g(md != NULL, ERR);
    return meta_rm_mdp(md, type, uuid);

ERR:
    return OAPV_ERR_NOT_FOUND;

}

int oapvm_set_all(oapvm_t mid, oapvm_payload_t *pld, int num_plds)
{
    int          ret;
    for(int i = 0; i < num_plds; i++) {
        ret = oapvm_set(mid, pld[i].group_id, pld[i].type, pld[i].data, pld[i].size);
        oapv_assert_g(OAPV_SUCCEEDED(ret), ERR);
    }
    return OAPV_OK;

ERR:
    return ret;
}


int oapvm_get_all(oapvm_t mid, oapvm_payload_t *pld, int *num_plds)
{
    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_rv(ctx, OAPV_ERR_INVALID_ARGUMENT);
    if(pld == NULL) {
        int num_payload = 0;
        for(int i = 0; i < ctx->num; i++) {
            num_payload += ctx->md_arr[i].mdp_num;
            if(num_payload > OAPV_MAX_NUM_META_PAYLOADS || num_payload < 0)
                return OAPV_ERR_REACHED_MAX;
        }
        *num_plds = num_payload;
        return OAPV_OK;
    }
    int pld_cnt = 0;
    for(int i = 0; i < ctx->num; i++) {
        oapv_md_t  *cur_md = &ctx->md_arr[i];
        int         group_id = cur_md->group_id;
        oapv_mdp_t *mdp = cur_md->md_payload;
        while(mdp != NULL) {
            if(pld_cnt >= *num_plds)
                return OAPV_ERR_REACHED_MAX;
            pld[pld_cnt].group_id = group_id;
            pld[pld_cnt].size = mdp->pld_size;
            pld[pld_cnt].data = mdp->pld_data;
            pld[pld_cnt].type = mdp->pld_type;
            if(pld[pld_cnt].type == OAPV_METADATA_USER_DEFINED) {
                oapv_mcpy(pld[pld_cnt].uuid, mdp->pld_data, 16);
            }
            pld_cnt++;
            mdp = mdp->next;
        }
    }
    *num_plds = pld_cnt;
    return OAPV_OK;
}

void oapvm_rem_all(oapvm_t mid)
{
    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_r(ctx != NULL);
    for(int i = 0; i < ctx->num; i++) {
        meta_free_md(&ctx->md_arr[i]);
        oapv_mset(&ctx->md_arr[i], 0, sizeof(oapv_md_t));
    }
    ctx->num = 0;
}

oapvm_t oapvm_create(int *err)
{
    oapvm_ctx_t *ctx;
    ctx = oapv_malloc(sizeof(oapvm_ctx_t));
    if(ctx == NULL) {
        *err = OAPV_ERR_OUT_OF_MEMORY;
        return NULL;
    }
    oapv_mset(ctx, 0, sizeof(oapvm_ctx_t));

    ctx->magic = OAPVM_MAGIC_CODE;
    return ctx;
}

void oapvm_delete(oapvm_t mid)
{
    oapvm_ctx_t *ctx = meta_id_to_ctx(mid);
    oapv_assert_r(ctx != NULL);
    for(int i = 0; i < ctx->num; i++) {
        meta_free_md(&ctx->md_arr[i]);
    }
    oapv_mfree(ctx);
}
