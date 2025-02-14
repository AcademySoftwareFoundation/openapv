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

#ifndef __OAPV_H__3342320849320483827648324783920483920432847382948__
#define __OAPV_H__3342320849320483827648324783920483920432847382948__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OAPV_STATIC_DEFINE
#define OAPV_EXPORT
#else
#include <oapv/oapv_exports.h>
#endif


/* size of macroblock */
#define OAPV_LOG2_MB                    (4)
#define OAPV_LOG2_MB_W                  (4)
#define OAPV_LOG2_MB_H                  (4)
#define OAPV_MB_W                       (1 << OAPV_LOG2_MB_W)
#define OAPV_MB_H                       (1 << OAPV_LOG2_MB_H)
#define OAPV_MB_D                       (OAPV_MB_W * OAPV_MB_H)

/* size of block */
#define OAPV_LOG2_BLK                   (3)
#define OAPV_LOG2_BLK_W                 (3)
#define OAPV_LOG2_BLK_H                 (3)
#define OAPV_BLK_W                      (1 << OAPV_LOG2_BLK)
#define OAPV_BLK_H                      (1 << OAPV_LOG2_BLK)
#define OAPV_BLK_D                      (OAPV_BLK_W * OAPV_BLK_H)

/* size of tile */
#define OAPV_MAX_TILE_ROWS              (20)
#define OAPV_MAX_TILE_COLS              (20)
#define OAPV_MAX_TILES                  (OAPV_MAX_TILE_ROWS * OAPV_MAX_TILE_COLS)
#define OAPV_MIN_TILE_W_MB              (16)
#define OAPV_MIN_TILE_H_MB              (8)

/* maximum number of thread */
#define OAPV_MAX_THREADS                (32)

/*****************************************************************************
 * return values and error code
 *****************************************************************************/
#define OAPV_OK                         (0)
#define OAPV_ERR                        (-1) /* generic error */
#define OAPV_ERR_INVALID_ARGUMENT       (-101)
#define OAPV_ERR_OUT_OF_MEMORY          (-102)
#define OAPV_ERR_REACHED_MAX            (-103)
#define OAPV_ERR_UNSUPPORTED            (-104)
#define OAPV_ERR_UNEXPECTED             (-105)
#define OAPV_ERR_UNSUPPORTED_COLORSPACE (-201)
#define OAPV_ERR_MALFORMED_BITSTREAM    (-202)
#define OAPV_ERR_OUT_OF_BS_BUF          (-203) /* too small bitstream buffer */
#define OAPV_ERR_NOT_FOUND              (-204)
#define OAPV_ERR_FAILED_SYSCALL         (-301)   /* failed system call */
#define OAPV_ERR_UNKNOWN                (-32767) /* unknown error */

/* return value checking */
#define OAPV_SUCCEEDED(ret)             ((ret) >= OAPV_OK)
#define OAPV_FAILED(ret)                ((ret) < OAPV_OK)

/*****************************************************************************
 * color spaces
 * - value format = (endian << 14) | (bit-depth << 8) | (color format)
 * - endian (1bit): little endian = 0, big endian = 1
 * - bit-depth (6bit): 0~63
 * - color format (8bit): 0~255
 *****************************************************************************/
/* color formats */
#define OAPV_CF_UNKNOWN                 (0)  /* unknown color format */
#define OAPV_CF_YCBCR400                (10) /* Y only */
#define OAPV_CF_YCBCR420                (11) /* YCbCr 420 */
#define OAPV_CF_YCBCR422                (12) /* YCBCR 422 narrow chroma*/
#define OAPV_CF_YCBCR444                (13) /* YCBCR 444*/
#define OAPV_CF_YCBCR4444               (14) /* YCBCR 4444*/
#define OAPV_CF_YCBCR422N               OAPV_CF_YCBCR422
#define OAPV_CF_YCBCR422W               (18) /* YCBCR422 wide chroma */
#define OAPV_CF_PLANAR2                 (20) /* Planar Y, Combined CB-CR, 422 */

/* macro for color space */
#define OAPV_CS_GET_FORMAT(cs)          (((cs) >> 0) & 0xFF)
#define OAPV_CS_GET_BIT_DEPTH(cs)       (((cs) >> 8) & 0x3F)
#define OAPV_CS_GET_BYTE_DEPTH(cs)      ((OAPV_CS_GET_BIT_DEPTH(cs) + 7) >> 3)
#define OAPV_CS_GET_ENDIAN(cs)          (((cs) >> 14) & 0x1)
#define OAPV_CS_SET(f, bit, e)          (((e) << 14) | ((bit) << 8) | (f))
#define OAPV_CS_SET_FORMAT(cs, v)       (((cs) & ~0xFF) | ((v) << 0))
#define OAPV_CS_SET_BIT_DEPTH(cs, v)    (((cs) & ~(0x3F << 8)) | ((v) << 8))
#define OAPV_CS_SET_ENDIAN(cs, v)       (((cs) & ~(0x1 << 14)) | ((v) << 14))

