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

#ifndef OPENAPV_OAPV_H_
#define OPENAPV_OAPV_H_

/*****************************************************************************
 * OpenAPV: encoder and decoder library for the APV codec
 *
 * Instances
 *   oapve_t  encoder; oapve_create() / oapve_delete()
 *   oapvd_t  decoder; oapvd_create() / oapvd_delete()
 *   oapvm_t  metadata container of one access unit (AU);
 *            oapvm_create() / oapvm_delete()
 *   An instance is not thread-safe; use one instance per thread.
 *
 * Encoding
 *   oapve_param_default() and set oapve_cdesc_t.param[] for each frame
 *   -> oapve_create() -> oapve_encode() for each AU -> oapve_delete()
 *
 * Decoding
 *   API set 0 decodes a whole AU in one call:
 *     oapvd_create() -> for each AU: oapvd_info() to size the output images,
 *     then oapvd_decode() -> oapvd_delete()
 *   API set 1 decodes one PBU at a time: oapvd_info_pbu() tells the PBU type,
 *   then oapvd_decode_frame(), oapvd_decode_tiles() or oapvd_decode_metadata()
 *
 * Image (oapv_imgb_t) and bitstream (oapv_bitb_t) buffers are allocated and
 * owned by the application.
 *
 * Functions returning int return OAPV_OK on success and a negative
 * OAPV_ERR_* code on failure; test them with OAPV_SUCCEEDED() and
 * OAPV_FAILED().
 *
 * See readme/programmers_guide.md for complete examples.
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ANDROID) || defined(OAPV_STATIC_DEFINE)
    #define OAPV_EXPORT
#else
    #include <oapv/oapv_exports.h>
#endif

/*****************************************************************************
 * version and related macro
 * the version string follows the rule of API_SET.MAJOR.MINOR.PATCH
 *****************************************************************************/
#define OAPV_VER_SET(apiset, major, minor, patch) \
    (((apiset & 0xFF) << 24)|((major & 0xFF) << 16)|((minor & 0xFF) << 8)|\
    (patch & 0xFF))
#define OAPV_VER_GET_APISET(v)          (((v) >> 24) & 0xFF)
#define OAPV_VER_GET_MAJOR(v)           (((v) >> 16) & 0xFF)
#define OAPV_VER_GET_MINOR(v)           (((v) >>  8) & 0xFF)
#define OAPV_VER_GET_PATCH(v)           (((v) >>  0) & 0xFF)

// version numbers (should be changed in case of new release)
#define OAPV_VER_APISET                 (1)
#define OAPV_VER_MAJOR                  (1)
#define OAPV_VER_MINOR                  (2)
#define OAPV_VER_PATCH                  (0)

// 4-bytes version number
#define OAPV_VER_NUM \
    OAPV_VER_SET(OAPV_VER_APISET,OAPV_VER_MAJOR,OAPV_VER_MINOR,OAPV_VER_PATCH)

// size of macroblock
#define OAPV_LOG2_MB                    (4)
#define OAPV_LOG2_MB_W                  (4)
#define OAPV_LOG2_MB_H                  (4)
#define OAPV_MB_W                       (1 << OAPV_LOG2_MB_W)
#define OAPV_MB_H                       (1 << OAPV_LOG2_MB_H)
#define OAPV_MB_D                       (OAPV_MB_W * OAPV_MB_H)

// size of block
#define OAPV_LOG2_BLK                   (3)
#define OAPV_LOG2_BLK_W                 (3)
#define OAPV_LOG2_BLK_H                 (3)
#define OAPV_BLK_W                      (1 << OAPV_LOG2_BLK)
#define OAPV_BLK_H                      (1 << OAPV_LOG2_BLK)
#define OAPV_BLK_D                      (OAPV_BLK_W * OAPV_BLK_H)

/* size of tile; the limits below apply only to the profiles other than the
   UNCONST ones */
#define OAPV_MAX_TILE_ROWS              (20) // max number of tile rows
#define OAPV_MAX_TILE_COLS              (20) // max number of tile columns
#define OAPV_MAX_TILES                  (OAPV_MAX_TILE_ROWS * OAPV_MAX_TILE_COLS)
#define OAPV_MIN_TILE_W_MB              (16) // min tile width in MBs
#define OAPV_MIN_TILE_H_MB              (8)  // min tile height in MBs
#define OAPV_MIN_TILE_W                 (OAPV_MIN_TILE_W_MB << OAPV_LOG2_MB_W)
#define OAPV_MIN_TILE_H                 (OAPV_MIN_TILE_H_MB << OAPV_LOG2_MB_H)

// maximum number of thread
#define OAPV_MAX_THREADS                (32)

/* value of 'threads' in oapve_cdesc_t and oapvd_cdesc_t that lets the
   library choose the number of threads */
#define OAPV_CDESC_THREADS_AUTO         (0)

// max number of frames in an access unit
#define OAPV_MAX_NUM_FRAMES             (16)
// max number of metadata in an access unit
#define OAPV_MAX_NUM_METAS              (16)
// max number of metadata payloads per access unit
#define OAPV_MAX_NUM_META_PAYLOADS      (128)
// max group ID; 0xFFFF is reserved
#define OAPV_MAX_GROUP_ID               (0xFFFE)

/*****************************************************************************
 * return values and error code
 *
 * Functions returning int return OAPV_OK on success and one of the negative
 * OAPV_ERR_* codes below on failure. The comment of a function lists a code
 * only when it has a meaning specific to that function.
 *****************************************************************************/
#define OAPV_OK                         (0)  // success
#define OAPV_ERR                        (-1) // generic error
// invalid argument, or an argument that holds an invalid value
#define OAPV_ERR_INVALID_ARGUMENT       (-101)
// memory allocation failed
#define OAPV_ERR_OUT_OF_MEMORY          (-102)
// a limit of the library or the capacity of an array was reached
#define OAPV_ERR_REACHED_MAX            (-103)
// the requested config id or parameter name is not supported
#define OAPV_ERR_UNSUPPORTED            (-104)
// unexpected internal error
#define OAPV_ERR_UNEXPECTED             (-105)
/* the color format or bit depth is not allowed by the profile or the
   bitstream */
#define OAPV_ERR_UNSUPPORTED_COLORSPACE (-201)
/* the bitstream is corrupted, truncated or does not conform to the APV
   specification */
