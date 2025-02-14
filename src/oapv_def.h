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

#ifndef _OAPV_DEF_H_4738294732894739280473892473829_
#define _OAPV_DEF_H_4738294732894739280473892473829_

#ifndef ENABLE_ENCODER
#define ENABLE_ENCODER 1 // for enabling encoder functionality
#endif

#ifndef ENABLE_DECODER
#define ENABLE_DECODER 1 // for enabling decoder functionality
#endif

#include "oapv.h"
#include "oapv_port.h"
#include "oapv_tpool.h"

/* oapv encoder magic code */
#define OAPVE_MAGIC_CODE          0x41503145 /* AP1E */

/* oapv decoder magic code */
#define OAPVD_MAGIC_CODE          0x41503144 /* AP1D */

/* Max. and min. Quantization parameter */
#define MAX_QUANT(BD)             (63 + ((BD-10)*6))
#define MIN_QUANT                 0

#define MAX_COST                  (1.7e+308) /* maximum cost value */

#define Y_C                       0 /* Y luma */
#define U_C                       1 /* Cb Chroma */
#define V_C                       2 /* Cr Chroma */
#define X_C                       3 /* X channel */
#define N_C                       4 /* maximum number of color component */

#define OAPV_VLC_TREE_LEVEL       2
#define OAPV_MIN_DC_LEVEL_CTX     0
#define OAPV_MAX_DC_LEVEL_CTX     5
#define OAPV_MIN_AC_LEVEL_CTX     0
#define OAPV_MAX_AC_LEVEL_CTX     4

/* Maximum transform dynamic range (excluding sign bit) */
#define MAX_TX_DYNAMIC_RANGE      15
#define MAX_TX_VAL                ((1 << MAX_TX_DYNAMIC_RANGE) - 1)
#define MIN_TX_VAL                (-(1 << MAX_TX_DYNAMIC_RANGE))

#define QUANT_SHIFT               14
#define QUANT_DQUANT_SHIFT        20

/* encoder status */
#define ENC_TILE_STAT_NOT_ENCODED 0
#define ENC_TILE_STAT_ON_ENCODING 1
#define ENC_TILE_STAT_ENCODED     2

/*****************************************************************************
 * PBU data structure
 *****************************************************************************/
typedef struct oapv_pbuh oapv_pbuh_t;
struct oapv_pbuh { // 4-byte
    int pbu_type;  /* u( 8) */
    int group_id;  /* u(16) */
    // int reserved_zero_8bits                    /* u( 8) */
};

/*****************************************************************************
 * Frame info
 *****************************************************************************/
typedef struct oapv_fi oapv_fi_t;
struct oapv_fi {     // 112byte
    int profile_idc; /* u( 8) */
    int level_idc;   /* u( 8) */
    int band_idc;    /* u( 3) */
    // int            reserved_zero_5bits;                     /* u( 5) */
    u32 frame_width;           /* u(24) */
    u32 frame_height;          /* u(24) */
    int chroma_format_idc;     /* u( 4) */
    int bit_depth;             /* u( 4) */
    int capture_time_distance; /* u( 8) */
    // int            reserved_zero_8bits;                     /* u( 8) */
};

/*****************************************************************************
 * Frame header
 *****************************************************************************/
typedef struct oapv_fh oapv_fh_t;
struct oapv_fh {
    oapv_fi_t fi;
    // int reserved_zero_8bits_3;                 /* u( 8) */
    int       color_description_present_flag; /* u( 1) */
    int       color_primaries;                /* u( 8) */
    int       transfer_characteristics;       /* u( 8) */
    int       matrix_coefficients;            /* u( 8) */
    int       full_range_flag;                /* u( 1) */
    int       use_q_matrix;                   /* u( 1) */
    /* (start) quantization_matix  */
    int       q_matrix[N_C][OAPV_BLK_H][OAPV_BLK_W]; /* u( 8) */
    /* ( end ) quantization_matix  */
    /* (start) tile_info */
    int       tile_width_in_mbs;            /* u(20) */
    int       tile_height_in_mbs;           /* u(20) */
    int       tile_size_present_in_fh_flag; /* u( 1) */
    u32       tile_size[OAPV_MAX_TILES];    /* u(32) */
    /* ( end ) tile_info  */
    // int reserved_zero_8bits_4;                   /* u( 8) */
};