/* pre-defined color spaces */
#define OAPV_CS_UNKNOWN                 OAPV_CS_SET(0, 0, 0)
#define OAPV_CS_YCBCR400                OAPV_CS_SET(OAPV_CF_YCBCR400, 8, 0)
#define OAPV_CS_YCBCR420                OAPV_CS_SET(OAPV_CF_YCBCR420, 8, 0)
#define OAPV_CS_YCBCR422                OAPV_CS_SET(OAPV_CF_YCBCR422, 8, 0)
#define OAPV_CS_YCBCR444                OAPV_CS_SET(OAPV_CF_YCBCR444, 8, 0)
#define OAPV_CS_YCBCR4444               OAPV_CS_SET(OAPV_CF_YCBCR4444, 8, 0)
#define OAPV_CS_YCBCR400_10LE           OAPV_CS_SET(OAPV_CF_YCBCR400, 10, 0)
#define OAPV_CS_YCBCR420_10LE           OAPV_CS_SET(OAPV_CF_YCBCR420, 10, 0)
#define OAPV_CS_YCBCR422_10LE           OAPV_CS_SET(OAPV_CF_YCBCR422, 10, 0)
#define OAPV_CS_YCBCR444_10LE           OAPV_CS_SET(OAPV_CF_YCBCR444, 10, 0)
#define OAPV_CS_YCBCR4444_10LE          OAPV_CS_SET(OAPV_CF_YCBCR4444, 10, 0)
#define OAPV_CS_YCBCR400_12LE           OAPV_CS_SET(OAPV_CF_YCBCR400, 12, 0)
#define OAPV_CS_YCBCR420_12LE           OAPV_CS_SET(OAPV_CF_YCBCR420, 12, 0)
#define OAPV_CS_YCBCR400_14LE           OAPV_CS_SET(OAPV_CF_YCBCR400, 14, 0)
#define OAPV_CS_YCBCR420_14LE           OAPV_CS_SET(OAPV_CF_YCBCR420, 14, 0)
#define OAPV_CS_P210                    OAPV_CS_SET(OAPV_CF_PLANAR2, 10, 0)

/* max number of color channel: ex) YCbCr4444 -> 4 channels */
#define OAPV_MAX_CC                     (4)

/*****************************************************************************
 * config types
 *****************************************************************************/
#define OAPV_CFG_SET_QP                 (201)
#define OAPV_CFG_SET_BPS                (202)
#define OAPV_CFG_SET_FPS_NUM            (204)
#define OAPV_CFG_SET_FPS_DEN            (205)
#define OAPV_CFG_SET_QP_MIN             (208)
#define OAPV_CFG_SET_QP_MAX             (209)
#define OAPV_CFG_SET_USE_FRM_HASH       (301)
#define OAPV_CFG_GET_QP_MIN             (600)
#define OAPV_CFG_GET_QP_MAX             (601)
#define OAPV_CFG_GET_QP                 (602)
#define OAPV_CFG_GET_RCT                (603)
#define OAPV_CFG_GET_BPS                (604)
#define OAPV_CFG_GET_FPS_NUM            (605)
#define OAPV_CFG_GET_FPS_DEN            (606)
#define OAPV_CFG_GET_WIDTH              (701)
#define OAPV_CFG_GET_HEIGHT             (702)

/*****************************************************************************
 * HLS configs
 *****************************************************************************/
#define OAPV_MAX_GRP_SIZE               ((1 << 16) - 1) // 0xFFFF reserved

/*****************************************************************************
 * PBU types
 *****************************************************************************/
#define OAPV_PBU_TYPE_RESERVED          (0)
#define OAPV_PBU_TYPE_PRIMARY_FRAME     (1)
#define OAPV_PBU_TYPE_NON_PRIMARY_FRAME (2)
#define OAPV_PBU_TYPE_PREVIEW_FRAME     (25)
#define OAPV_PBU_TYPE_DEPTH_FRAME       (26)
#define OAPV_PBU_TYPE_ALPHA_FRAME       (27)
#define OAPV_PBU_TYPE_AU_INFO           (65)
#define OAPV_PBU_TYPE_METADATA          (66)
#define OAPV_PBU_TYPE_FILLER            (67)
#define OAPV_PBU_TYPE_UNKNOWN           (-1)
#define OAPV_PBU_NUMS                   (10)

