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

#include "oapv.h"
#include "oapv_app_util.h"
#include "oapv_app_args.h"
#include "oapv_app_y4m.h"

#define MAX_BS_BUF   (128 * 1024 * 1024)
#define MAX_NUM_FRMS (1)           // supports only 1-frame in an access unit
#define FRM_IDX      (0)           // supports only 1-frame in an access unit
#define MAX_NUM_CC   (OAPV_MAX_CC) // Max number of color components (upto 4:4:4:4)

typedef enum _STATES {
    STATE_ENCODING,
    STATE_SKIPPING,
    STATE_STOP
} STATES;

// clang-format off

/* define various command line options as a table */
static const args_opt_t enc_args_opts[] = {
    {
        'v',  "verbose", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "verbose (log) level\n"
        "      - 0: no message\n"
        "      - 1: only error message\n"
        "      - 2: simple messages\n"
        "      - 3: frame-level messages"
    },
    {
        'i', "input", ARGS_VAL_TYPE_STRING | ARGS_VAL_TYPE_MANDATORY, 0, NULL, 0,
        "file name of input video"
    },
    {
        'o', "output", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "file name of output bitstream"
    },
    {
        'r', "recon", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "file name of reconstructed video"
    },
    {
        'w',  "width", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "pixel width of input video"
    },
    {
        'h',  "height", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "pixel height of input video"
    },
    {
        'q',  "qp", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "QP value: 0 ~ (63 + (bitdepth - 10)*6) \n"
        "      - 10bit input: 0 ~ 63\n"
        "      - 12bit input and 12C16bit: 0 ~ 75\n"
        "      - 'auto' means that the value is internally determined"
    },
    {
        'z',  "fps", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "frame rate (frames per second)"
    },
    {
        'm',  "threads", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "force use of a specific number of threads\n"
        "      - 'auto' means that the value is internally determined"
    },
    {
        ARGS_NO_KEY,  "preset", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "encoder preset [fastest, fast, medium, slow, placebo]"
    },
    {
        'd',  "input-depth", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "input bit depth (8, 10-12, 16)\n"
        "      - Note: 8bit input will be converted to 10bit"
    },
    {
        ARGS_NO_KEY,  "input-csp", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "input color space (chroma format)\n"
        "      - 0: 400\n"
        "      - 2: 422\n"
        "      - 3: 444\n"
        "      - 4: 4444\n"
        "      - 5: P2(Planar Y, Combined CbCr, 422)"
    },
    {
        ARGS_NO_KEY,  "family", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "family name for bitrate setting\n"
        "      - 422-LQ: YCbCr422 low quality\n"
        "      - 422-SQ: YCbCr422 standard quality\n"
        "      - 422-HQ: YCbCr422 high quality\n"
        "      - 444-UQ: YCbCr444 ultra quality\n"
        "      Note: 'family' and 'bitrate' value cannot be set together.\n"
        "            The family and profile arguments should be set with the same\n"
        "            color space, if they coexists."
    },
    {
        ARGS_NO_KEY,  "profile", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "profile string\n"
        "      - 422-10: YCbCr422 10bit (default)\n"
        "      - 422-12: YCbCr422 12bit\n"
        "      - 444-10: YCbCr(RGB)444 10bit\n"
        "      - 444-12: YCbCr(RGB)444 12bit\n"
        "      - 4444-10: YCbCrX(RGBA)4444 10bit\n"
        "      - 4444-12: YCbCrX(RGBA)4444 12bit\n"
        "      - 4444-16C12: YCbCrX(RGBA)4444 16bit companded to 12bit\n"
        "      - 400-10: YCbCr400 (monochrome) 10bit\n"
        "      Note: Color space and bit depth of input video will be converted\n"
        "            automatically to support the given profile, if needs\n"
        "            The family and profile arguments should be set with the same\n"
        "            color space, if they coexists."
    },
    {
        ARGS_NO_KEY,  "level", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "level setting (1, 1.1, 2, 2.1, 3, 3.1, 4, 4.1, 5, 5.1, 6, 6.1, 7, 7.1)\n"
        "      - 'auto' means that the value is internally determined"
    },
    {
        ARGS_NO_KEY,  "band", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "band setting (0, 1, 2, 3)\n"
        "      - 'auto' means that the value is internally determined"
    },
    {
        ARGS_NO_KEY,  "max-au", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "maximum number of access units to be encoded"
    },
    {
        ARGS_NO_KEY,  "seek", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "number of skipped access units before encoding"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c1", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "QP offset value for Component 1 (Cb)"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c2", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "QP offset value for Component 2 (Cr)"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c3", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "QP offset value for Component 3"
    },
    {
        ARGS_NO_KEY,  "tile-w", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "width of tile in units of pixels"
    },
    {
        ARGS_NO_KEY,  "tile-h", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "height of tile in units of pixels"
    },
    {
        ARGS_NO_KEY,  "bitrate", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "enable ABR rate control\n"
        "      bitrate in terms of kbits per second: Kbps(none,K,k), Mbps(M,m)\n"
        "      ex) 100 = 100K = 0.1M"
    },
    {
        ARGS_NO_KEY,  "q-matrix-c0", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "custom quantization matrix for component 0 (Y) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c1", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "custom quantization matrix for component 1 (Cb) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c2", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "custom quantization matrix for component 2 (Cr) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c3", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "custom quantization matrix for component 3 \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "color-primaries", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "ColourPrimaries value defined in ITU-T H.273\n"
        "      - 1: bt709\n"
        "      - 2: unspecified\n"
        "      - 3: reserved\n"
        "      - 4: bt470m\n"
        "      - 5: bt470bg\n"
        "      - 6: smpte170m\n"
        "      - 7: smpte240m\n"
        "      - 8: film\n"
        "      - 9: bt2020\n"
        "      - 10: smpte4280\n"
        "      - 11: smpte4311\n"
        "      - 12: smpte4322\n"
        "      Note: This value should be set along with all other color aspects.\n"
        "            i.e. 'color-primaries', 'color-transfer', 'color-matrix', \n"
        "            and 'color-range' should all be set."
    },
    {
        ARGS_NO_KEY,  "color-transfer", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "TransferCharacteristics value defined in ITU-T H.273\n"
        "      - 1: bt709\n"
        "      - 2: unspecified\n"
        "      - 4: bt470m\n"
        "      - 5: bt470bg\n"
        "      - 6: smpte170m\n"
        "      - 7: smpte240m\n"
        "      - 8: linear\n"
        "      - 9: log100\n"
        "      - 10: log316\n"
        "      - 11: iec61966-2-4\n"
        "      - 12: bt1361e\n"
        "      - 13: iec61966-2-1\n"
        "      - 14: bt2020-10\n"
        "      - 15: bt2020-12\n"
        "      - 16: smpte2084\n"
        "      - 17: smpte428\n"
        "      - 18: hybrid log-gamma (HLG), arib-std-b67\n"
        "      Note: This value should be set along with all other color aspects.\n"
        "            i.e. 'color-primaries', 'color-transfer', 'color-matrix', \n"
        "            and 'color-range' should all be set."

    },
    {
        ARGS_NO_KEY,  "color-matrix", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "MatrixCoefficients value defined in ITU-T H.273\n"
        "      - 0: gbr\n"
        "      - 1: bt709\n"
        "      - 2: unspecified\n"
        "      - 4: fcc\n"
        "      - 5: bt470bg\n"
        "      - 6: smpte170m\n"
        "      - 7: smpte240m\n"
        "      - 8: ycgco\n"
        "      - 9: bt2020nc\n"
        "      - 10: bt2020c\n"
        "      - 11: smpte2085\n"
        "      - 12: chroma-derived-nc\n"
        "      - 13: chroma-derived-c\n"
        "      - 14: ictcp\n"
        "      Note: This value should be set along with all other color aspects.\n"
        "            i.e. 'color-primaries', 'color-transfer', 'color-matrix', \n"
        "            and 'color-range' should all be set."
    },
    {
        ARGS_NO_KEY,  "color-range", ARGS_VAL_TYPE_INTEGER, 0, NULL, 0,
        "Color range\n"
        "      - 0: limited color range ('tv' color range) \n"
        "      - 1: full color range ('pc' color range)\n"
        "      Note: This value should be set along with all other color aspects.\n"
        "            i.e. 'color-primaries', 'color-transfer', 'color-matrix', \n"
        "            and 'color-range' should all be set."
    },
    {
        ARGS_NO_KEY,  "hash", ARGS_VAL_TYPE_NONE, 0, NULL, 0,
        "embed frame hash value for conformance checking in decoding"
    },
    {
        ARGS_NO_KEY,  "disable-tile-size-in-fh", ARGS_VAL_TYPE_NONE, 0, NULL, 0,
        "do not write tile size values into the frame header"
    },
    {
        ARGS_NO_KEY,  "master-display", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "mastering display color volume metadata"
    },
    {
        ARGS_NO_KEY,  "max-cll", ARGS_VAL_TYPE_STRING, 0, NULL, 0,
        "content light level information metadata"
    },
    {ARGS_END_KEY, "", ARGS_VAL_TYPE_NONE, 0, NULL, 0, ""} /* termination */
};

