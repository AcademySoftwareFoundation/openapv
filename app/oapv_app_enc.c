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
#define MAX_NUM_CC   (OAPV_MAX_CC) // Max number of color componets (upto 4:4:4:4)

typedef enum _STATES {
    STATE_ENCODING,
    STATE_SKIPPING,
    STATE_STOP
} STATES;

// clang-format off

/* define various command line options as a table */
static const args_opt_t enc_args_opts[] = {
    {
        'v',  "verbose", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "verbose (log) level\n"
        "      - 0: no message\n"
        "      - 1: only error message\n"
        "      - 2: simple messages\n"
        "      - 3: frame-level messages"
    },
    {
        'i', "input", ARGS_VAL_TYPE_STRING | ARGS_VAL_TYPE_MANDATORY, 0, NULL,
        "file name of input video"
    },
    {
        'o', "output", ARGS_VAL_TYPE_STRING, 0, NULL,
        "file name of output bitstream"
    },
    {
        'r', "recon", ARGS_VAL_TYPE_STRING, 0, NULL,
        "file name of reconstructed video"
    },
    {
        'w',  "width", ARGS_VAL_TYPE_INTEGER | ARGS_VAL_TYPE_MANDATORY, 0, NULL,
        "pixel width of input video"
    },
    {
        'h',  "height", ARGS_VAL_TYPE_INTEGER | ARGS_VAL_TYPE_MANDATORY, 0, NULL,
        "pixel height of input video"
    },
    {
        'q',  "qp", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "QP value: 0 ~ (63 + (bitdepth - 10)*6) \n"
        "      - 10bit input: 0 ~ 63\n"
        "      - 12bit input: 0 ~ 75\n"
    },
    {
        'z',  "fps", ARGS_VAL_TYPE_STRING | ARGS_VAL_TYPE_MANDATORY, 0, NULL,
        "frame rate (frame per second))"
    },
    {
        'm',  "threads", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "force to use a specific number of threads"
    },
    {
        ARGS_NO_KEY,  "preset", ARGS_VAL_TYPE_STRING, 0, NULL,
        "encoder preset [fastest, fast, medium, slow, placebo]"
    },
    {
        'd',  "input-depth", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "input bit depth (8, 10-12)\n"
        "      - Note: 8bit input will be converted to 10bit"
    },
    {
        ARGS_NO_KEY,  "input-csp", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "input color space (chroma format)\n"
        "      - 0: 400\n"
        "      - 2: 422\n"
        "      - 3: 444\n"
        "      - 4: 4444\n"
        "      - 5: P2(Planar Y, Combined CbCr, 422)"
    },
    {
        ARGS_NO_KEY,  "profile", ARGS_VAL_TYPE_STRING, 0, NULL,
        "profile setting flag  (422-10)"
    },
    {
        ARGS_NO_KEY,  "level", ARGS_VAL_TYPE_STRING, 0, NULL,
        "level setting (1, 1.1, 2, 2.1, 3, 3.1, 4, 4.1, 5, 5.1, 6, 6.1, 7, 7.1)"
    },
    {
        ARGS_NO_KEY,  "band", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "band setting (0, 1, 2, 3)"
    },
    {
        ARGS_NO_KEY,  "max-au", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "maximum number of access units to be encoded"
    },
    {
        ARGS_NO_KEY,  "seek", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "number of skipped access units before encoding"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c1", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "QP offset value for Component 1 (Cb)"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c2", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "QP offset value for Component 2 (Cr)"
    },
    {
        ARGS_NO_KEY,  "qp-offset-c3", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "QP offset value for Component 3"
    },
    {
        ARGS_NO_KEY,  "tile-w-mb", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "width of tile in units of MBs"
    },
    {
        ARGS_NO_KEY,  "tile-h-mb", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "height of tile in units of MBs"
    },
    {
        ARGS_NO_KEY,  "bitrate", ARGS_VAL_TYPE_STRING, 0, NULL,
        "enable ABR rate control\n"
        "      bitrate in terms of kilo-bits per second: Kbps(none,K,k), Mbps(M,m)\n"
        "      ex) 100 = 100K = 0.1M"
    },
    {
        ARGS_NO_KEY,  "use-filler", ARGS_VAL_TYPE_INTEGER, 0, NULL,
        "user filler flag"
    },
    {
        ARGS_NO_KEY,  "q-matrix-c0", ARGS_VAL_TYPE_STRING, 0, NULL,
        "custom quantization matrix for component 0 (Y) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c1", ARGS_VAL_TYPE_STRING, 0, NULL,
        "custom quantization matrix for component 1 (Cb) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c2", ARGS_VAL_TYPE_STRING, 0, NULL,
        "custom quantization matrix for component 2 (Cr) \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "q-matrix-c3", ARGS_VAL_TYPE_STRING, 0, NULL,
        "custom quantization matrix for component 3 \"q1 q2 ... q63 q64\""
    },
    {
        ARGS_NO_KEY,  "hash", ARGS_VAL_TYPE_NONE, 0, NULL,
        "embed frame hash value for conformance checking in decoding"
    },
    {ARGS_END_KEY, "", ARGS_VAL_TYPE_NONE, 0, NULL, ""} /* termination */
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
    int            input_depth;
    int            input_csp;
    int            seek;
    int            threads;
    char           profile[32];
    char           level[32];
    int            band;
    char           bitrate[64];
    char           fps[256];
    char           q_matrix[OAPV_MAX_CC][512]; // raster-scan order
    char           preset[32];
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

    /*args_set_variable_by_key_long(opts, "config", args->fname_cfg);*/
    args_set_variable_by_key_long(opts, "input", vars->fname_inp);
    args_set_variable_by_key_long(opts, "output", vars->fname_out);
    args_set_variable_by_key_long(opts, "recon", vars->fname_rec);
    args_set_variable_by_key_long(opts, "max-au", &vars->max_au);
    args_set_variable_by_key_long(opts, "hash", &vars->hash);
    args_set_variable_by_key_long(opts, "verbose", &op_verbose);
    op_verbose = VERBOSE_SIMPLE; /* default */
    args_set_variable_by_key_long(opts, "input-depth", &vars->input_depth);
    vars->input_depth = 10; /* default */
    args_set_variable_by_key_long(opts, "input-csp", &vars->input_csp);
    vars->input_csp = -1;
    args_set_variable_by_key_long(opts, "seek", &vars->seek);
    args_set_variable_by_key_long(opts, "profile", vars->profile);
    strcpy(vars->profile, "422-10");
    args_set_variable_by_key_long(opts, "level", vars->level);
    strcpy(vars->level, "4.1");
    args_set_variable_by_key_long(opts, "band", &vars->band);
    vars->band = 2; /* default */
    args_set_variable_by_key_long(opts, "bitrate", vars->bitrate);
    args_set_variable_by_key_long(opts, "fps", vars->fps);
    strcpy(vars->fps, "60");
    args_set_variable_by_key_long(opts, "q-matrix-c0", vars->q_matrix[0]);
    strcpy(vars->q_matrix[0], "");
    args_set_variable_by_key_long(opts, "q-matrix-c1", vars->q_matrix[1]);
    strcpy(vars->q_matrix[1], "");
    args_set_variable_by_key_long(opts, "q-matrix-c2", vars->q_matrix[2]);
    strcpy(vars->q_matrix[2], "");
    args_set_variable_by_key_long(opts, "q-matrix-c3", vars->q_matrix[3]);
    strcpy(vars->q_matrix[3], "");
    args_set_variable_by_key_long(opts, "threads", &vars->threads);
    vars->threads = 1; /* default */
    args_set_variable_by_key_long(opts, "preset", vars->preset);
    strcpy(vars->preset, "");

    ARGS_SET_PARAM_VAR_KEY(opts, param, w);
    ARGS_SET_PARAM_VAR_KEY(opts, param, h);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, qp);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, use_filler);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, tile_w_mb);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, tile_h_mb);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, qp_offset_c1);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, qp_offset_c2);
    ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, qp_offset_c3);

    return vars;
}