#define OAPV_PBU_FRAME_TYPE_NUM         (5)

/*****************************************************************************
 * metadata types
 *****************************************************************************/
#define OAPV_METADATA_ITU_T_T35         (4)
#define OAPV_METADATA_MDCV              (5)
#define OAPV_METADATA_CLL               (6)
#define OAPV_METADATA_FILLER            (10)
#define OAPV_METADATA_USER_DEFINED      (170)

/*****************************************************************************
 * profiles
 *****************************************************************************/
#define OAPV_PROFILE_422_10             (33)

/*****************************************************************************
 * optimization level control
 *****************************************************************************/
#define OAPV_PRESET_FASTEST             (0)
#define OAPV_PRESET_FAST                (1)
#define OAPV_PRESET_MEDIUM              (2)
#define OAPV_PRESET_SLOW                (3)
#define OAPV_PRESET_PLACEBO             (4)
#define OAPV_PRESET_DEFAULT             OAPV_PRESET_MEDIUM

/*****************************************************************************
 * rate-control types
 *****************************************************************************/
#define OAPV_RC_CQP                     (0)
#define OAPV_RC_ABR                     (1)

/*****************************************************************************
 * type and macro for media time
 *****************************************************************************/
typedef long long        oapv_mtime_t; /* in 100-nanosec unit */

/*****************************************************************************
 * image buffer format
 *
 *    baddr
 *     +---------------------------------------------------+ ---
 *     |                                                   |  ^
 *     |                                              |    |  |
 *     |      a                                       v    |  |
 *     |   --- +-----------------------------------+ ---   |  |
 *     |    ^  |  (x, y)                           |  y    |  |
 *     |    |  |   +---------------------------+   + ---   |  |
 *     |    |  |   |                           |   |  ^    |  |
 *     |    |  |   |            /\             |   |  |    |  |
 *     |    |  |   |           /  \            |   |  |    |  |
 *     |    |  |   |          /    \           |   |  |    |  |
 *     |       |   |  +--------------------+   |   |       |
 *     |    ah |   |   \                  /    |   |  h    |  e
 *     |       |   |    +----------------+     |   |       |
 *     |    |  |   |       |          |        |   |  |    |  |
 *     |    |  |   |      @    O   O   @       |   |  |    |  |
 *     |    |  |   |        \    ~   /         |   |  v    |  |
 *     |    |  |   +---------------------------+   | ---   |  |
 *     |    v  |                                   |       |  |
 *     |   --- +---+-------------------------------+       |  |
 *     |     ->| x |<----------- w ----------->|           |  |
 *     |       |<--------------- aw -------------->|       |  |
 *     |                                                   |  v
 *     +---------------------------------------------------+ ---
 *
 *     |<---------------------- s ------------------------>|
 *
 * - x, y, w, aw, h, ah : unit of pixel
 * - s, e : unit of byte
 *****************************************************************************/

typedef struct oapv_imgb oapv_imgb_t;
struct oapv_imgb {
    int           cs; /* color space */
    int           np; /* number of plane */
    /* width (in unit of pixel) */
    int           w[OAPV_MAX_CC];
    /* height (in unit of pixel) */
    int           h[OAPV_MAX_CC];
    /* X position of left top (in unit of pixel) */
    int           x[OAPV_MAX_CC];
    /* Y postion of left top (in unit of pixel) */
    int           y[OAPV_MAX_CC];
    /* buffer stride (in unit of byte) */
    int           s[OAPV_MAX_CC];
    /* buffer elevation (in unit of byte) */
    int           e[OAPV_MAX_CC];
    /* address of each plane */
    void         *a[OAPV_MAX_CC];

    /* hash data for signature */
    unsigned char hash[OAPV_MAX_CC][16];

    /* time-stamps */
    oapv_mtime_t  ts[4];

    int           ndata[4]; /* arbitrary data, if needs */
    void         *pdata[4]; /* arbitrary adedress if needs */

    /* aligned width (in unit of pixel) */
    int           aw[OAPV_MAX_CC];
    /* aligned height (in unit of pixel) */
    int           ah[OAPV_MAX_CC];