/*****************************************************************************
 * Tile header
 *****************************************************************************/
#define OAPV_TILE_SIZE_LEN 4 /* u(32), 4byte */
typedef struct oapv_th oapv_th_t;
struct oapv_th {
    int tile_header_size;    /* u(16) */
    int tile_index;          /* u(16) */
    u32 tile_data_size[N_C]; /* u(32) */
    int tile_qp[N_C];        /* u( 8) */
    int reserved_zero_8bits; /* u( 8) */
};

/*****************************************************************************
 * Access unit info
 *****************************************************************************/
typedef struct oapv_aui oapv_aui_t;
struct oapv_aui {
    int       num_frames;                    // u(16)
    int       pbu_type[OAPV_MAX_NUM_FRAMES]; // u(8)
    int       group_id[OAPV_MAX_NUM_FRAMES]; // u(16)
    // int reserved_zero_8bits[OAPV_MAX_NUM_FRAMES]; // u(8)
    oapv_fi_t frame_info[OAPV_MAX_NUM_FRAMES];
    // int reserved_zero_8bits // u(8)
};

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
 * pre-declaration
 *****************************************************************************/
typedef struct oapve_ctx  oapve_ctx_t;
typedef struct oapve_core oapve_core_t;

/*****************************************************************************
 * pre-defined function structure
 *****************************************************************************/
typedef void (*oapv_fn_itx_part_t)(s16 *coef, s16 *t, int shift, int line);
typedef void (*oapv_fn_itx_t)(s16 *coef, int shift1, int shift2, int line);
typedef void (*oapv_fn_tx_t)(s16 *coef, s16 *t, int shift, int line);
typedef void (*oapv_fn_itx_adj_t)(int *src, int *dst, int itrans_diff_idx, int diff_step, int shift);
typedef int (*oapv_fn_quant_t)(s16 *coef, u8 qp, int q_matrix[OAPV_BLK_D], int log2_w, int log2_h, int bit_depth, int deadzone_offset);
typedef void (*oapv_fn_dquant_t)(s16 *coef, s16 q_matrix[OAPV_BLK_D], int log2_w, int log2_h, s8 shift);
typedef int (*oapv_fn_sad_t)(int w, int h, void *src1, void *src2, int s_src1, int s_src2);
typedef s64 (*oapv_fn_ssd_t)(int w, int h, void *src1, void *src2, int s_src1, int s_src2);
typedef void (*oapv_fn_diff_t)(int w, int h, void *src1, void *src2, int s_src1, int s_src2, int s_diff, s16 *diff);

typedef double (*oapv_fn_enc_blk_cost_t)(oapve_ctx_t *ctx, oapve_core_t *core, int log2_w, int log2_h, int c);
typedef void (*oapv_fn_imgb_to_blk_rc_t)(oapv_imgb_t *imgb, int c, int x_l, int y_l, int w_l, int h_l, s16 *block);
typedef void (*oapv_fn_imgb_to_blk_t)(void *src, int blk_w, int blk_h, int s_src, int offset_src, int s_dst, void *dst);
typedef void (*oapv_fn_blk_to_imgb_t)(void *src, int blk_w, int blk_h, int s_src, int offset_dst, int s_dst, void *dst);
typedef void (*oapv_fn_img_pad_t)(oapve_ctx_t *ctx, oapv_imgb_t *imgb);
typedef int (*oapv_fn_had8x8_t)(pel *org, int s_org);

/*****************************************************************************
 * rate-control related
 *****************************************************************************/