#define OAPV_ERR_MALFORMED_BITSTREAM    (-202)
// the bitstream buffer is too small for the coded data
#define OAPV_ERR_OUT_OF_BS_BUF          (-203)
// the requested item, such as a metadata payload, does not exist
#define OAPV_ERR_NOT_FOUND              (-204)
// a system call, such as creating a thread or a mutex, failed
#define OAPV_ERR_FAILED_SYSCALL         (-301)
// invalid profile, or the profile is not known
#define OAPV_ERR_INVALID_PROFILE        (-400)
// invalid level, or a level that does not allow the bitrate
#define OAPV_ERR_INVALID_LEVEL          (-401)
// invalid band
#define OAPV_ERR_INVALID_BAND           (-402)
/* invalid width, such as a width that does not match the image or an odd
   width for 4:2:2 */
#define OAPV_ERR_INVALID_WIDTH          (-405)
// invalid height, such as a height that does not match the image
#define OAPV_ERR_INVALID_HEIGHT         (-406)
// invalid or missing frame rate
#define OAPV_ERR_INVALID_FPS            (-407)
// invalid QP or QP offset
#define OAPV_ERR_INVALID_QP             (-410)
// invalid APV family
#define OAPV_ERR_INVALID_FAMILY         (-501)
// unknown error
#define OAPV_ERR_UNKNOWN                (-32767)

// return value checking
#define OAPV_SUCCEEDED(ret)             ((ret) >= OAPV_OK)
#define OAPV_FAILED(ret)                ((ret) < OAPV_OK)

/*****************************************************************************
 * color spaces
 * - value format = (endian << 14) | (bit-depth << 8) | (color format)
 * - endian (1bit): little endian = 0, big endian = 1
 * - bit-depth (6bit): 0~63
 * - color format (8bit): 0~255
 *****************************************************************************/
// color formats
#define OAPV_CF_UNKNOWN                 (0)  // unknown color format
#define OAPV_CF_YCBCR400                (10) // Y only
#define OAPV_CF_YCBCR420                (11) // YCbCr 420
#define OAPV_CF_YCBCR422                (12) // YCBCR 422 narrow chroma
#define OAPV_CF_YCBCR444                (13) // YCBCR 444
#define OAPV_CF_YCBCR4444               (14) // YCBCR 4444
#define OAPV_CF_YCBCR422N               OAPV_CF_YCBCR422
#define OAPV_CF_YCBCR422W               (18) // YCBCR422 wide chroma
#define OAPV_CF_PLANAR2                 (20) // Planar Y, Combined CB-CR, 422

// macro for color space
#define OAPV_CS_GET_FORMAT(cs)          (((cs) >> 0) & 0xFF)
#define OAPV_CS_GET_BIT_DEPTH(cs)       (((cs) >> 8) & 0x3F)
#define OAPV_CS_GET_BYTE_DEPTH(cs)      ((OAPV_CS_GET_BIT_DEPTH(cs) + 7) >> 3)
#define OAPV_CS_GET_ENDIAN(cs)          (((cs) >> 14) & 0x1)
#define OAPV_CS_SET(f, bit, e)          (((e) << 14) | ((bit) << 8) | (f))
#define OAPV_CS_SET_FORMAT(cs, v)       (((cs) & ~0xFF) | ((v) << 0))
#define OAPV_CS_SET_BIT_DEPTH(cs, v)    (((cs) & ~(0x3F << 8)) | ((v) << 8))
#define OAPV_CS_SET_ENDIAN(cs, v)       (((cs) & ~(0x1 << 14)) | ((v) << 14))

// pre-defined color spaces
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
#define OAPV_CS_YCBCR422_12LE           OAPV_CS_SET(OAPV_CF_YCBCR422, 12, 0)
#define OAPV_CS_YCBCR444_12LE           OAPV_CS_SET(OAPV_CF_YCBCR444, 12, 0)
#define OAPV_CS_YCBCR4444_12LE          OAPV_CS_SET(OAPV_CF_YCBCR4444, 12, 0)
#define OAPV_CS_YCBCR4444_16LE          OAPV_CS_SET(OAPV_CF_YCBCR4444, 16, 0)
#define OAPV_CS_P210                    OAPV_CS_SET(OAPV_CF_PLANAR2, 10, 0)

// max number of color channel: ex) YCbCr4444 -> 4 channels
#define OAPV_MAX_CC                     (4)

/*****************************************************************************
 * config types
 *
 * Config ids for oapve_config() and oapvd_config(); each value is an int.
 *****************************************************************************/
#define OAPV_CFG_SET_QP                 (201) // set QP
#define OAPV_CFG_SET_BPS                (202) // set bitrate in kbps
#define OAPV_CFG_SET_FPS_NUM            (204) // set frame rate numerator
#define OAPV_CFG_SET_FPS_DEN            (205) // set frame rate denominator
#define OAPV_CFG_SET_QP_MIN             (208) // set minimum QP
#define OAPV_CFG_SET_QP_MAX             (209) // set maximum QP
#define OAPV_CFG_SET_USE_FRM_HASH       (301) // write frame hash as metadata
#define OAPV_CFG_SET_AU_BS_FMT          (302) // set AU bitstream format
#define OAPV_CFG_SET_TILE_SIZE_IN_FH    (303) // write tile sizes into the FH
#define OAPV_CFG_SET_DISABLE_COMPANDING (400) // disable de-companding (16C12)
#define OAPV_CFG_GET_QP_MIN             (600) // get minimum QP
#define OAPV_CFG_GET_QP_MAX             (601) // get maximum QP
#define OAPV_CFG_GET_QP                 (602) // get QP
#define OAPV_CFG_GET_RCT                (603) // get rate control type
#define OAPV_CFG_GET_BPS                (604) // get bitrate in kbps
#define OAPV_CFG_GET_FPS_NUM            (605) // get frame rate numerator
#define OAPV_CFG_GET_FPS_DEN            (606) // get frame rate denominator
#define OAPV_CFG_GET_WIDTH              (701) // get frame width
#define OAPV_CFG_GET_HEIGHT             (702) // get frame height
#define OAPV_CFG_GET_AU_BS_FMT          (802) // get AU bitstream format
#define OAPV_CFG_GET_TILE_SIZE_IN_FH    (803) // whether the FH has tile sizes

/* Target a specific frame's parameters in oapve_config(): the upper 16 bits of
 * 'cfg' carry the frame index, the lower 16 bits the config id above. Legacy
 * callers pass the plain id (index 0). */
#define OAPV_CFG_FRM(cfg, frm_idx) \
    ((((frm_idx) & 0xFFFF) << 16) | ((cfg) & 0xFFFF))

// values of OAPV_CFG_SET_AU_BS_FMT
// The output from the encoder is compliant with raw_bitstream_access_unit
#define OAPV_CFG_VAL_AU_BS_FMT_RBAU     (0)
// The output from the encoder is the only AU without bitstream format
#define OAPV_CFG_VAL_AU_BS_FMT_NONE     (1)

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