// clang-format on

#define NUM_ARGS_OPT ((int)(sizeof(enc_args_opts) / sizeof(enc_args_opts[0])))

typedef struct args_var {
    /* variables for options */
    char           fname_inp[256];
    char           fname_out[256];
    char           fname_rec[256];
    int            max_au;
    int            hash;
    int            disable_tile_size_in_fh;
    int            input_depth;
    int            input_csp;
    int            seek;
    char           threads[16];

    char           profile[16];
    char           level[16];
    char           band[16];

    char           width[16];
    char           height[16];
    char           fps[24];

    char           qp[16];
    char           qp_offset_c1[16];
    char           qp_offset_c2[16];
    char           qp_offset_c3[16];
    char           family[16];
    char           bitrate[32];

    char           preset[16];

    char           q_matrix_c0[512]; // raster-scan order
    char           q_matrix_c1[512]; // raster-scan order
    char           q_matrix_c2[512]; // raster-scan order
    char           q_matrix_c3[512]; // raster-scan order
    char           tile_w[16];
    char           tile_h[16];

    int            color_primaries;
    int            color_transfer;
    int            color_matrix;
    int            color_range;

    char           master_display[512];
    char           max_cll[64];

    oapve_param_t *param;
} args_var_t;

static args_var_t *args_init_vars(args_parser_t *args, oapve_param_t *param)
{
    args_opt_t *opts;
    args_var_t *vars;

    opts = args->opts;
    vars = malloc(sizeof(args_var_t));
    assert_rv(vars != NULL, NULL);
    memset(vars, 0, sizeof(args_var_t));

    vars->param = param;

    /*args_set_variable_by_key_long(opts, "config", args->fname_cfg, 0);*/
    args_set_variable_by_key_long(opts, "input", vars->fname_inp, sizeof(vars->fname_inp));
    args_set_variable_by_key_long(opts, "output", vars->fname_out, sizeof(vars->fname_out));
    args_set_variable_by_key_long(opts, "recon", vars->fname_rec, sizeof(vars->fname_rec));
    args_set_variable_by_key_long(opts, "max-au", &vars->max_au, 0);
    args_set_variable_by_key_long(opts, "hash", &vars->hash, 0);
    args_set_variable_by_key_long(opts, "disable-tile-size-in-fh", &vars->disable_tile_size_in_fh, 0);
    args_set_variable_by_key_long(opts, "verbose", &op_verbose, 0);
    op_verbose = VERBOSE_SIMPLE; /* default */
    args_set_variable_by_key_long(opts, "input-depth", &vars->input_depth, 0);
    vars->input_depth = 10; /* default */
    args_set_variable_by_key_long(opts, "input-csp", &vars->input_csp, 0);
    vars->input_csp = -1;
    args_set_variable_by_key_long(opts, "seek", &vars->seek, 0);
    args_set_variable_by_key_long(opts, "profile", vars->profile, sizeof(vars->profile));
    strcpy(vars->profile, "422-10");
    args_set_variable_by_key_long(opts, "level", vars->level, sizeof(vars->level));
    strcpy(vars->level, "auto"); /* default */
    args_set_variable_by_key_long(opts, "band", vars->band, sizeof(vars->band));
    strcpy(vars->band, "auto"); /* default */

    args_set_variable_by_key_long(opts, "width", vars->width, sizeof(vars->width));
    args_set_variable_by_key_long(opts, "height", vars->height, sizeof(vars->height));
    args_set_variable_by_key_long(opts, "fps", vars->fps, sizeof(vars->fps));

    args_set_variable_by_key_long(opts, "qp", vars->qp, sizeof(vars->qp));
    strcpy(vars->qp, "auto"); /* default */
    args_set_variable_by_key_long(opts, "qp_offset_c1", vars->qp_offset_c1, sizeof(vars->qp_offset_c1));
    args_set_variable_by_key_long(opts, "qp_offset_c2", vars->qp_offset_c2, sizeof(vars->qp_offset_c2));
    args_set_variable_by_key_long(opts, "qp_offset_c3", vars->qp_offset_c3, sizeof(vars->qp_offset_c3));

    args_set_variable_by_key_long(opts, "family", vars->family, sizeof(vars->family));
    args_set_variable_by_key_long(opts, "bitrate", vars->bitrate, sizeof(vars->bitrate));

    args_set_variable_by_key_long(opts, "q-matrix-c0", vars->q_matrix_c0, sizeof(vars->q_matrix_c0));
    args_set_variable_by_key_long(opts, "q-matrix-c1", vars->q_matrix_c1, sizeof(vars->q_matrix_c1));
    args_set_variable_by_key_long(opts, "q-matrix-c2", vars->q_matrix_c2, sizeof(vars->q_matrix_c2));
    args_set_variable_by_key_long(opts, "q-matrix-c3", vars->q_matrix_c3, sizeof(vars->q_matrix_c3));

    args_set_variable_by_key_long(opts, "threads", vars->threads, sizeof(vars->threads));
    strcpy(vars->threads, "auto");

    args_set_variable_by_key_long(opts, "tile-w", vars->tile_w, sizeof(vars->tile_w));
    args_set_variable_by_key_long(opts, "tile-h", vars->tile_h, sizeof(vars->tile_h));

    args_set_variable_by_key_long(opts, "preset", vars->preset, sizeof(vars->preset));

    args_set_variable_by_key_long(opts, "color-primaries", &vars->color_primaries, 0);
    vars->color_primaries = -1; /* unset */
    args_set_variable_by_key_long(opts, "color-transfer", &vars->color_transfer, 0);
    vars->color_transfer = -1; /* unset */
    args_set_variable_by_key_long(opts, "color-matrix", &vars->color_matrix, 0);
    vars->color_matrix = -1; /* unset */
    args_set_variable_by_key_long(opts, "color-range", &vars->color_range, 0);
    vars->color_range = -1; /* unset */

    args_set_variable_by_key_long(opts, "master-display", vars->master_display, sizeof(vars->master_display));
    args_set_variable_by_key_long(opts, "max-cll", vars->max_cll, sizeof(vars->max_cll));

    return vars;
}

