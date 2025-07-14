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

#ifndef _OAPV_METADATA_H_
#define _OAPV_METADATA_H_

#include "oapv_def.h"

/* oapv metadata container magic code */
#define OAPVM_MAGIC_CODE 0x4150314D /* AP1M */

/* Metadata payload */
typedef struct oapv_mdp oapv_mdp_t;
struct oapv_mdp {
    u32         pld_type; /* u(32) */
    u32         pld_size; /* u(32) */
    void       *pld_data;
    oapv_mdp_t *next;
};

typedef struct oapv_md oapv_md_t;
struct oapv_md {
    int         mdp_num;
    int         group_id;
    oapv_mdp_t *md_payload;
};

typedef struct oapvm_ctx oapvm_ctx_t;
struct oapvm_ctx {
    u32       magic; // magic code
    oapv_md_t md_arr[OAPV_MAX_NUM_METAS];
    int       num;
};

/* Filler metadata */
typedef struct oapv_md_fm oapv_md_fm_t;
struct oapv_md_fm {
    u8 *ff_byte; /* f(8) */
};

/* Recommendation ITU-T T.35 metadata*/
typedef struct oapv_md_t35 oapv_md_t35_t;
struct oapv_md_t35 {
    u32 itu_t_t35_country_code;           /* b(8) */
    u32 itu_t_t35_country_code_extension; /* b(8) */
    u8 *itu_t_t35_payload;
};

/* Mastering display colour volume metadata*/
typedef struct oapv_md_mdcv oapv_md_mdcv_t;
struct oapv_md_mdcv {
    u16 primary_chromaticity_x[3];  /* u(16) */
    u16 primary_chromaticity_y[3];  /* u(16) */
    u16 white_point_chromaticity_x; /* u(16) */
    u16 white_point_chromaticity_y; /* u(16) */
    u32 max_mastering_luminance;    /* u(32) */
    u32 min_mastering_luminance;    /* u(32) */
};

/* Content light level information*/
typedef struct oapv_md_cll oapv_md_cll_t;
struct oapv_md_cll {
    u16 max_cll;  /* u(16) */
    u16 max_fall; /* u(16) */
};

/* User defined metadata */
typedef struct oapv_md_usd oapv_md_usd_t;
struct oapv_md_usd {
    u8  uuid[16]; /* u(128) */
    u8 *user_defined_data_payload;
};

/* Undefined metadata */
typedef struct oapv_md_ud oapv_md_ud_t;
struct oapv_md_ud {
    u8 *undefined_data_payload;
};

#endif /* _OAPV_METADATA_H_ */