/* number of the frame PBU types; keep it in sync with
   OAPV_PBU_TYPE_IS_FRAME() */
#define OAPV_PBU_FRAME_TYPE_NUM         (5)
/* true for the PBU types that carry a frame: primary, non-primary, preview,
   depth and alpha */
#define OAPV_PBU_TYPE_IS_FRAME(pbu_type)   \
    ((pbu_type)==1 || (pbu_type)==2 || ((pbu_type)>=25 && (pbu_type)<=27))

/*****************************************************************************
 * metadata types
 *****************************************************************************/
#define OAPV_METADATA_ITU_T_T35         (4)   // ITU-T T.35
#define OAPV_METADATA_MDCV              (5)   // mastering display, 24 bytes
#define OAPV_METADATA_CLL               (6)   // content light level, 4 bytes
#define OAPV_METADATA_FILLER            (10)  // filler, all 0xFF
#define OAPV_METADATA_USER_DEFINED      (170) // starts with a 16-byte UUID

/* profile_idc values: the 16C12 profiles code 16-bit samples companded to
   12 bits */
#define OAPV_PROFILE_422_10             (33)
#define OAPV_PROFILE_422_12             (44)
#define OAPV_PROFILE_444_10             (55)
#define OAPV_PROFILE_444_12             (66)
#define OAPV_PROFILE_4444_10            (77)
#define OAPV_PROFILE_4444_12            (88)
#define OAPV_PROFILE_400_10             (99)
#define OAPV_PROFILE_444_16C12          (140)
#define OAPV_PROFILE_4444_16C12         (144)

// profile extensions: the profiles above without the tile constraint
#define OAPV_PROFILE_422_10_UNCONST     (43)
#define OAPV_PROFILE_422_12_UNCONST     (54)
#define OAPV_PROFILE_444_10_UNCONST     (65)
#define OAPV_PROFILE_444_12_UNCONST     (76)
#define OAPV_PROFILE_4444_10_UNCONST    (87)
#define OAPV_PROFILE_4444_12_UNCONST    (98)
#define OAPV_PROFILE_400_10_UNCONST     (109)

// APV families: bitrate classes
#define OAPV_FAMILY_422_LQ              (1) // 4:2:2, low data rate editing
#define OAPV_FAMILY_422_SQ              (2) // 4:2:2, standard quality
#define OAPV_FAMILY_422_HQ              (3) // 4:2:2, high quality
#define OAPV_FAMILY_444_UQ              (4) // 4:4:4, finishing

/* presets: values of oapve_param_t.preset, from the fastest to the best
   coding gain */
#define OAPV_PRESET_FASTEST             (0)
#define OAPV_PRESET_FAST                (1)
#define OAPV_PRESET_MEDIUM              (2)
#define OAPV_PRESET_SLOW                (3)
#define OAPV_PRESET_PLACEBO             (4)
#define OAPV_PRESET_DEFAULT             OAPV_PRESET_MEDIUM

// rate control types: values of oapve_param_t.rc_type
#define OAPV_RC_CQP                     (0) // constant QP
#define OAPV_RC_ABR                     (1) // average bitrate

/*****************************************************************************
 * constant string and value pairs
 *****************************************************************************/
/* pair of a name and a value, for the tables below; each table ends with an
   entry whose name is empty */
typedef struct oapv_dict_str_int oapv_dict_str_int_t; // dictionary type
struct oapv_dict_str_int {
    const char * key;
    const int    val;
};

// printable names of the PBU types
static const oapv_dict_str_int_t oapv_dict_pbu_type[] = {
    {"primary frame",           OAPV_PBU_TYPE_PRIMARY_FRAME},
    {"non-primary frame",       OAPV_PBU_TYPE_NON_PRIMARY_FRAME},
    {"preview frame",           OAPV_PBU_TYPE_PREVIEW_FRAME},
    {"depth frame",             OAPV_PBU_TYPE_DEPTH_FRAME},
    {"alpha frame",             OAPV_PBU_TYPE_ALPHA_FRAME},
    {"access unit information", OAPV_PBU_TYPE_AU_INFO},
    {"metadata",                OAPV_PBU_TYPE_METADATA},
    {"filler",                  OAPV_PBU_TYPE_FILLER},
    {"", 0} // termination
};

// printable names of the metadata types
static const oapv_dict_str_int_t oapv_dict_metadata_type[] = {
    {"itu_t_t35",       OAPV_METADATA_ITU_T_T35},
    {"mdcv",            OAPV_METADATA_MDCV},
    {"cll",             OAPV_METADATA_CLL},
    {"filler",          OAPV_METADATA_FILLER},
    {"user_defined",    OAPV_METADATA_USER_DEFINED},
    {"", 0} // termination
};

// values of the "profile" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_profile[] = {
    {"422-10",      OAPV_PROFILE_422_10},
    {"422-12",      OAPV_PROFILE_422_12},
    {"444-10",      OAPV_PROFILE_444_10},
    {"444-12",      OAPV_PROFILE_444_12},
    {"4444-10",     OAPV_PROFILE_4444_10},
    {"4444-12",     OAPV_PROFILE_4444_12},
    {"400-10",      OAPV_PROFILE_400_10},
    {"444-16C12",   OAPV_PROFILE_444_16C12},
    {"4444-16C12",  OAPV_PROFILE_4444_16C12},
    {"422-10-UNCONST",  OAPV_PROFILE_422_10_UNCONST},
    {"422-12-UNCONST",  OAPV_PROFILE_422_12_UNCONST},
    {"444-10-UNCONST",  OAPV_PROFILE_444_10_UNCONST},
    {"444-12-UNCONST",  OAPV_PROFILE_444_12_UNCONST},
    {"4444-10-UNCONST", OAPV_PROFILE_4444_10_UNCONST},
    {"4444-12-UNCONST", OAPV_PROFILE_4444_12_UNCONST},
    {"400-10-UNCONST",  OAPV_PROFILE_400_10_UNCONST},
    {"", 0} // termination
};

// values of the "preset" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_preset[] = {
    {"fastest", OAPV_PRESET_FASTEST},
    {"fast",    OAPV_PRESET_FAST},
    {"medium",  OAPV_PRESET_MEDIUM},
    {"slow",    OAPV_PRESET_SLOW},
    {"placebo", OAPV_PRESET_PLACEBO},
    {"", 0} // termination
};

// values of the "color-range" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_color_range[] = {
    {"limited", 0},
    {"tv",      0}, // alternative value of "limited"
    {"full",    1},
    {"pc",      1}, // alternative value of "full"
    {"", 0} // termination
};