static void print_usage(const char **argv)
{
    int            i;
    char           str[ARGS_HELP_BUF_SIZE];
    args_parser_t *args;
    args_var_t    *args_var = NULL;
    oapve_param_t  default_param;

    oapve_param_default(&default_param);
    args = args_create(enc_args_opts, NUM_ARGS_OPT);
    if(args == NULL)
        goto ERR;
    args_var = args_init_vars(args, &default_param);
    if(args_var == NULL)
        goto ERR;

    logv2("Syntax: \n");
    logv2("  %s -i 'input-file' [ options ] \n\n", argv[0]);

    logv2("Options:\n");
    logv2("  --help\n    : list options\n");
    for(i = 0; i < args->num_option; i++) {
        if(args->get_help(args, i, str) < 0)
            return;
        logv2("%s\n", str);
    }

ERR:
    if(args)
        args->release(args);
    if(args_var)
        free(args_var);
}

static int get_val_from_key(const oapv_dict_str_int_t * dict, const char * key)
{
    while(strlen(dict->key) > 0) {
        if(strcmp(dict->key, key) == 0){
            return dict->val;
        }
        dict++;
    }
    return -1;
}

static const oapv_dict_str_int_t opts_family[] = {
    {"422-LQ",    OAPV_FAMILY_422_LQ},
    {"422-SQ",    OAPV_FAMILY_422_SQ},
    {"422-HQ",    OAPV_FAMILY_422_HQ},
    {"444-UQ",    OAPV_FAMILY_444_UQ},
    {"", 0} // termination
};

static int check_conf(oapve_cdesc_t *cdesc, args_var_t *vars)
{
    int i;
    for(i = 0; i < cdesc->max_num_frms; i++) {
        // ensure frame width multiple of 2 in case of 422 format
        if ((vars->input_csp == 2) && (cdesc->param[i].w & 0x1)) {
            logerr("ERR: %d-th frame's width should be a multiple of 2 for '--input-csp 2'\n", i);
            return -1;
        }
        if(vars->hash && strlen(vars->fname_rec) == 0) {
            logerr("ERR: cannot use frame hash without reconstructed picture option!\n");
            return -1;
        }
    }
    if(strlen(vars->family) > 0) {
        int f = get_val_from_key(opts_family, vars->family);
        if(f < 0) {
            logerr("ERR: invalid family (%s)\n", vars->family);
            return -1;
        }
        int p = get_val_from_key(oapv_param_opts_profile, vars->profile);
        if(p < 0) {
            logerr("ERR: invalid profile (%s)\n", vars->profile);
            return -1;
        }

        switch(f) {
        case OAPV_FAMILY_422_LQ:
        case OAPV_FAMILY_422_SQ:
        case OAPV_FAMILY_422_HQ:
            if(p != OAPV_PROFILE_422_10) {
                logerr("ERR: 'family(%s)' and 'profile(%s)' value are unmatched.\n", vars->family, vars->profile);
                return -1;
            }
            break;
        case OAPV_FAMILY_444_UQ:
            if(p != OAPV_PROFILE_444_10) {
                logerr("ERR: 'family(%s)' and 'profile(%s)' value are unmatched.\n", vars->family, vars->profile);
                return -1;
            }
            break;
        default:
            logerr("ERR: invalid family (%s)\n", vars->family);
            return -1;
        }
    }
    return 0;
}

static int set_extra_config(oapve_t id, args_var_t *vars, oapve_param_t *param)
{
    int ret = 0, size, value;

    if(vars->hash) {
        value = 1;
        size = 4;
        ret = oapve_config(id, OAPV_CFG_FRM(OAPV_CFG_SET_USE_FRM_HASH, FRM_IDX), &value, &size);
        if(OAPV_FAILED(ret)) {
            logerr("ERR: failed to set config for using frame hash\n");
            return -1;
        }
    }
    if(vars->disable_tile_size_in_fh) {
        value = 0;
        size = 4;
        ret = oapve_config(id, OAPV_CFG_FRM(OAPV_CFG_SET_TILE_SIZE_IN_FH, FRM_IDX), &value, &size);
        if(OAPV_FAILED(ret)) {
            logerr("ERR: failed to set config for tile size in frame header\n");
            return -1;
        }
    }
    return ret;
}