typedef struct oapve_rc_param {
    double alpha;
    double beta;

    int    qp;
    double lambda;
    double cost;
} oapve_rc_param_t;

typedef struct oapve_rc_tile {
    int              qp;
    int              target_bits;
    double           lambda;
    int              number_pixel;
    double           cost;
    int              target_bits_left;

    u64              total_dist;
    oapve_rc_param_t rc_param;
} oapve_rc_tile_t;

/*****************************************************************************
 * CORE information used for encoding process.
 *
 * The variables in this structure are very often used in encoding process.
 *****************************************************************************/
struct oapve_core {
    ALIGNED_16(s16 coef[OAPV_BLK_D]);
    ALIGNED_16(s16 coef_rec[OAPV_BLK_D]);
    oapve_ctx_t *ctx;
    int          prev_dc_ctx[N_C];
    int          prev_1st_ac_ctx[N_C];
    int          tile_idx;
    int          prev_dc[N_C];
    int          dc_diff; /* DC difference, which is represented in 17 bits */
                          /* and coded as abs_dc_coeff_diff and sign_dc_coeff_diff */
    int          qp[N_C]; // QPs for Y, Cb(U), Cr(V)
    int          dq_shift[N_C];

    int          q_mat_enc[N_C][OAPV_BLK_D];
    s16          q_mat_dec[N_C][OAPV_BLK_D];
    int          thread_idx;
    /* platform specific data, if needed */
    void        *pf;
};

#include "oapv_bs.h"

typedef struct oapve_tile oapve_tile_t;
struct oapve_tile {
    oapv_th_t       th;

    int             x; /* x (column) position in a frame in unit of pixel */
    int             y; /* y (row) position in a frame in unit of pixel */
    int             w; /* tile width in unit of pixel */
    int             h; /* tile height in unit of pixel */
    u32             tile_size;
    oapve_rc_tile_t rc;
    u8             *bs_buf;
    s32             bs_size;
    u32             bs_buf_max;
    volatile s32    stat;
};

/******************************************************************************
 * CONTEXT used for encoding process.
 *
 * All have to be stored are in this structure.
 *****************************************************************************/
struct oapve_ctx {
    u32                       magic; // magic code
    oapve_t                   id;    // identifier
    oapve_cdesc_t             cdesc;
    oapv_imgb_t              *imgb;
    oapv_imgb_t              *rec;

    oapve_param_t            *param;
    oapv_fh_t                 fh;
    oapve_tile_t              tile[OAPV_MAX_TILES];
    int                       num_tiles;
    int                       num_tile_cols;
    int                       num_tile_rows;
    int                       qp[N_C];
    s8                        qp_offset[N_C];
    int                       w;
    int                       h;
    int                       cfi;
    int                       num_comp;
    int                       bit_depth;
    int                       comp_sft[N_C][2];
    int                       log2_block;
    oapv_tpool_t             *tpool;
    oapv_thread_t             thread_id[OAPV_MAX_THREADS];
    oapv_sync_obj_t           sync_obj;
    oapve_core_t             *core[OAPV_MAX_THREADS];

    oapv_bs_t                 bs;
    const oapv_fn_itx_part_t *fn_itx_part;
    const oapv_fn_itx_t      *fn_itx;
    const oapv_fn_itx_adj_t  *fn_itx_adj;
    const oapv_fn_tx_t       *fn_txb;
    const oapv_fn_quant_t    *fn_quant;
    const oapv_fn_dquant_t   *fn_dquant;
    const oapv_fn_sad_t      *fn_sad;
    const oapv_fn_ssd_t      *fn_ssd;
    const oapv_fn_diff_t     *fn_diff;
    oapv_fn_imgb_to_blk_rc_t  fn_imgb_to_blk_rc;
    oapv_fn_imgb_to_blk_t     fn_imgb_to_blk[N_C];
    oapv_fn_blk_to_imgb_t     fn_blk_to_imgb[N_C];
    oapv_fn_img_pad_t         fn_img_pad;
    oapv_fn_enc_blk_cost_t    fn_enc_blk;
    oapv_fn_had8x8_t          fn_had8x8;