// values of the "color-primaries" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_color_primaries[] = {
    {"reserved",     0},
    {"bt709",        1},
    {"unspecified",  2},
    {"reserved",     3},
    {"bt470m",       4},
    {"bt470bg",      5},
    {"smpte170m",    6},
    {"smpte240m",    7},
    {"film",         8},
    {"bt2020",       9},
    {"smpte428",    10},
    {"smpte431",    11},
    {"smpte432",    12},
    {"", 0} // termination
};

// values of the "color-transfer" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_color_transfer[] = {
    {"reserved",        0},
    {"bt709",           1},
    {"unspecified",     2},
    {"reserved",        3},
    {"bt470m",          4},
    {"bt470bg",         5},
    {"smpte170m",       6},
    {"smpte240m",       7},
    {"linear",          8},
    {"log100",          9},
    {"log316",         10},
    {"iec61966-2-4",   11},
    {"bt1361e",        12},
    {"iec61966-2-1",   13},
    {"bt2020-10",      14},
    {"bt2020-12",      15},
    {"smpte2084",      16},
    {"smpte428",       17},
    {"arib-std-b67",   18},
    {"", 0} // termination
};

// values of the "color-matrix" parameter of oapve_param_parse()
static const oapv_dict_str_int_t oapv_param_opts_color_matrix[] = {
    {"gbr",                 0},
    {"bt709",               1},
    {"unspecified",         2},
    {"reserved",            3},
    {"fcc",                 4},
    {"bt470bg",             5},
    {"smpte170m",           6},
    {"smpte240m",           7},
    {"ycgco",               8},
    {"bt2020nc",            9},
    {"bt2020c",            10},
    {"smpte2085",          11},
    {"chroma-derived-nc",  12},
    {"chroma-derived-c",   13},
    {"ictcp",              14},
    {"", 0} // termination
};

/*****************************************************************************
 * common types
 *****************************************************************************/
typedef long long        oapv_mtime_t; // in 100-nanosec unit

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

/*
 * Image buffer of one frame, allocated and owned by the application. The
 * arrays are indexed by plane; the layout of a plane is shown above.
 */
typedef struct oapv_imgb oapv_imgb_t;
struct oapv_imgb {
    int           cs; // color space
    int           np; // number of plane
    // width (in unit of pixel)
    int           w[OAPV_MAX_CC];
    // height (in unit of pixel)
    int           h[OAPV_MAX_CC];
    // X position of left top (in unit of pixel)
    int           x[OAPV_MAX_CC];
    // Y position of left top (in unit of pixel)
    int           y[OAPV_MAX_CC];
    // buffer stride (in unit of byte)
    int           s[OAPV_MAX_CC];
    // buffer elevation (in unit of byte)
    int           e[OAPV_MAX_CC];
    // address of each plane
    void         *a[OAPV_MAX_CC];
    // MD5 hash of each plane, filled when the frame hash is used
    unsigned char hash[OAPV_MAX_CC][16];
    // time-stamps
    oapv_mtime_t  ts[4];
    int           ndata[4]; // arbitrary data, if needed
    void         *pdata[4]; // arbitrary address if needed
    // aligned width (in unit of pixel)
    int           aw[OAPV_MAX_CC];
    // aligned height (in unit of pixel)
    int           ah[OAPV_MAX_CC];

    // left padding size (in unit of pixel)
    int           padl[OAPV_MAX_CC];
    // right padding size (in unit of pixel)
    int           padr[OAPV_MAX_CC];
    // up padding size (in unit of pixel)
    int           padu[OAPV_MAX_CC];
    // bottom padding size (in unit of pixel)
    int           padb[OAPV_MAX_CC];
    // address of actual allocated buffer
    void         *baddr[OAPV_MAX_CC];
    // actual allocated buffer size
    int           bsize[OAPV_MAX_CC];
    /* life cycle management: the library calls addref() while it uses the
       buffer and release() when it is done; either may be NULL */
    int           refcnt;
    int (*addref)(oapv_imgb_t *imgb);
    int (*getref)(oapv_imgb_t *imgb);
    int (*release)(oapv_imgb_t *imgb);
};

// one frame of an AU
typedef struct oapv_frm oapv_frm_t;
struct oapv_frm {
    oapv_imgb_t *imgb;     // image buffer of the frame
    int          pbu_type; // OAPV_PBU_TYPE_* of the frame
    int          group_id; /* 0 ~ OAPV_MAX_GROUP_ID; ties the frame to its
                              metadata */
};

// the frames of an AU, in the order they appear in the AU
typedef struct oapv_frms oapv_frms_t;
struct oapv_frms {
    int        num_frms;                 // number of frames
    oapv_frm_t frm[OAPV_MAX_NUM_FRAMES]; // container of frames
};

/*
 * Bitstream buffer, allocated and owned by the application. The encoder
 * writes into it up to 'bsize' bytes; the decoder reads 'ssize' bytes of it.
 */
typedef struct oapv_bitb oapv_bitb_t;
struct oapv_bitb {
    // user space address indicating buffer
    void        *addr;
    // physical address indicating buffer, if any
    void        *pddr;
    /* byte size of buffer memory; for decoding, 0 skips the check against
       'ssize' */
    int          bsize;
    // byte size of bitstream in buffer
    int          ssize;
    // bitstream has an error?
    int          err;
    // arbitrary data, if needs
    int          ndata[4];
    // arbitrary address, if needs
    void        *pdata[4];
    // time-stamps
    oapv_mtime_t ts[4];
};

/*****************************************************************************
 * memory operations interface
 *
 * Custom allocator for a single codec instance, supplied through the codec
 * descriptor (cdesc) at creation time and copied into the codec context;
 * there is no process-global allocator state. When a function pointer is
 * NULL the library uses the corresponding standard C routine. 'udata' is an
 * opaque, caller-owned pointer passed back to every callback (e.g. the host
 * allocator/arena object); it must stay valid for the codec's lifetime.
 *****************************************************************************/
#define OAPV_OPS_MAGIC_CODE_MEM (0x30504D4D) // 0PMM

typedef struct oapv_ops_mem oapv_ops_mem_t;
struct oapv_ops_mem {
    // set to OAPV_OPS_MAGIC_CODE_MEM for custom allocators; 0 => libc
    unsigned int magic;
    void *(*malloc)(void *udata, unsigned int size);
    void *(*calloc)(void *udata, unsigned int count, unsigned int size);
    void *(*realloc)(void *udata, void *ptr, unsigned int size);
    void (*free)(void *udata, void *ptr);
    void *udata;
};