static int write_rec_img(char *fname, oapv_imgb_t *img, int flag_y4m)
{
    if(flag_y4m) {
        if(write_y4m_frame_header(fname))
            return -1;
    }
    if(imgb_write(fname, img))
        return -1;
    return 0;
}

static void print_commandline(int argc, const char **argv)
{
    int i;
    if(op_verbose < VERBOSE_FRAME)
        return;

    logv3("Command line: ");
    for(i = 0; i < argc; i++) {
        logv3("%s ", argv[i]);
    }
    logv3("\n\n");
}

static void add_thousands_comma_to_number(char *in, char *out)
{
    int len, left = 0;
    len = strlen(in);
    left = len % 3;

    while(len > 0) {
        *out = *in;

        out++; in++;

        left--;
        len--;

        if(left == 0 && len >= 3) {
            *out = ',';
            out++;
            left = 3;
        }
    }
    *out='\0';
}

static void print_config(args_var_t *vars, oapve_param_t *param)
{
    if(op_verbose < VERBOSE_FRAME)
        return;

    logv3_line("Configurations");
    logv3("Input sequence : %s \n", vars->fname_inp);
    if(strlen(vars->fname_out) > 0) {
        logv3("Output bitstream : %s \n", vars->fname_out);
    }
    if(strlen(vars->fname_rec) > 0) {
        logv3("Reconstructed sequence : %s \n", vars->fname_rec);
    }
    logv3("    profile             = %s\n", vars->profile);
    logv3("    level               = %s\n", vars->level);
    logv3("    band                = %s\n", vars->band);
    logv3("    width               = %d\n", param->w);
    logv3("    height              = %d\n", param->h);
    logv3("    fps                 = %.2f\n", (float)param->fps_num / param->fps_den);
    logv3("    rate control type   = %s\n", (param->rc_type == OAPV_RC_ABR) ? "average bitrate" : "constant qp");
    if(strlen(vars->family) > 0) {
        logv3("    family              = %s\n", vars->family);
    }
    if(param->rc_type == OAPV_RC_CQP){
        logv3("    qp                  = %d\n", param->qp);
    }
    else if(param->rc_type == OAPV_RC_ABR) {
        //add_thousands_comma_to_number(vars->bitrate, tstr);
        logv3("    target bitrate      = %s\n", vars->bitrate);
    }
    logv3("    max number of AUs   = %d\n", vars->max_au);
    logv3("    tile size           = %d x %d\n", param->tile_w, param->tile_h);
}

static void print_stat_au(oapve_stat_t *stat, int au_cnt, oapve_param_t *param, int max_au, double bitrate_tot, oapv_clk_t clk_au, oapv_clk_t clk_tot)
{
    if(op_verbose >= VERBOSE_FRAME) {
        logv3_line("");
        logv3("AU %-5d  %10d-bytes  %3d-frame(s) %10d msec\n", au_cnt, stat->write, stat->aui.num_frms, oapv_clk_msec(clk_au));
    }
    else {
        int total_time = ((int)oapv_clk_msec(clk_tot) / 1000);
        int h = total_time / 3600;
        total_time = total_time % 3600;
        int m = total_time / 60;
        total_time = total_time % 60;
        int    s = total_time;
        double curr_bitrate = bitrate_tot;
        curr_bitrate *= (((float)param->fps_num / param->fps_den) * 8);
        curr_bitrate /= (au_cnt + 1);
        curr_bitrate /= 1000;
        logv2("[ %d / %d AU(s) ] [ %.2f AU/sec ] [ %.4f kbps ] [ %2dh %2dm %2ds ] \r",
              au_cnt, max_au, ((float)(au_cnt + 1) * 1000) / ((float)oapv_clk_msec(clk_tot)), curr_bitrate, h, m, s);
        fflush(stdout);
    }
}

static void print_stat_frms(oapve_stat_t *stat, oapv_frms_t *ifrms, oapv_frms_t *rfrms, double psnr_avg[MAX_NUM_FRMS][MAX_NUM_CC])
{
    int              i, j, cfmt;
    oapv_frm_info_t *finfo;
    double           psnr[MAX_NUM_FRMS][MAX_NUM_CC] = { 0 };

    assert(stat->aui.num_frms == ifrms->num_frms);
    assert(stat->aui.num_frms <= MAX_NUM_FRMS);

    // calculate PSNRs
    if(rfrms != NULL) {
        for(i = 0; i < stat->aui.num_frms; i++) {
            if(rfrms->frm[i].imgb) {
                measure_psnr(ifrms->frm[i].imgb, rfrms->frm[i].imgb, psnr[i], OAPV_CS_GET_BIT_DEPTH(ifrms->frm[i].imgb->cs));
                for(j = 0; j < MAX_NUM_CC; j++) {
                    psnr_avg[i][j] += psnr[i][j];
                }
            }
        }
    }
    // print verbose messages
    if(op_verbose < VERBOSE_FRAME)
        return;

    finfo = stat->aui.frm_info;

    for(i = 0; i < stat->aui.num_frms; i++) {
        // clang-format off
        const char* str_frm_type = finfo[i].pbu_type == OAPV_PBU_TYPE_PRIMARY_FRAME ? "PRIMARY"
                                 : finfo[i].pbu_type == OAPV_PBU_TYPE_NON_PRIMARY_FRAME ? "NON-PRIMARY"
                                 : finfo[i].pbu_type == OAPV_PBU_TYPE_PREVIEW_FRAME ? "PREVIEW"
                                 : finfo[i].pbu_type == OAPV_PBU_TYPE_DEPTH_FRAME ? "DEPTH"
                                 : finfo[i].pbu_type == OAPV_PBU_TYPE_ALPHA_FRAME ? "ALPHA"
                                 : "UNKNOWN";

        cfmt = OAPV_CS_GET_FORMAT(finfo[i].cs);

        // clang-format on
        if (cfmt == OAPV_CF_YCBCR400) { // 1 channel
            logv3("- FRM %-2d GID %-5d %-11s %9d-bytes %8.4fdB\n",
                i, finfo[i].group_id, str_frm_type, stat->frm_size[i], psnr[i][0]);
        }
        else if (cfmt == OAPV_CF_YCBCR4444) { // 4 channels
            logv3("- FRM %-2d GID %-5d %-11s %9d-bytes %8.4fdB %8.4fdB %8.4fdB %8.4fdB\n",
                i, finfo[i].group_id, str_frm_type, stat->frm_size[i], psnr[i][0], psnr[i][1], psnr[i][2], psnr[i][3]);
        }
        else { // 3 channels
            logv3("- FRM %-2d GID %-5d %-11s %9d-bytes %8.4fdB %8.4fdB %8.4fdB\n",
                i, finfo[i].group_id, str_frm_type, stat->frm_size[i], psnr[i][0], psnr[i][1], psnr[i][2]);
        }
    }
    fflush(stdout);
    fflush(stderr);
}