    /* left padding size (in unit of pixel) */
    int           padl[OAPV_MAX_CC];
    /* right padding size (in unit of pixel) */
    int           padr[OAPV_MAX_CC];
    /* up padding size (in unit of pixel) */
    int           padu[OAPV_MAX_CC];
    /* bottom padding size (in unit of pixel) */
    int           padb[OAPV_MAX_CC];

    /* address of actual allocated buffer */
    void         *baddr[OAPV_MAX_CC];
    /* actual allocated buffer size */
    int           bsize[OAPV_MAX_CC];

    /* life cycle management */
    int           refcnt;
    int (*addref)(oapv_imgb_t *imgb);
    int (*getref)(oapv_imgb_t *imgb);
    int (*release)(oapv_imgb_t *imgb);
};

typedef struct oapv_frm oapv_frm_t;
struct oapv_frm {
    oapv_imgb_t *imgb;
    int          pbu_type;
    int          group_id;
};

#define OAPV_MAX_NUM_FRAMES (16) // max number of frames in an access unit
#define OAPV_MAX_NUM_METAS  (16) // max number of metadata in an access unit

typedef struct oapv_frms oapv_frms_t;
struct oapv_frms {
    int        num_frms;                 // number of frames
    oapv_frm_t frm[OAPV_MAX_NUM_FRAMES]; // container of frames
};

/*****************************************************************************
 * Bitstream buffer
 *****************************************************************************/
typedef struct oapv_bitb oapv_bitb_t;
struct oapv_bitb {
    /* user space address indicating buffer */
    void        *addr;
    /* physical address indicating buffer, if any */
    void        *pddr;
    /* byte size of buffer memory */
    int          bsize;
    /* byte size of bitstream in buffer */
    int          ssize;
    /* bitstream has an error? */
    int          err;
    /* arbitrary data, if needs */
    int          ndata[4];
    /* arbitrary address, if needs */
    void        *pdata[4];
    /* time-stamps */
    oapv_mtime_t ts[4];
};

/*****************************************************************************
 * brief information of frame
 *****************************************************************************/
typedef struct oapv_frm_info oapv_frm_info_t;
struct oapv_frm_info {
    int           w;
    int           h;
    int           cs;
    int           pbu_type;
    int           group_id;
    int           profile_idc;
    int           level_idc;
    int           band_idc;
    int           chroma_format_idc;
    int           bit_depth;
    int           capture_time_distance;
    /* custom quantization matrix */
    int           use_q_matrix;
    unsigned char q_matrix[OAPV_MAX_CC][OAPV_BLK_D]; // only meaningful if use_q_matrix is true
    /* color description values */
    int           color_description_present_flag;
    unsigned char color_primaries;          // only meaningful if color_description_present_flag is true
    unsigned char transfer_characteristics; // only meaningful if color_description_present_flag is true
    unsigned char matrix_coefficients;      // only meaningful if color_description_present_flag is true
    int           full_range_flag;          // only meaningful if color_description_present_flag is true
};

typedef struct oapv_au_info oapv_au_info_t;
struct oapv_au_info {
    int             num_frms; // number of frames
    oapv_frm_info_t frm_info[OAPV_MAX_NUM_FRAMES];
};

/*****************************************************************************
 * coding parameters
 *****************************************************************************/
typedef struct oapve_param oapve_param_t;
struct oapve_param {
    /* profile_idc */
    int           profile_idc;
    /* level */
    int           level_idc;
    /* band */
    int           band_idc;
    /* width of input frame */
    int           w;
    /* height of input frame */
    int           h;
    /* frame rate (Hz) numerator, denominator */
    int           fps_num;
    int           fps_den;
    /* rate control type */
    int           rc_type;
    /* quantization parameters : 0 ~ (63 + (bitdepth - 10)*6)
       - 10bit input: 0 ~ 63
       - 12bit input: 0 ~ 75
    */
    unsigned char qp;
    /* quantization parameter offsets */
    signed char   qp_offset_c1;
    /* quantization parameter offsets */
    signed char   qp_offset_c2;
    /* quantization parameter offsets */
    signed char   qp_offset_c3;
    /* bitrate (unit: kbps) */
    int           bitrate;
    /* use filler data for tight constant bitrate */
    int           use_filler;
    /* use quantization matrix */
    int           use_q_matrix;
    unsigned char q_matrix[OAPV_MAX_CC][OAPV_BLK_D]; // raster-scan order
    /* color space */
    int           csp;
    int           tile_cols;
    int           tile_rows;
    int           tile_w_mb;
    int           tile_h_mb;
    int           preset;
    /* color description values */
    int           color_description_present_flag;
    unsigned char color_primaries;
    unsigned char transfer_characteristics;
    unsigned char matrix_coefficients;
    int           full_range_flag;
};