// information of a frame, filled by the library
typedef struct oapv_frm_info oapv_frm_info_t;
struct oapv_frm_info {
    int           w; // frame width in pixels
    int           h; // frame height in pixels
    /* output frame's color space
       16bit color space will be set if the profile is 444/4444-16C12 */
    int           cs;
    int           pbu_type;
    int           group_id;
    int           profile_idc;
    int           level_idc;
    int           band_idc;
    int           chroma_format_idc;
    int           bit_depth; // coded bit depth
    int           capture_time_distance;
    int           use_companding; // 16C12 profiles: samples are companded
    // flag for custom quantization matrix
    int           use_q_matrix;
    // q_matrix is meaningful if use_q_matrix is true
    unsigned char q_matrix[OAPV_MAX_CC][OAPV_BLK_D];
    // flag for color_description_present_flag
    int           color_description_present_flag;
    /* color_primaries, transfer_characteristics, matrix_coefficients, and
       full_range_flag are meaningful if color_description_present_flag */
    unsigned char color_primaries;
    unsigned char transfer_characteristics;
    unsigned char matrix_coefficients;
    int           full_range_flag;
    // tile partitioning; 0 if unknown
    int           tile_width_in_mbs;
    int           tile_height_in_mbs;
    int           tile_cols;
    int           tile_rows;
    int           num_tiles;
};

// information of the frames of an AU, filled by the library
typedef struct oapv_au_info oapv_au_info_t;
struct oapv_au_info {
    int             num_frms; // number of frames
    oapv_frm_info_t frm_info[OAPV_MAX_NUM_FRAMES];
};

// PBU information, filled by oapvd_info_pbu()
typedef struct oapv_pbu_info oapv_pbu_info_t;
struct oapv_pbu_info {
    int  pbu_type; // OAPV_PBU_TYPE_*
    int  group_id;
};

// position and size of a tile, filled by oapvd_info_tile()
typedef struct oapv_tile_pos oapv_tile_pos_t;
struct oapv_tile_pos {
    int idx; // tile index in raster scan order
    int x_mb; // x-position in MB unit
    int y_mb; // y-position in MB unit
    int w_mb; // width in MB unit
    int h_mb; // height in MB unit
    int offset; // byte offset of the tile from the start of the PBU
    // byte size of the tile data; 0 if the frame header has no tile sizes
    int size;
};

/*****************************************************************************
 * metadata container
 *****************************************************************************/
// instance identifier for OAPV metadata container
typedef void       *oapvm_t;

// creation descriptor of a metadata container, copied by oapvm_create()
typedef struct oapvm_cdesc oapvm_cdesc_t;
struct oapvm_cdesc {
    // custom memory allocator interface, or NULL for standard C library
    const oapv_ops_mem_t *ops_mem;
};

// one metadata payload, for oapvm_set_all() and oapvm_get_all()
typedef struct oapvm_payload oapvm_payload_t;
struct oapvm_payload {
    int           group_id;  // group ID; 0 ~ OAPV_MAX_GROUP_ID
    int           type;      // payload type
    int           size;      // byte size of metadata payload
    void         *data;      // address of metadata payload
    /* UUID for user-defined metadata payload; filled by oapvm_get_all(),
       while oapvm_set_all() takes it from the first 16 bytes of 'data' */
    unsigned char uuid[16];
};

// Mastering display colour volume metadata payload
typedef struct oapvm_payload_mdcv oapvm_payload_mdcv_t;
struct oapvm_payload_mdcv {
    int           primary_chromaticity_x[3];  // range: 0 ~ 0xFFFF
    int           primary_chromaticity_y[3];  // range: 0 ~ 0xFFFF
    int           white_point_chromaticity_x; // range: 0 ~ 0xFFFF
    int           white_point_chromaticity_y; // range: 0 ~ 0xFFFF
    unsigned long max_mastering_luminance;    // range: 0 ~ 0xFFFFFFFF
    unsigned long min_mastering_luminance;    // range: 0 ~ 0xFFFFFFFF
};

// Content light level information metadata payload
typedef struct oapvm_payload_cll oapvm_payload_cll_t;
struct oapvm_payload_cll {
    int max_cll;  // range: 0 ~ 0xFFFF
    int max_fall; // range: 0 ~ 0xFFFF
};

/*
 * Create a metadata container. A container holds the metadata payloads of
 * one AU, grouped by group ID.
 *   - cdesc: creation descriptor. It sets an optional memory allocator. It is
 *            copied into the container.
 *   - err  : if not NULL, receives OAPV_OK or the error code.
 * Returns the container handle, or NULL on failure.
 */
OAPV_EXPORT oapvm_t oapvm_create(oapvm_cdesc_t *cdesc, int *err);

/*
 * Release a metadata container and all the payloads in it.
 *   - mid: container handle from oapvm_create()
 */
OAPV_EXPORT void oapvm_delete(oapvm_t mid);

/*
 * Add a payload to the container, or replace the payload of the same type in
 * the same group.
 *   - mid     : container handle
 *   - group_id: group ID the payload belongs to; 0 ~ OAPV_MAX_GROUP_ID
 *   - type    : payload type, such as OAPV_METADATA_MDCV
 *   - data    : payload data; it is copied into the container
 *   - size    : payload size in bytes
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_REACHED_MAX: the container already holds the maximum number of
 *     groups or payloads
 * NOTE: For OAPV_METADATA_USER_DEFINED, the first 16 bytes of 'data' are the
 *       UUID, and payloads with different UUIDs are kept apart.
 */
OAPV_EXPORT int oapvm_set(oapvm_t mid, int group_id, int type, void *data, int size);

/*
 * Get a payload from the container.
 *   - mid     : container handle
 *   - group_id: group ID of the payload
 *   - type    : payload type
 *   - data    : receives the address of the payload data
 *   - size    : receives the payload size in bytes
 *   - uuid    : 16-byte UUID; required for OAPV_METADATA_USER_DEFINED and
 *               ignored for the other types
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_NOT_FOUND: the container has no such payload
 * NOTE: '*data' points into the container; it stays valid until the payload
 *       is removed or replaced, or the container is cleared or released.
 */
OAPV_EXPORT int oapvm_get(oapvm_t mid, int group_id, int type, void **data, int *size, unsigned char *uuid);

/*
 * Remove a payload from the container.
 *   - mid     : container handle
 *   - group_id: group ID of the payload
 *   - type    : payload type
 *   - uuid    : 16-byte UUID; required for OAPV_METADATA_USER_DEFINED and
 *               ignored for the other types
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_NOT_FOUND: the container has no such payload
 */