static int family_to_bitrate(char * family, oapve_param_t *param)
{
    int ret, kbps;
    int fn = get_val_from_key(opts_family, family);
    if(fn < 0) {
        logerr("ERR: invalid family value (%s)\n", family);
        return -1;
    }
    ret = oapve_family_bitrate(fn, param->w, param->h, param->fps_num, param->fps_den, &kbps);
    if(OAPV_FAILED(ret)) {
        return -1;
    }
    return kbps;
}

#define UPDATE_A_PARAM_W_KEY_VAL(param, key, val) \
    if(strlen(val) > 0) { \
        if(OAPV_FAILED(oapve_param_parse(param, key, val))) { \
            logerr("ERR: input value (%s) of %s is invalid\n", val, key); \
            return -1; \
        } \
    }

static int update_param(args_var_t *vars, oapve_param_t *param)
{
    UPDATE_A_PARAM_W_KEY_VAL(param, "profile", vars->profile);
    UPDATE_A_PARAM_W_KEY_VAL(param, "level", vars->level);
    UPDATE_A_PARAM_W_KEY_VAL(param, "band", vars->band);

    UPDATE_A_PARAM_W_KEY_VAL(param, "width", vars->width);
    UPDATE_A_PARAM_W_KEY_VAL(param, "height", vars->height);
    if(param->w <= 0 || param->w > IMGB_MAX_W || param->h <= 0 || param->h > IMGB_MAX_H) {
        logerr("ERR: frame dimensions %dx%d out of range (max %dx%d)\n",
               param->w, param->h, IMGB_MAX_W, IMGB_MAX_H);
        return -1;
    }
    UPDATE_A_PARAM_W_KEY_VAL(param, "fps", vars->fps);

    UPDATE_A_PARAM_W_KEY_VAL(param, "qp", vars->qp);
    UPDATE_A_PARAM_W_KEY_VAL(param, "qp-offset-c1", vars->qp_offset_c1);
    UPDATE_A_PARAM_W_KEY_VAL(param, "qp-offset-c2", vars->qp_offset_c2);
    UPDATE_A_PARAM_W_KEY_VAL(param, "qp-offset-c3", vars->qp_offset_c3);

    if(strlen(vars->family) > 0) {
        if(strlen(vars->bitrate) > 0) {
            logerr("ERR: 'family' and 'bitrate' value cannot be set together.\n");
            return -1;
        }
        int kbps = family_to_bitrate(vars->family, param);
        if(kbps < 0) {
            logerr("ERR: failed to get target bitrate from family value\n");
            return -1;
        }
        sprintf(vars->bitrate, "%d", kbps);
    }
    UPDATE_A_PARAM_W_KEY_VAL(param, "bitrate", vars->bitrate);

    UPDATE_A_PARAM_W_KEY_VAL(param, "preset", vars->preset);

    UPDATE_A_PARAM_W_KEY_VAL(param, "q-matrix-c0", vars->q_matrix_c0);
    UPDATE_A_PARAM_W_KEY_VAL(param, "q-matrix-c1", vars->q_matrix_c1);
    UPDATE_A_PARAM_W_KEY_VAL(param, "q-matrix-c2", vars->q_matrix_c2);
    UPDATE_A_PARAM_W_KEY_VAL(param, "q-matrix-c3", vars->q_matrix_c3);

    // check color aspects
    if(vars->color_primaries >= 0 || vars->color_transfer >= 0 ||
        vars->color_matrix >= 0 || vars->color_range >= 0) {
        // need to check all values are set
        if(vars->color_primaries < 0 || vars->color_transfer < 0 ||
            vars->color_matrix < 0 || vars->color_range < 0) {
            logerr("ERR: 'color-primaries', 'color-transfer', 'color-matrix', and 'color-range' should all be set.\n");
            return -1;
        }
        param->color_primaries = vars->color_primaries;
        param->transfer_characteristics = vars->color_transfer;
        param->matrix_coefficients = vars->color_matrix;
        param->full_range_flag = vars->color_range;
        param->color_description_present_flag = 1;
    }

    UPDATE_A_PARAM_W_KEY_VAL(param, "tile-w", vars->tile_w);
    UPDATE_A_PARAM_W_KEY_VAL(param, "tile-h", vars->tile_h);
    return 0;
}

static int parse_master_display(const char* data_string, oapvm_payload_mdcv_t *mdcv)
{
    int assigned_fields = sscanf(data_string,
        "G(%u,%u)B(%u,%u)R(%u,%u)WP(%u,%u)L(%lu,%lu)",
        &mdcv->primary_chromaticity_x[1], &mdcv->primary_chromaticity_y[1], // G
        &mdcv->primary_chromaticity_x[2], &mdcv->primary_chromaticity_y[2], // B
        &mdcv->primary_chromaticity_x[0], &mdcv->primary_chromaticity_y[0], // R
        &mdcv->white_point_chromaticity_x, &mdcv->white_point_chromaticity_y, // White Point
        &mdcv->max_mastering_luminance, &mdcv->min_mastering_luminance       // Luminance
    );

    // Check if sscanf successfully assigned all expected fields (10 numerical values).
    const int expected_fields = 10;
    if (assigned_fields != expected_fields) {
        logerr("ERR: Parsing error: master display color volume information");
        return -1;
    }
    return 0; // Success
}

static int parse_max_cll(const char* data_string, oapvm_payload_cll_t *cll)
{
    int assigned_fields = sscanf(data_string,
        "%u,%u",
        &cll->max_cll, &cll->max_fall
    );

    // Check if sscanf successfully assigned all expected fields (2 numerical values).
    const int expected_fields = 2;
    if (assigned_fields != expected_fields) {
        logerr("ERR: parsing error: content light level information");
        return -1;
    }
    return 0; // Success
}