/*****************************************************************************
 * description for encoder creation
 *****************************************************************************/
typedef struct oapve_cdesc oapve_cdesc_t;
struct oapve_cdesc {
    int           max_bs_buf_size;            // max bitstream buffer size
    int           max_num_frms;               // max number of frames to be encoded
    int           threads;                    // number of threads
    oapve_param_t param[OAPV_MAX_NUM_FRAMES]; // encoding parameters
};

/*****************************************************************************
 * encoding status
 *****************************************************************************/
typedef struct oapve_stat oapve_stat_t;
struct oapve_stat {
    int            write;                         // byte size of encoded bitstream
    oapv_au_info_t aui;                           // information of encoded frames
    int            frm_size[OAPV_MAX_NUM_FRAMES]; // bitstream byte size of each frame
};

/*****************************************************************************
 * description for decoder creation
 *****************************************************************************/
typedef struct oapvd_cdesc oapvd_cdesc_t;
struct oapvd_cdesc {
    int threads; // number of threads
};

/*****************************************************************************
 * decoding status
 *****************************************************************************/
typedef struct oapvd_stat oapvd_stat_t;
struct oapvd_stat {
    int            read;                          // byte size of decoded bitstream (read size)
    oapv_au_info_t aui;                           // information of decoded frames
    int            frm_size[OAPV_MAX_NUM_FRAMES]; // bitstream byte size of each frame
};

/*****************************************************************************
 * metadata payload
 *****************************************************************************/
typedef struct oapvm_payload oapvm_payload_t;
struct oapvm_payload {
    int           group_id;  // group ID
    int           type;      // payload type
    unsigned char uuid[16];  // UUID for user-defined metadata payload
    void         *data;      // address of metadata payload
    int           data_size; // byte size of metadata payload
};

/*****************************************************************************
 * interface for metadata container
 *****************************************************************************/
typedef void       *oapvm_t; /* instance identifier for OAPV metadata container */

oapvm_t OAPV_EXPORT oapvm_create(int *err);
void OAPV_EXPORT oapvm_delete(oapvm_t mid);
void OAPV_EXPORT oapvm_rem_all(oapvm_t mid);
int OAPV_EXPORT oapvm_set(oapvm_t mid, int group_id, int type, void *data, int size, unsigned char *uuid);
int OAPV_EXPORT oapvm_get(oapvm_t mid, int group_id, int type, void **data, int *size, unsigned char *uuid);
int OAPV_EXPORT oapvm_rem(oapvm_t mid, int group_id, int type, unsigned char *uuid);
int OAPV_EXPORT oapvm_set_all(oapvm_t mid, oapvm_payload_t *pld, int num_plds);
int OAPV_EXPORT oapvm_get_all(oapvm_t mid, oapvm_payload_t *pld, int *num_plds);

/*****************************************************************************
 * interface for encoder
 *****************************************************************************/
typedef void       *oapve_t; /* instance identifier for OAPV encoder */

oapve_t OAPV_EXPORT oapve_create(oapve_cdesc_t *cdesc, int *err);
void OAPV_EXPORT oapve_delete(oapve_t eid);
int OAPV_EXPORT oapve_config(oapve_t eid, int cfg, void *buf, int *size);
int OAPV_EXPORT oapve_param_default(oapve_param_t *param);
int OAPV_EXPORT oapve_encode(oapve_t eid, oapv_frms_t *ifrms, oapvm_t mid, oapv_bitb_t *bitb, oapve_stat_t *stat, oapv_frms_t *rfrms);

/*****************************************************************************
 * interface for decoder
 *****************************************************************************/
typedef void       *oapvd_t; /* instance identifier for OAPV decoder */

oapvd_t OAPV_EXPORT oapvd_create(oapvd_cdesc_t *cdesc, int *err);
void OAPV_EXPORT oapvd_delete(oapvd_t did);
int OAPV_EXPORT oapvd_config(oapvd_t did, int cfg, void *buf, int *size);
int OAPV_EXPORT oapvd_decode(oapvd_t did, oapv_bitb_t *bitb, oapv_frms_t *ofrms, oapvm_t mid, oapvd_stat_t *stat);

/*****************************************************************************
 * interface for utility
 *****************************************************************************/
int OAPV_EXPORT oapvd_info(void *au, int au_size, oapv_au_info_t *aui);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OAPV_H__3342320849320483827648324783920483920432847382948__ */