OAPV_EXPORT int oapvm_rem(oapvm_t mid, int group_id, int type, unsigned char *uuid);

/*
 * Add or replace several payloads, as oapvm_set() does for each.
 *   - mid     : container handle
 *   - pld     : array of payloads
 *   - num_plds: number of entries of 'pld'
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: On failure, the payloads before the failing entry stay in the
 *       container.
 */
OAPV_EXPORT int oapvm_set_all(oapvm_t mid, oapvm_payload_t *pld, int num_plds);

/*
 * Get all the payloads in the container.
 *   - mid     : container handle
 *   - pld     : array that receives the payloads, or NULL to get only the
 *               number of payloads
 *   - num_plds: on input, the number of entries of 'pld'; on output, the
 *               number of payloads
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_REACHED_MAX: 'pld' is too small
 * NOTE: Call it with 'pld' set to NULL first to find the number of entries
 *       needed. The data address of each payload points into the container,
 *       as for oapvm_get().
 */
OAPV_EXPORT int oapvm_get_all(oapvm_t mid, oapvm_payload_t *pld, int *num_plds);

/*
 * Remove all the payloads from the container.
 *   - mid: container handle
 * NOTE: Call it between AUs when the container is reused.
 */
OAPV_EXPORT void oapvm_rem_all(oapvm_t mid);

/*
 * Write MDCV (mastering display colour volume) values in the payload format.
 *   - mdcv: MDCV values
 *   - data: receives the payload; it must hold at least 24 bytes
 *   - size: receives the payload size in bytes, which is 24
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Pass the result to oapvm_set() with OAPV_METADATA_MDCV.
 */
OAPV_EXPORT int oapvm_write_mdcv(oapvm_payload_mdcv_t *mdcv, void *data, int *size);

/*
 * Read MDCV values from a payload.
 *   - data: payload data, such as the one from oapvm_get()
 *   - size: payload size in bytes; at least 24
 *   - mdcv: receives the MDCV values
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 */
OAPV_EXPORT int oapvm_read_mdcv(void *data, int size, oapvm_payload_mdcv_t *mdcv);

/*
 * Write CLL (content light level) values in the payload format.
 *   - cll : CLL values
 *   - data: receives the payload; it must hold at least 4 bytes
 *   - size: receives the payload size in bytes, which is 4
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Pass the result to oapvm_set() with OAPV_METADATA_CLL.
 */
OAPV_EXPORT int oapvm_write_cll(oapvm_payload_cll_t *cll, void *data, int *size);

/*
 * Read CLL values from a payload.
 *   - data: payload data, such as the one from oapvm_get()
 *   - size: payload size in bytes; at least 4
 *   - cll : receives the CLL values
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 */
OAPV_EXPORT int oapvm_read_cll(void *data, int size, oapvm_payload_cll_t *cll);

/*****************************************************************************
 * encoder
 *****************************************************************************/
// instance identifier for OAPV encoder
typedef void       *oapve_t;

// level_idc of a level, such as 123 for level 4.1
#define OAPV_LEVEL_TO_LEVEL_IDC(level)   (int)(((level) * 30.0) + 0.5)
/* values that let the encoder choose the level, the band or the QP from the
   resolution, the frame rate and the bitrate */
#define OAPVE_PARAM_LEVEL_IDC_AUTO       (0)
#define OAPVE_PARAM_BAND_IDC_AUTO        (4)
#define OAPVE_PARAM_QP_AUTO              (255)

/*
 * Coding parameters of one frame slot of an AU. Fill them with
 * oapve_param_default() before setting any member.
 */
typedef struct oapve_param oapve_param_t;
struct oapve_param {
    // profile_idc defined in spec.; one of OAPV_PROFILE_*
    int           profile_idc;
    /* level_idc defined in spec.; OAPV_LEVEL_TO_LEVEL_IDC() of the level, or
       OAPVE_PARAM_LEVEL_IDC_AUTO */
    int           level_idc;
    // band_idc defined in spec.; 0 ~ 3, or OAPVE_PARAM_BAND_IDC_AUTO
    int           band_idc;
    // width of input frame
    int           w;
    // height of input frame
    int           h;
    // frame rate (Hz) numerator, denominator
    int           fps_num;
    int           fps_den;
    // rate control type; OAPV_RC_CQP or OAPV_RC_ABR
    int           rc_type;
    /* quantization parameters : 0 ~ (63 + (bitdepth - 10)*6)
       - 10bit input: 0 ~ 63
       - 12bit input: 0 ~ 75
       or OAPVE_PARAM_QP_AUTO
    */
    unsigned char qp;
    // quantization parameter offset of component 1
    signed char   qp_offset_c1;
    // quantization parameter offset of component 2
    signed char   qp_offset_c2;
    // quantization parameter offset of component 3
    signed char   qp_offset_c3;
    // bitrate (unit: kbps); the target of OAPV_RC_ABR
    int           bitrate;
    // use filler data for tight constant bitrate
    int           use_filler;
    // use quantization matrix
    int           use_q_matrix;
    unsigned char q_matrix[OAPV_MAX_CC][OAPV_BLK_D]; // raster-scan order
    /* NOTE: tile_w and tile_h value can be changed internally,
             if the values are not set properly.
             the min and max values are defined in APV specification */
    int           tile_w; // width of tile MUST be N * MB width
    int           tile_h; // height of tile MUST be N * MB height

    /* preset for setting trade-off between complexity and coding gain;
       one of OAPV_PRESET_* */
    int           preset;
    // color description values
    int           color_description_present_flag;
    unsigned char color_primaries;
    unsigned char transfer_characteristics;
    unsigned char matrix_coefficients;
    int           full_range_flag;
};

// creation descriptor of an encoder, copied by oapve_create()
typedef struct oapve_cdesc oapve_cdesc_t;
struct oapve_cdesc {
    // max bitstream buffer size; large enough for one coded AU
    int           max_bs_buf_size;
    // max number of frames to be encoded in one AU; 1 ~ OAPV_MAX_NUM_FRAMES
    int           max_num_frms;
    // max number of threads (or OAPV_CDESC_THREADS_AUTO for auto-assignment)
    int           threads;
    /* encoding parameters of each frame slot; the first 'max_num_frms' are
       used */
    oapve_param_t param[OAPV_MAX_NUM_FRAMES];
    // custom memory allocator interface, or NULL for standard C library
    const oapv_ops_mem_t *ops_mem;
};