static int update_metadata(args_var_t *vars, oapvm_t mid)
{
    int ret = 0, size;
    oapvm_payload_mdcv_t mdcv;
    oapvm_payload_cll_t cll;
    int is_mdcv, is_cll;
    unsigned char payload[64];

    is_mdcv = (strlen(vars->master_display) > 0)? 1: 0;
    is_cll = (strlen(vars->max_cll) > 0)? 1: 0;

    if(!is_mdcv && !is_cll) {
        // no need to add metadata payload
        return 0;
    }

    if(is_mdcv) {
        if(parse_master_display(vars->master_display, &mdcv)) {
            logerr("ERR: cannot parse master display information");
            ret = -1;
            goto ERR;
        }
        if(OAPV_FAILED(oapvm_write_mdcv(&mdcv, payload, &size))) {
            logerr("ERR: cannot get master display information bitstream");
            ret = -1;
            goto ERR;
        }
        if(OAPV_FAILED(oapvm_set(mid, 1, OAPV_METADATA_MDCV, payload, size))) {
            logerr("ERR: cannot set master display information to handler");
            ret = -1;
            goto ERR;
        }
    }

    if(is_cll) {
        if(parse_max_cll(vars->max_cll, &cll)) {
            logerr("ERR: cannot parse contents light level information");
            ret = -1;
            goto ERR;
        }
        if(OAPV_FAILED(oapvm_write_cll(&cll, payload, &size))) {
            logerr("ERR: cannot get contents light level information bitstream");
            ret = -1;
            goto ERR;
        }
        if(OAPV_FAILED(oapvm_set(mid, 1, OAPV_METADATA_CLL, payload, size))) {
            logerr("ERR: cannot set contents light level information to handler");
            ret = -1;
            goto ERR;
        }
    }

ERR:
    return ret;
}