static void print_usage(const char **argv)
{
    int            i;
    char           str[1024];
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

static int check_conf(oapve_cdesc_t *cdesc, args_var_t *vars)
{
    int i;
    for(i = 0; i < cdesc->max_num_frms; i++) {
        if(vars->hash && strlen(vars->fname_rec) == 0) {
            logerr("cannot use frame hash without reconstructed picture option!\n");
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
        ret = oapve_config(id, OAPV_CFG_SET_USE_FRM_HASH, &value, &size);
        if(OAPV_FAILED(ret)) {
            logerr("failed to set config for using frame hash\n");
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
    logv3("    width               = %d\n", param->w);
    logv3("    height              = %d\n", param->h);
    logv3("    FPS                 = %.2f\n", (float)param->fps_num / param->fps_den);
    logv3("    QP                  = %d\n", param->qp);
    logv3("    max number of AUs   = %d\n", vars->max_au);
    logv3("    rate control type   = %s\n", (param->rc_type == OAPV_RC_ABR) ? "average Bitrate" : "constant QP");
    if(param->rc_type == OAPV_RC_ABR) {
        logv3("    target bitrate      = %dkbps\n", param->bitrate);
    }
    logv3("    tile size           = %d x %d\n", param->tile_w_mb * OAPV_MB_W, param->tile_h_mb * OAPV_MB_H);
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
    int              i, j;
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
        // clang-format on

        logv3("- FRM %-2d GID %-5d %-11s %9d-bytes %8.4fdB %8.4fdB %8.4fdB\n",
              i, finfo[i].group_id, str_frm_type, stat->frm_size[i], psnr[i][0], psnr[i][1], psnr[i][2]);
    }
    fflush(stdout);
    fflush(stderr);
}

static int kbps_str_to_int(char *str)
{
    int kbps;
    if(strchr(str, 'K') || strchr(str, 'k')) {
        char *tmp = strtok(str, "Kk ");
        kbps = (int)(atof(tmp));
    }
    else if(strchr(str, 'M') || strchr(str, 'm')) {
        char *tmp = strtok(str, "Mm ");
        kbps = (int)(atof(tmp) * 1000);
    }
    else {
        kbps = atoi(str);
    }
    return kbps;
}

static int update_param(args_var_t *vars, oapve_param_t *param)
{
    int q_len[OAPV_MAX_CC];
    /* update reate controller  parameters */
    if(strlen(vars->bitrate) > 0) {
        param->bitrate = kbps_str_to_int(vars->bitrate);
        param->rc_type = OAPV_RC_ABR;
    }

    /* update q_matrix */
    for(int c = 0; c < OAPV_MAX_CC; c++) {
        q_len[c] = (int)strlen(vars->q_matrix[c]);
        if(q_len[c] > 0) {
            param->use_q_matrix = 1;
            char *qstr = vars->q_matrix[c];
            int   qcnt = 0;
            while(strlen(qstr) > 0 && qcnt < OAPV_BLK_D) {
                int t0, read;
                sscanf(qstr, "%d%n", &t0, &read);
                if(t0 < 1 || t0 > 255) {
                    logerr("input value (%d) for q_matrix[%d][%d] is invalid\n", t0, c, qcnt);
                    return -1;
                }
                param->q_matrix[c][qcnt] = t0;
                qstr += read;
                qcnt++;
            }
            if(qcnt < OAPV_BLK_D) {
                logerr("input number of q_matrix[%d] is not enough\n", c);
                return -1;
            }
        }
    }

    param->csp = vars->input_csp;

    /* update level idc */
    double tmp_level = 0;
    sscanf(vars->level, "%lf", &tmp_level);
    param->level_idc = tmp_level * 30;
    /* update band idc */
    param->band_idc = vars->band;

    /* update fps */
    if(strpbrk(vars->fps, "/") != NULL) {
        sscanf(vars->fps, "%d/%d", &param->fps_num, &param->fps_den);
    }
    else if(strpbrk(vars->fps, ".") != NULL) {
        float tmp_fps = 0;
        sscanf(vars->fps, "%f", &tmp_fps);
        param->fps_num = tmp_fps * 10000;
        param->fps_den = 10000;
    }
    else {
        sscanf(vars->fps, "%d", &param->fps_num);
        param->fps_den = 1;
    }

    if(strlen(vars->preset) > 0) {
        if(strcmp(vars->preset, "fastest") == 0) {
            param->preset = OAPV_PRESET_FASTEST;
        }
        else if(strcmp(vars->preset, "fast") == 0) {
            param->preset = OAPV_PRESET_FAST;
        }
        else if(strcmp(vars->preset, "medium") == 0) {
            param->preset = OAPV_PRESET_MEDIUM;
        }
        else if(strcmp(vars->preset, "slow") == 0) {
            param->preset = OAPV_PRESET_SLOW;
        }
        else if(strcmp(vars->preset, "placebo") == 0) {
            param->preset = OAPV_PRESET_PLACEBO;
        }
        else {
            logerr("input value of preset is invalid\n");
            return -1;
        }
    }
    else {
        param->preset = OAPV_PRESET_DEFAULT;
    }

    /* update tile */
    if (param->tile_w_mb < OAPV_MIN_TILE_W_MB) {
        param->tile_w_mb = OAPV_MIN_TILE_W_MB;
    }
    if (param->tile_h_mb < OAPV_MIN_TILE_H_MB) {
        param->tile_h_mb = OAPV_MIN_TILE_H_MB;
    }

    int tile_w = param->tile_w_mb << OAPV_LOG2_MB_W;
    int tile_h = param->tile_h_mb << OAPV_LOG2_MB_H;
    int tile_cols = (param->w + tile_w - 1) / tile_w;
    int tile_rows = (param->h + tile_h - 1) / tile_h;
    if (tile_cols > OAPV_MAX_TILE_COLS) {
        param->tile_w_mb = (((param->w + OAPV_MB_W - 1) >> OAPV_LOG2_MB_W) + OAPV_MAX_TILE_COLS - 1) / OAPV_MAX_TILE_COLS;
    }
    if (tile_rows > OAPV_MAX_TILE_ROWS) {
        param->tile_h_mb = (((param->h + OAPV_MB_H - 1) >> OAPV_LOG2_MB_H) + OAPV_MAX_TILE_ROWS - 1) / OAPV_MAX_TILE_ROWS;
    }

    return 0;
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
        logerr("cannot set default parameter\n");
        ret = -1;
        goto ERR;
    }
    /* parse command line */
    args = args_create(enc_args_opts, NUM_ARGS_OPT);
    if(args == NULL) {
        logerr("cannot create argument parser\n");
        ret = -1;
        goto ERR;
    }
    args_var = args_init_vars(args, param);
    if(args_var == NULL) {
        logerr("cannot initialize argument parser\n");
        ret = -1;
        goto ERR;
    }
    if(args->parse(args, argc, argv, &errstr)) {
        logerr("command parsing error (%s)\n", errstr);
        ret = -1;
        goto ERR;
    }
    // print logo
    logv2("  ____                ___   ___ _   __\n");
    logv2(" / __ \\___  ___ ___  / _ | / _ \\ | / / Encoder\n");
    logv2("/ /_/ / _ \\/ -_) _ \\/ __ |/ ___/ |/ / \n");
    logv2("\\____/ .__/\\__/_//_/_/ |_/_/   |___/  \n");
    logv2("    /_/                               \n");
    logv2("\n");

    // print command line string for information
    print_commandline(argc, argv);

    // check mandatory arguments
    if(args->check_mandatory(args, &errstr)) {
        logerr("'--%s' argument is mandatory\n", errstr);
        ret = -1;
        goto ERR;
    }

    /* try to open input file */
    fp_inp = fopen(args_var->fname_inp, "rb");
    if(fp_inp == NULL) {
        logerr("ERROR: cannot open input file = (%s)\n", args_var->fname_inp);
        ret = -1;
        goto ERR;
    }

    /* y4m header parsing  */
    is_inp_y4m = y4m_test(fp_inp);
    if(is_inp_y4m) {
        if(y4m_header_parser(fp_inp, &y4m)) {
            logerr("This y4m is not supported (%s)\n", args_var->fname_inp);
            ret = -1;
            goto ERR;
        }
        y4m_update_param(args, &y4m);
        cfmt = y4m.color_format;
        // clang-format off
        args_var->input_csp = (cfmt == OAPV_CF_YCBCR400 ? 0 : \
            (cfmt == OAPV_CF_YCBCR420 ? 1 : \
            (cfmt == OAPV_CF_YCBCR422 ? 2 : \
            (cfmt == OAPV_CF_YCBCR444 ? 3 : \
            (cfmt == OAPV_CF_YCBCR4444 ? 4 : \
            (cfmt == OAPV_CF_PLANAR2 ? 5 : -1))))));
        // clang-format on

        if(args_var->input_csp != -1) {
            // force "input-csp" argument has set
            args->set_flag(args, "input-csp", 1);
        }
    }
    else {
        // clang-format off
        cfmt = (args_var->input_csp == 0 ? OAPV_CF_YCBCR400 : \
            (args_var->input_csp == 1 ? OAPV_CF_YCBCR420 : \
            (args_var->input_csp == 2 ? OAPV_CF_YCBCR422 : \
            (args_var->input_csp == 3 ? OAPV_CF_YCBCR444  : \
            (args_var->input_csp == 4 ? OAPV_CF_YCBCR4444 : \
            (args_var->input_csp == 5 ? OAPV_CF_PLANAR2   : OAPV_CF_UNKNOWN))))));
        // clang-format on
    }
    if(args_var->input_csp == -1) {
        logerr("Unknown input color space. set '--input-csp' argument\n");
        ret = -1;
        goto ERR;
    }

    /* update parameters */
    if(update_param(args_var, param)) {
        logerr("parameters is not proper\n");
        ret = -1;
        goto ERR;
    }

    cdesc.max_bs_buf_size = MAX_BS_BUF; /* maximum bitstream buffer size */
    cdesc.max_num_frms = MAX_NUM_FRMS;
    cdesc.threads = args_var->threads;

    if(check_conf(&cdesc, args_var)) {
        logerr("invalid configuration\n");
        ret = -1;
        goto ERR;
    }

    if(strlen(args_var->fname_out) > 0) {
        clear_data(args_var->fname_out);
        is_out = 1;
    }

    if(strlen(args_var->fname_rec) > 0) {
        ret = check_file_name_type(args_var->fname_rec);
        if(ret > 0) {
            is_rec_y4m = 1;
        }
        else if(ret == 0) {
            is_rec_y4m = 0;
        }
        else { // invalid or unknown file name type
            logerr("unknown file name type for reconstructed video\n");
            ret = -1; goto ERR;
        }
        clear_data(args_var->fname_rec);
        is_rec = 1;
    }

    /* allocate bitstream buffer */
    bs_buf = (unsigned char *)malloc(MAX_BS_BUF);
    if(bs_buf == NULL) {
        logerr("cannot allocate bitstream buffer, size=%d", MAX_BS_BUF);
        ret = -1;
        goto ERR;
    }

    /* create encoder */
    id = oapve_create(&cdesc, NULL);
    if(id == NULL) {
        logerr("cannot create OAPV encoder\n");
        ret = -1;
        goto ERR;
    }

    /* create metadata handler */
    mid = oapvm_create(&ret);
    if(mid == NULL || OAPV_FAILED(ret)) {
        logerr("cannot create OAPV metadata handler\n");
        ret = -1;
        goto ERR;
    }

    if(set_extra_config(id, args_var, param)) {
        logerr("cannot set extra configurations\n");
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

    for(int i = 0; i < num_frames; i++) {
        if(args_var->input_depth == 10) {
            ifrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
        }
        else {
            imgb_r = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
            ifrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, 10, 0));
        }

        if(is_rec) {
            if(args_var->input_depth == 10) {
                rfrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
            }
            else {
                imgb_w = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, args_var->input_depth, 0));
                rfrms.frm[i].imgb = imgb_create(param->w, param->h, OAPV_CS_SET(cfmt, 10, 0));
            }
            rfrms.num_frms++;
        }
        ifrms.num_frms++;
    }

    /* encode pictures *******************************************************/
    while(args_var->max_au == 0 || (au_cnt < args_var->max_au)) {
        for(int i = 0; i < num_frames; i++) {
            if(args_var->input_depth == 10) {
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
            if(args_var->input_depth != 10) {
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

            bitrate_tot += stat.frm_size[FRM_IDX];

            print_stat_au(&stat, au_cnt, param, args_var->max_au, bitrate_tot, clk_end, clk_tot);

            for(int fidx = 0; fidx < num_frames; fidx++) {
                if(is_rec) {
                    if(args_var->input_depth != 10) {
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
                            logerr("cannot write bitstream\n");
                            ret = -1;
                            goto ERR;
                        }
                    }
                }
                else {
                    logerr("failed to encode\n");
                    ret = -1;
                    goto ERR;
                }

                // store recon image
                if(is_rec) {
                    if(frm_cnt[fidx] == 0 && is_rec_y4m) {
                        if(write_y4m_header(args_var->fname_rec, imgb_o)) {
                            logerr("cannot write Y4M header\n");
                            ret = -1;
                            goto ERR;
                        }
                    }
                    if(write_rec_img(args_var->fname_rec, imgb_o, is_rec_y4m)) {
                        logerr("cannot write reconstructed video\n");
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
    psnr_avg[FRM_IDX][1] /= au_cnt;
    psnr_avg[FRM_IDX][2] /= au_cnt;
    if(cfmt == OAPV_CF_YCBCR4444) {
        psnr_avg[FRM_IDX][3] /= au_cnt;
    }

    logv3("  PSNR Y(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][0]);
    logv3("  PSNR U(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][1]);
    logv3("  PSNR V(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][2]);
    if(cfmt == OAPV_CF_YCBCR4444) {
        logv3("  PSNR T(dB)       : %-5.4f\n", psnr_avg[FRM_IDX][3]);
    }
    logv3("  Total bits(bits) : %.0f\n", bitrate_tot * 8);
    bitrate_tot *= (((float)param->fps_num / param->fps_den) * 8);
    bitrate_tot /= au_cnt;
    bitrate_tot /= 1000;

    logv3("  -----------------: bitrate(kbps)\tPSNR-Y\tPSNR-U\tPSNR-V\n");
    if(cfmt == OAPV_CF_YCBCR4444) {
        logv3("  Summary          : %-4.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\n",
              bitrate_tot, psnr_avg[FRM_IDX][0], psnr_avg[FRM_IDX][1], psnr_avg[FRM_IDX][2], psnr_avg[FRM_IDX][3]);
    }
    else {
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