// result of oapve_encode()
typedef struct oapve_stat oapve_stat_t;
struct oapve_stat {
    // byte size of encoded bitstream
    int            write;
    // information of encoded frames
    oapv_au_info_t aui;
    // bitstream byte size of each frame
    int            frm_size[OAPV_MAX_NUM_FRAMES];
};

/*
 * Create an encoder instance.
 *   - cdesc: creation descriptor. It sets the capacity of the bitstream
 *            buffer, the number of frames in one AU, the worker threads,
 *            the coding parameters of each frame slot (param[]) and an
 *            optional memory allocator. It is copied into the instance.
 *   - err  : if not NULL, receives OAPV_OK or the error code.
 * Returns the encoder handle, or NULL on failure.
 * NOTE: Fill each cdesc->param[] with oapve_param_default() first.
 */
OAPV_EXPORT oapve_t oapve_create(oapve_cdesc_t *cdesc, int *err);

/*
 * Release an encoder instance and everything it allocated.
 *   - eid: encoder handle from oapve_create()
 */
OAPV_EXPORT void oapve_delete(oapve_t eid);

/*
 * Get or set an encoding option of an existing encoder.
 *   - eid : encoder handle
 *   - cfg : an OAPV_CFG_SET_* or OAPV_CFG_GET_* config id. Most options
 *           belong to one frame slot; use OAPV_CFG_FRM(cfg, frm_idx) to
 *           select a slot other than 0. AU-level options ignore the frame
 *           index.
 *   - buf : the value to set, or where the value is returned
 *   - size: in bytes; sizeof(int) for most config ids
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_UNSUPPORTED: unsupported config id
 * NOTE: An option set here applies from the next oapve_encode() call.
 */
OAPV_EXPORT int oapve_config(oapve_t eid, int cfg, void *buf, int *size);

/*
 * Fill coding parameters with the default values.
 *   - param: parameters of one frame slot
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Call it before setting any member of oapve_param_t, so that members
 *       the application does not set keep valid values.
 */
OAPV_EXPORT int oapve_param_default(oapve_param_t *param);

/*
 * Set one coding parameter from a name and a value string.
 *   - param: parameters to update
 *   - name : parameter name, the same as the long option of the sample
 *            encoder, such as "profile", "qp", "bitrate" or "tile-w"
 *   - value: value in the same format as the sample encoder takes
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_UNSUPPORTED: unknown parameter name
 */
OAPV_EXPORT int oapve_param_parse(oapve_param_t* param, const char* name,  const char* value);

/*
 * Encode one access unit (AU).
 *   - eid  : encoder handle
 *   - ifrms: input frames of the AU. Each oapv_frm_t gives the image buffer,
 *            the PBU type and the group ID of one frame.
 *   - mid  : metadata to write into the AU, or NULL for none
 *   - bitb : output buffer allocated by the application
 *   - stat : receives the result of encoding
 *   - rfrms: if not NULL, the reconstructed frames are written into its
 *            image buffers
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_OUT_OF_BS_BUF: 'bitb' is too small for the coded AU
 * NOTE: 'mid' must not be NULL when the frame hash is enabled with
 *       OAPV_CFG_SET_USE_FRM_HASH for the slot of a primary or non-primary
 *       frame, because the hash is written as metadata.
 */
OAPV_EXPORT int oapve_encode(oapve_t eid, oapv_frms_t *ifrms, oapvm_t mid, oapv_bitb_t *bitb, oapve_stat_t *stat, oapv_frms_t *rfrms);

/*****************************************************************************
 * decoder
 *****************************************************************************/
// instance identifier for OAPV decoder
typedef void       *oapvd_t;

// creation descriptor of a decoder, copied by oapvd_create()
typedef struct oapvd_cdesc oapvd_cdesc_t;
struct oapvd_cdesc {
    // max number of threads (or OAPV_CDESC_THREADS_AUTO for auto-assignment)
    int threads;
    // custom memory allocator interface, or NULL for standard C library
    const oapv_ops_mem_t *ops_mem;
};

// result of oapvd_decode() or oapvd_decode_frame()
typedef struct oapvd_stat oapvd_stat_t;
struct oapvd_stat {
    // byte size of decoded bitstream (read size)
    int            read;
    // information of decoded frames
    oapv_au_info_t aui;
    // bitstream byte size of each frame
    int            frm_size[OAPV_MAX_NUM_FRAMES];
};

// API set 0: access unit decoding

/*
 * Create a decoder instance.
 *   - cdesc: creation descriptor. It sets the worker threads and an optional
 *            memory allocator. It is copied into the instance.
 *   - err  : if not NULL, receives OAPV_OK or the error code.
 * Returns the decoder handle, or NULL on failure.
 */
OAPV_EXPORT oapvd_t oapvd_create(oapvd_cdesc_t *cdesc, int *err);

/*
 * Release a decoder instance and everything it allocated.
 *   - did: decoder handle from oapvd_create()
 */
OAPV_EXPORT void oapvd_delete(oapvd_t did);

/*
 * Set a decoding option.
 *   - did : decoder handle
 *   - cfg : an OAPV_CFG_SET_* config id
 *   - buf : the value to set
 *   - size: in bytes; sizeof(int) for most config ids
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_UNSUPPORTED: unsupported config id
 */
OAPV_EXPORT int oapvd_config(oapvd_t did, int cfg, void *buf, int *size);

/*
 * Decode one access unit (AU) into image buffers (API set 0).
 *   - did  : decoder handle
 *   - bitb : the AU to decode, from its 'aPv1' signature
 *   - ofrms: output frames. The application sets up an image buffer for
 *            each frame of the AU; the PBU type and group ID of each frame
 *            are filled by the decoder.
 *   - mid  : receives the metadata of the AU, or NULL to ignore it
 *   - stat : receives the result of decoding, including the number of
 *            decoded frames
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_INVALID_ARGUMENT: also returned when 'ofrms' has fewer
 *     frames than the AU
 * NOTE: In the raw bitstream format, each AU is preceded by a 4-byte size;
 *       pass the AU after it. Use oapvd_info() to find the number of frames
 *       and the format of each before allocating the image buffers.
 */
OAPV_EXPORT int oapvd_decode(oapvd_t did, oapv_bitb_t *bitb, oapv_frms_t *ofrms, oapvm_t mid, oapvd_stat_t *stat);

// API set 1: PBU-based decoding

/*
 * Decode an AU information PBU.
 *   - did : decoder handle
 *   - bitb: the AU information PBU, from its PBU header
 *   - aui : receives the number of frames and the information of each frame
 *           of the AU
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: An AU information PBU is optional. When present, it describes the
 *       frames of the AU before they are read.
 */
OAPV_EXPORT int oapvd_decode_auinfo(oapvd_t did, oapv_bitb_t *bitb, oapv_au_info_t *aui);