int main(int argc, const char **argv)
{
    args_parser_t *args = NULL;
    args_var_t    *args_var = NULL;
    STATES         state = STATE_ENCODING;
    unsigned char *bs_buf = NULL;
    FILE          *fp_inp = NULL;
    oapve_t        id = NULL;
    oapvm_t        mid = NULL;
    oapve_cdesc_t  cdesc;
    oapvm_cdesc_t  mdesc;
    oapve_param_t *param = NULL;
    oapv_bitb_t    bitb;
    oapve_stat_t   stat;
    oapv_imgb_t   *imgb_r = NULL; // image buffer for read
    oapv_imgb_t   *imgb_w = NULL; // image buffer for write
    oapv_imgb_t   *imgb_i = NULL; // image buffer for input
    oapv_imgb_t   *imgb_o = NULL; // image buffer for output
    oapv_frms_t    ifrms = { 0 }; // frames for input
    oapv_frms_t    rfrms = { 0 }; // frames for reconstruction
    int            ret;
    oapv_clk_t     clk_beg, clk_end, clk_tot;
    oapv_mtime_t   au_cnt, au_skip;
    int            frm_cnt[MAX_NUM_FRMS] = { 0 };
    double         bitrate_tot; // total bitrate (byte)
    double         psnr_avg[MAX_NUM_FRMS][MAX_NUM_CC] = { 0 };
    int            is_inp_y4m, is_rec_y4m = 0;
    y4m_params_t   y4m;
    int            is_out = 0, is_rec = 0;
    char          *errstr = NULL;
    int            cfmt;                      // color format
    const int      num_frames = MAX_NUM_FRMS; // number of frames in an access unit

    // print logo
    logv2("  ____                ___   ___ _   __\n");
    logv2(" / __ \\___  ___ ___  / _ | / _ \\ | / / Encoder (v%s)\n", oapv_version(NULL));
    logv2("/ /_/ / _ \\/ -_) _ \\/ __ |/ ___/ |/ / \n");
    logv2("\\____/ .__/\\__/_//_/_/ |_/_/   |___/  \n");
    logv2("    /_/                               \n");
    logv2("\n");

    /* help message */
    if(argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        print_usage(argv);
        return 0;
    }

    /* set default parameters */
    memset(&cdesc, 0, sizeof(oapve_cdesc_t));
    param = &cdesc.param[FRM_IDX];
    ret = oapve_param_default(param);
    if(OAPV_FAILED(ret)) {
        logerr("ERR: cannot set default parameter\n");
        ret = -1;
        goto ERR;
    }
    /* parse command line */
    args = args_create(enc_args_opts, NUM_ARGS_OPT);
    if(args == NULL) {
        logerr("ERR: cannot create argument parser\n");
        ret = -1;
        goto ERR;
    }
    args_var = args_init_vars(args, param);
    if(args_var == NULL) {
        logerr("ERR: cannot initialize argument parser\n");
        ret = -1;
        goto ERR;
    }
    if(args->parse(args, argc, argv, &errstr)) {
        logerr("ERR: command parsing error (%s)\n", errstr);
        ret = -1;
        goto ERR;
    }
    // print command line string for information
    print_commandline(argc, argv);

    // check mandatory arguments
    if(args->check_mandatory(args, &errstr)) {
        logerr("ERR: '--%s' argument is mandatory\n", errstr);
        ret = -1;
        goto ERR;
    }

    /* try to open input file */
    fp_inp = fopen(args_var->fname_inp, "rb");
    if(fp_inp == NULL) {
        logerr("ERR: cannot open input file = (%s)\n", args_var->fname_inp);
        ret = -1;
        goto ERR;
    }

    /* y4m header parsing  */
    is_inp_y4m = y4m_test(fp_inp);
    if(is_inp_y4m) {
        if(y4m_header_parser(fp_inp, &y4m)) {
            logerr("ERR: y4m format is not supported (%s)\n", args_var->fname_inp);
            ret = -1;
            goto ERR;
        }
        y4m_update_param(args, &y4m);
        cfmt = y4m.color_format;
        // clang-format off
        args_var->input_csp = (cfmt == OAPV_CF_YCBCR400 ? 0 : \
            (cfmt == OAPV_CF_YCBCR422 ? 2 : \
            (cfmt == OAPV_CF_YCBCR444 ? 3 : \
            (cfmt == OAPV_CF_YCBCR4444 ? 4 : \
            (cfmt == OAPV_CF_PLANAR2 ? 5 : -1)))));
        // clang-format on

        if(args_var->input_csp != -1) {
            // force "input-csp" argument has set
            args->set_flag(args, "input-csp", 1);
        }
    }
    else {
        // check mandatory parameters for YUV raw file.
        if(args_var->input_csp == -1) {
            logerr("ERR: set '--input-csp' argument\n");
            ret = -1;
            goto ERR;
        }
        if(strlen(args_var->width) == 0) {
            logerr("ERR: '--width' argument is required\n"); ret = -1; goto ERR;
        }
        if(strlen(args_var->height) == 0) {
            logerr("ERR: '--height' argument is required\n"); ret = -1; goto ERR;
        }
        if(strlen(args_var->fps) == 0) {
            logerr("ERR: '--fps' argument is required\n"); ret = -1; goto ERR;
        }
        // clang-format off
        cfmt = (args_var->input_csp == 0 ? OAPV_CF_YCBCR400 : \
            (args_var->input_csp == 2 ? OAPV_CF_YCBCR422 : \
            (args_var->input_csp == 3 ? OAPV_CF_YCBCR444  : \
            (args_var->input_csp == 4 ? OAPV_CF_YCBCR4444 : \
            (args_var->input_csp == 5 ? OAPV_CF_PLANAR2   : OAPV_CF_UNKNOWN)))));
        // clang-format on
    }
    if(cfmt == OAPV_CF_UNKNOWN) {
        logerr("ERR: unsupported Y4M color format\n");
        ret = -1;
        goto ERR;
    }

    /* update parameters */
    if(update_param(args_var, param)) {
        logerr("ERR: the coding parameters are not set correctly\n");
        ret = -1;
        goto ERR;
    }

    cdesc.max_bs_buf_size = MAX_BS_BUF; /* maximum bitstream buffer size */
    cdesc.max_num_frms = MAX_NUM_FRMS;
    if(!strcmp(args_var->threads, "auto")){
        cdesc.threads = OAPV_CDESC_THREADS_AUTO;
    }
    else {
        char *endptr;
        long threads_val = strtol(args_var->threads, &endptr, 10);
        if(endptr == args_var->threads || threads_val <= 0 || threads_val > INT_MAX) {
            logerr("ERR: invalid threads value: %s\n", args_var->threads);
            ret = -1;
            goto ERR;
        }
        cdesc.threads = (int)threads_val;
    }

    if(check_conf(&cdesc, args_var)) {
        logerr("ERR: invalid configuration\n");
        ret = -1;
        goto ERR;
    }

    if(strlen(args_var->fname_out) > 0) {
        clear_data(args_var->fname_out);
        is_out = 1;
    }

    if(strlen(args_var->fname_rec) > 0) {
        ret = check_file_name_type(args_var->fname_rec);
        if(ret > 0 && args_var->input_csp != 16) {
            is_rec_y4m = 1;
        }
        else if(ret == 0) {
            is_rec_y4m = 0;
        }
        else { // invalid or unknown file name type
            logerr("ERR: unknown file name type for reconstructed video\n");
            ret = -1; goto ERR;
        }
        clear_data(args_var->fname_rec);
        is_rec = 1;
    }

    /* allocate bitstream buffer */
    bs_buf = (unsigned char *)malloc(MAX_BS_BUF);
    if(bs_buf == NULL) {
        logerr("ERR: cannot allocate bitstream buffer, size=%d", MAX_BS_BUF);
        ret = -1;
        goto ERR;
    }

    /* create encoder */
    id = oapve_create(&cdesc, &ret);
    if(id == NULL) {
        logerr("ERR: cannot create OAPV encoder\n");
        ret = -1;
        goto ERR;
    }

    /* create metadata handler */
    memset(&mdesc, 0, sizeof(oapvm_cdesc_t));
    mid = oapvm_create(&mdesc, &ret);
    if(mid == NULL || OAPV_FAILED(ret)) {
        logerr("ERR: cannot create OAPV metadata handler\n");
        ret = -1;
        goto ERR;
    }

    if(set_extra_config(id, args_var, param)) {
        logerr("ERR: cannot set extra configurations\n");
        ret = -1;
        goto ERR;
    }

    print_config(args_var, param);

    bitrate_tot = 0;
    bitb.addr = bs_buf;
    bitb.bsize = MAX_BS_BUF;

    if(args_var->seek > 0) {
        state = STATE_SKIPPING;
    }

    clk_tot = 0;
    au_cnt = 0;
    au_skip = 0;

    // create input and reconstruction image buffers
    memset(&ifrms, 0, sizeof(oapv_frm_t));
    memset(&rfrms, 0, sizeof(oapv_frm_t));

    int codec_depth = (param->profile_idc == OAPV_PROFILE_422_10 ||
        param->profile_idc == OAPV_PROFILE_400_10 ||
        param->profile_idc == OAPV_PROFILE_444_10 ||
        param->profile_idc == OAPV_PROFILE_4444_10) ? 10 : (
        param->profile_idc == OAPV_PROFILE_422_12 ||
        param->profile_idc == OAPV_PROFILE_444_12 ||
        param->profile_idc == OAPV_PROFILE_4444_12) ? 12 : (
        param->profile_idc == OAPV_PROFILE_4444_16C12) ? 16 : 0;

    if (codec_depth == 0) {
        logerr("ERR: invalid profile\n");
        ret = -1;
        goto ERR;
    }

    for(int i = 0; i < num_frames; i++) {
        if(args_var->input_depth == codec_depth) {
            ifrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
        }
        else {
            if (cfmt == OAPV_CF_PLANAR2) {
                ifrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, codec_depth, 0));
            }
            else {
                imgb_r = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
                if(imgb_r == NULL) { ret = -1; goto ERR; }
                ifrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, codec_depth, 0));
            }
        }
        if(ifrms.frm[i].imgb == NULL) { ret = -1; goto ERR; }

        if(is_rec) {
            if(args_var->input_depth == codec_depth) {
                rfrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
            }
            else {
                if (cfmt == OAPV_CF_PLANAR2) {
                    rfrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, codec_depth, 0));
                }
                else
                {
                    imgb_w = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
                    if(imgb_w == NULL) { ret = -1; goto ERR; }
                    rfrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, codec_depth, 0));
                }
            }
            if(rfrms.frm[i].imgb == NULL) { ret = -1; goto ERR; }
            rfrms.num_frms++;
        }
        ifrms.num_frms++;
    }

    /* ready metadata if needs */
    if(update_metadata(args_var, mid)) {
        logerr("ERR: failed to update metadata");
        ret = -1;
        goto ERR;
    }

    /* encode pictures *******************************************************/
    while(args_var->max_au == 0 || (au_cnt < args_var->max_au)) {
        for(int i = 0; i < num_frames; i++) {
            if(args_var->input_depth == codec_depth || cfmt == OAPV_CF_PLANAR2) {
                imgb_i = ifrms.frm[i].imgb;
            }
            else {
                imgb_i = imgb_r;
            }
            ret = imgb_read(fp_inp, imgb_i, param->w, param->h, is_inp_y4m);
            if(ret < 0) {
                logv3("reached out the end of input file\n");
                ret = OAPV_OK;
                state = STATE_STOP;
                break;
            }
            if(args_var->input_depth != codec_depth && cfmt != OAPV_CF_PLANAR2) {
                imgb_cpy(ifrms.frm[i].imgb, imgb_i);
            }
            ifrms.frm[i].group_id = 1; // FIX-ME : need to set properly in case of multi-frame
            ifrms.frm[i].pbu_type = OAPV_PBU_TYPE_PRIMARY_FRAME;
        }

        if(state == STATE_ENCODING) {
            /* encoding */
            clk_beg = oapv_clk_get();

            ret = oapve_encode(id, &ifrms, mid, &bitb, &stat, &rfrms);

            clk_end = oapv_clk_from(clk_beg);
            clk_tot += clk_end;

            if(OAPV_FAILED(ret)) {
                logerr("ERR: failed to encode (return: %d)\n", ret);
                goto ERR;
            }

            bitrate_tot += stat.frm_size[FRM_IDX];

            print_stat_au(&stat, au_cnt, param, args_var->max_au, bitrate_tot, clk_end, clk_tot);

            for(int fidx = 0; fidx < num_frames; fidx++) {
                if(is_rec) {
                    if(args_var->input_depth != codec_depth && cfmt != OAPV_CF_PLANAR2) {
                        imgb_cpy(imgb_w, rfrms.frm[fidx].imgb);
                        imgb_o = imgb_w;
                    }
                    else {
                        imgb_o = rfrms.frm[fidx].imgb;
                    }
                }

                /* store bitstream */
                if(OAPV_SUCCEEDED(ret)) {
                    if(is_out && stat.write > 0) {
                        if(write_data(args_var->fname_out, bs_buf, stat.write)) {
                            logerr("ERR: cannot write bitstream\n");
                            ret = -1;
                            goto ERR;
                        }
                    }
                }
                else {
                    logerr("ERR: failed to encode\n");
                    ret = -1;
                    goto ERR;
                }

                // store recon image
                if(is_rec) {
                    if(frm_cnt[fidx] == 0 && is_rec_y4m) {
                        if(write_y4m_header(args_var->fname_rec, imgb_o)) {
                            logerr("ERR: cannot write Y4M header\n");
                            ret = -1;
                            goto ERR;
                        }
                    }
                    if(write_rec_img(args_var->fname_rec, imgb_o, is_rec_y4m)) {
                        logerr("ERR: cannot write reconstructed video\n");
                        ret = -1;
                        goto ERR;
                    }
                }
                print_stat_frms(&stat, &ifrms, &rfrms, psnr_avg);
                frm_cnt[fidx] += 1;
            }
            au_cnt++;
        }
        else if(state == STATE_SKIPPING) {
            if(au_skip < args_var->seek) {
                au_skip++;
                continue;
            }
            else {
                state = STATE_ENCODING;
            }
        }
        else if(state == STATE_STOP) {
            break;
        }
        oapvm_rem_all(mid);
    }

    logv2_line("Summary");
    psnr_avg[FRM_IDX][0] /= au_cnt;
    if (cfmt != OAPV_CF_YCBCR400) {
        psnr_avg[FRM_IDX][1] /= au_cnt;
        psnr_avg[FRM_IDX][2] /= au_cnt;
        if (cfmt == OAPV_CF_YCBCR4444) {
            psnr_avg[FRM_IDX][3] /= au_cnt;
        }
    }

    logv3("  PSNR Y(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][0]);
    if (cfmt != OAPV_CF_YCBCR400) {
        logv3("  PSNR U(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][1]);
        logv3("  PSNR V(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][2]);
        if (cfmt == OAPV_CF_YCBCR4444) {
            logv3("  PSNR T(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][3]);
        }
    }
    logv3("  Total bits(bits) : %.0f\n", bitrate_tot * 8);
    bitrate_tot *= (((float)param->fps_num / param->fps_den) * 8);
    bitrate_tot /= au_cnt;
    bitrate_tot /= 1000;


    if (cfmt == OAPV_CF_YCBCR400) { // 1-channel
        logv3("  -----------------: bitrate(kbps)\tPSNR-Y\n");
        logv3("  Summary          : %-4.4f\t%-5.4f\n",
            bitrate_tot, psnr_avg[FRM_IDX][0]);
    }
    else if(cfmt == OAPV_CF_YCBCR4444) { // 4-channel
        logv3("  -----------------: bitrate(kbps)\tPSNR-Y\tPSNR-U\tPSNR-V\tPSNR-X\n");
        logv3("  Summary          : %-4.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\n",
              bitrate_tot, psnr_avg[FRM_IDX][0], psnr_avg[FRM_IDX][1], psnr_avg[FRM_IDX][2], psnr_avg[FRM_IDX][3]);
    }
    else { // 3-channel
        logv3("  -----------------: bitrate(kbps)\tPSNR-Y\tPSNR-U\tPSNR-V\n");
        logv3("  Summary          : %-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\n",
              bitrate_tot, psnr_avg[FRM_IDX][0], psnr_avg[FRM_IDX][1], psnr_avg[FRM_IDX][2]);
    }

    logv2("Bitrate                           = %.4f kbps\n", bitrate_tot);
    logv2("Encoded frame count               = %d\n", (int)au_cnt);
    logv2("Total encoding time               = %.3f msec,",
          (float)oapv_clk_msec(clk_tot));
    logv2(" %.3f sec\n", (float)(oapv_clk_msec(clk_tot) / 1000.0));

    logv2("Average encoding time for a frame = %.3f msec\n",
          (float)oapv_clk_msec(clk_tot) / au_cnt);
    logv2("Average encoding speed            = %.3f frames/sec\n",
          ((float)au_cnt * 1000) / ((float)oapv_clk_msec(clk_tot)));
    logv2_line(NULL);

    if(args_var->max_au > 0 && au_cnt != args_var->max_au) {
        logv3("Wrong frames count: should be %d was %d\n", args_var->max_au, (int)au_cnt);
    }
ERR:

    if(imgb_r != NULL)
        imgb_r->release(imgb_r);
    if(imgb_w != NULL)
        imgb_w->release(imgb_w);

    for(int i = 0; i < num_frames; i++) {
        if(ifrms.frm[i].imgb != NULL) {
            ifrms.frm[i].imgb->release(ifrms.frm[i].imgb);
        }
    }
    for(int i = 0; i < num_frames; i++) {
        if(rfrms.frm[i].imgb != NULL) {
            rfrms.frm[i].imgb->release(rfrms.frm[i].imgb);
        }
    }

    if(id)
        oapve_delete(id);
    if(mid)
        oapvm_delete(mid);
    if(fp_inp)
        fclose(fp_inp);
    if(bs_buf)
        free(bs_buf); /* release bitstream buffer */
    if(args)
        args->release(args);
    if(args_var)
        free(args_var);

    return ret;
}