    int                       use_frm_hash;
    oapve_rc_param_t          rc_param;

    /* platform specific data, if needed */
    void                     *pf;
};
///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
#define DEC_TILE_STAT_NOT_DECODED 0
#define DEC_TILE_STAT_ON_DECODING 1
#define DEC_TILE_STAT_DECODED     2
#define DEC_TILE_STAT_SIZE_ERROR  -1

typedef struct oapvd_tile oapvd_tile_t;
struct oapvd_tile {
    oapv_th_t    th;

    int          x;         /* x (column) position in a frame in unit of pixel */
    int          y;         /* y (row) position in a frame in unit of pixel */
    int          w;         /* tile width in unit of pixel */
    int          h;         /* tile height in unit of pixel */
    u32          data_size; /* tile size including tile_size syntax */

    u8          *bs_beg; /* start position of tile in input bistream */
    u8          *bs_end; /* end position of tile() in input bistream */
    volatile s32 stat;   // decoding status
};

typedef struct oapvd_core oapvd_core_t;
typedef struct oapvd_ctx  oapvd_ctx_t;

struct oapvd_core {
    ALIGNED_16(s16 coef[OAPV_MB_D]);
    oapvd_ctx_t *ctx;

    int          prev_dc_ctx[N_C];
    int          prev_1st_ac_ctx[N_C];
    int          prev_dc[N_C];
    int          dc_diff; /* DC difference, which is represented in 17 bits */
                          /* and coded as abs_dc_coeff_diff and sign_dc_coeff_diff */
    int          qp[N_C];
    int          dq_shift[N_C];
    s16          q_mat[N_C][OAPV_BLK_D];

    int          tile_idx;

    /* platform specific data, if needed */
    void        *pf;
};

struct oapvd_ctx {
    u32                     magic; // magic code
    oapvd_t                 id;    // identifier

    oapvd_cdesc_t           cdesc;
    oapvd_core_t           *core[OAPV_MAX_THREADS];
    oapv_imgb_t            *imgb;
    const oapv_fn_itx_t    *fn_itx;
    const oapv_fn_dquant_t *fn_dquant;
    oapv_fn_blk_to_imgb_t   fn_block_to_imgb[N_C];
    oapv_bs_t               bs;

    oapv_fh_t               fh;
    oapvd_tile_t            tile[OAPV_MAX_TILES];

    u8                     *tile_end;
    int                     num_tiles;
    int                     num_tile_cols;
    int                     num_tile_rows;
    int                     w;
    int                     h;
    oapv_tpool_t           *tpool;
    oapv_thread_t           thread_id[OAPV_MAX_THREADS];
    oapv_sync_obj_t         sync_obj;
    int                     cfi;              // chroma format indicator
    int                     bit_depth;        // bit depth of decoding picture
    int                     num_comp;         // number of components
    int                     comp_sft[N_C][2]; // width or height shift value of each compoents, 0: width, 1: height
    int                     use_frm_hash;

    /* platform specific data, if needed */
    void                   *pf;
};
///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

#define OAPV_FRAME_INFO_BYTE (112)
#define OAPV_PBU_HEADER_BYTE (32)

#include "oapv_metadata.h"
#include "oapv_vlc.h"
#include "oapv_tq.h"
#include "oapv_util.h"
#include "oapv_tbl.h"
#include "oapv_rc.h"
#include "oapv_sad.h"

#if X86_SSE
#include "sse/oapv_sad_sse.h"
#include "sse/oapv_tq_sse.h"
#include "avx/oapv_sad_avx.h"
#include "avx/oapv_tq_avx.h"
#elif ARM_NEON
#include "neon/oapv_sad_neon.h"
#include "neon/oapv_tq_neon.h"
#endif

#endif /* _OAPV_DEF_H_4738294732894739280473892473829_ */