/*
 * Decode a frame PBU into an image buffer.
 *   - did : decoder handle
 *   - bitb: the frame PBU, from its PBU header
 *   - imgb: output image buffer allocated by the application to match the
 *           frame
 *   - stat: receives the result of decoding
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Use oapvd_info_frame() to find the size and color space of the
 *       frame before allocating the image buffer.
 */
OAPV_EXPORT int oapvd_decode_frame(oapvd_t did, oapv_bitb_t *bitb, oapv_imgb_t *imgb, oapvd_stat_t *stat);

/*
 * Decode a metadata PBU into a metadata container.
 *   - did : decoder handle
 *   - bitb: the metadata PBU, from its PBU header
 *   - mid : container that receives the payloads under the group ID of the
 *           PBU
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_REACHED_MAX: the container is full
 */
OAPV_EXPORT int oapvd_decode_metadata(oapvd_t did, oapv_bitb_t *bitb, oapvm_t mid);

/*****************************************************************************
 * selective tile decoding
 *
 * Decodes a chosen set of tiles of one frame, each into its own buffer sized to
 * a tile instead of to the picture. 'bitb' carries a single frame PBU, as it
 * does for oapvd_decode_frame(), and oapvd_info_tile() reports the tile indices
 * and dimensions needed to plan a selection.
 *
 * oapvd_decode_frame() decodes every tile into a scanline-strided oapv_imgb_t
 * sized to the whole picture, so decoding a few tiles of a large frame still
 * has to allocate that picture.
 *****************************************************************************/

/* destination of one decoded tile. unlike oapv_imgb_t this describes a tile and
   not a picture: the tile's dimensions come from the frame header, so only the
   address and the row stride of each component are the caller's to state. */
typedef struct oapv_imgb_tile oapv_imgb_tile_t;
struct oapv_imgb_tile {
    int   cs;                 // color space
    void *a[OAPV_MAX_CC];     // address of each component
    int   s[OAPV_MAX_CC];     // buffer stride (in unit of byte)
    int   bsize[OAPV_MAX_CC]; // buffer size behind a[c], or 0 to skip the check
};

typedef struct oapv_tile_req oapv_tile_req_t;
struct oapv_tile_req {
    int              idx; // tile index in raster scan order
    oapv_imgb_tile_t imgb;
};

/*
 * Decode chosen tiles of a frame PBU, each into its own buffer.
 *   - did      : decoder handle
 *   - bitb     : the frame PBU, from its PBU header
 *   - num_tiles: number of entries of 'tile_reqs'
 *   - tile_reqs: the tiles to decode; each entry gives a tile index and the
 *                destination of that tile
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_INVALID_ARGUMENT: also returned for a tile index out of range
 *     or given twice, and for destinations with different color spaces
 * NOTE: All destinations must have the same color space. Use
 *       oapvd_info_tile() to find the tile indices and sizes.
 */
OAPV_EXPORT int oapvd_decode_tiles(oapvd_t did, oapv_bitb_t *bitb, int num_tiles, oapv_tile_req_t *tile_reqs);

/*****************************************************************************
 * utility APIs
 *
 * These functions need no encoder or decoder instance.
 *****************************************************************************/
/*
 * Get the information of an AU without decoding it.
 *   - au     : the AU, from its 'aPv1' signature
 *   - au_size: size of the AU in bytes
 *   - aui    : receives the number of frames and the size, color space and
 *              coding parameters of each frame
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Use it to allocate the output image buffers for oapvd_decode().
 */
OAPV_EXPORT int oapvd_info(void *au, int au_size, oapv_au_info_t *aui);

/*
 * Get the type and group ID of a PBU without decoding it (API set 1).
 *   - pbu     : the PBU, from its PBU header
 *   - pbu_size: size of the PBU in bytes, not counting the 4-byte pbu_size
 *               field that precedes it in the bitstream
 *   - pbu_info: receives the PBU type and group ID
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Use the PBU type to choose which function decodes the PBU.
 */
OAPV_EXPORT int oapvd_info_pbu(void *pbu, int pbu_size, oapv_pbu_info_t *pbu_info);

/*
 * Get the information of a frame PBU without decoding it.
 *   - pbu     : the frame PBU, from its PBU header
 *   - pbu_size: size of the PBU in bytes
 *   - frm_info: receives the size, color space and coding parameters of the
 *               frame
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: Use it to allocate the output image buffer for oapvd_decode_frame().
 */
OAPV_EXPORT int oapvd_info_frame(void *pbu, int pbu_size, oapv_frm_info_t *frm_info);

/*
 * Get the position and size of each tile of a frame PBU without decoding it.
 *   - pbu      : the frame PBU, from its PBU header
 *   - pbu_size : size of the PBU in bytes
 *   - pos_tiles: receives the tiles in raster scan order, or NULL to get only
 *                the number of tiles
 *   - num_tiles: on input, the number of entries of 'pos_tiles'; on output,
 *                the number of tiles of the frame
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 *   - OAPV_ERR_REACHED_MAX: 'pos_tiles' is too small; '*num_tiles' is set to
 *     the number of entries needed
 * NOTE: The byte offset and size of each tile are known only when the frame
 *       header carries the tile sizes; otherwise they are set to 0.
 */
OAPV_EXPORT int oapvd_info_tile(void *pbu, int pbu_size, oapv_tile_pos_t *pos_tiles, int *num_tiles);

/*
 * Get the target bitrate of an APV family for a resolution and frame rate.
 *   - family : OAPV_FAMILY_422_LQ, _422_SQ, _422_HQ or _444_UQ
 *   - w      : frame width in pixels
 *   - h      : frame height in pixels
 *   - fps_num: frame rate numerator
 *   - fps_den: frame rate denominator
 *   - kbps   : receives the bitrate in kbps
 * Returns OAPV_OK on success, or a negative OAPV_ERR_* code on failure.
 * NOTE: See readme/apv_family.md for the families.
 */
OAPV_EXPORT int oapve_family_bitrate(int family, int w, int h, int fps_num, int fps_den, int * kbps);

/*
 * Get the version of the library.
 *   - ver_num: if not NULL, receives the version as a number made with
 *              OAPV_VER_SET(); use OAPV_VER_GET_APISET() and the other
 *              OAPV_VER_GET_* macros to read its parts
 * Returns the version string in the form "APISET.MAJOR.MINOR.PATCH".
 * NOTE: The string is a constant of the library; do not modify or free it.
 */
OAPV_EXPORT const char *oapv_version(unsigned int *ver_num);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENAPV_OAPV_H_
