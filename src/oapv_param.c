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
#include <string.h>

int oapve_param_default(oapve_param_t *param)
{
    oapv_mset(param, 0, sizeof(oapve_param_t));
    param->preset = OAPV_PRESET_DEFAULT;

    param->qp = OAPVE_PARAM_QP_AUTO; // default
    param->qp_offset_c1 = 0;
    param->qp_offset_c2 = 0;
    param->qp_offset_c3 = 0;

    param->tile_w = 16 * OAPV_MB_W; // default: 256
    param->tile_h = 16 * OAPV_MB_H; // default: 256

    param->profile_idc = OAPV_PROFILE_422_10;
    param->level_idc = OAPVE_PARAM_LEVEL_IDC_AUTO;
    param->band_idc  = OAPVE_PARAM_BAND_IDC_AUTO;

    param->use_q_matrix = 0;

    for(int c = 0; c < OAPV_MAX_CC; c++) {
        for(int i = 0; i < OAPV_BLK_D; i++) {
            param->q_matrix[c][i] = 16;
        }
    }

    param->color_description_present_flag = 0;
    param->color_primaries = 2; // unspecified color primaries
    param->transfer_characteristics = 2; // unspecified transfer characteristics
    param->matrix_coefficients = 2; // unspecified matrix coefficients
    param->full_range_flag = 0; // limited range
    return OAPV_OK;
}

///////////////////////////////////////////////////////////////////////////////
// parameter parsing helper function for encoder
static int is_digit(const char* str)
{
    while(*str) {
        if(*str < '0' || *str > '9')
            return 0;
        ++str;
    }
    return 1;
}

static int get_ival_from_skey(const oapv_dict_str_int_t * dict, const char * skey, int * ival)
{
    while(strlen(dict->key) > 0) {
        if(strcmp(dict->key, skey) == 0){
            *ival = dict->val;
            return 0;
        }
        dict++;
    }
    return -1;
}

static int kbps_str_to_int(const char *str)
{
    int kbps;
    char *s = (char *)str;
    if(strchr(s, 'K') || strchr(s, 'k')) {
        char *tmp = strtok(s, "Kk ");
        if(tmp == NULL) return -1;
        char *endptr;
        float fval;
        errno = 0;
        fval = strtof(tmp, &endptr);
        if(endptr == tmp || *endptr != '\0' || errno == ERANGE) {
            return -1;
        }
        kbps = (int)fval;
    }
    else if(strchr(s, 'M') || strchr(s, 'm')) {
        char *tmp = strtok(s, "Mm ");
        if(tmp == NULL) return -1;
        char *endptr;
        float fval;
        errno = 0;
        fval = strtof(tmp, &endptr);
        if(endptr == tmp || *endptr != '\0' || errno == ERANGE) {
            return -1;
        }
        kbps = (int)(fval * 1000);
    }
    else if(strchr(s, 'G') || strchr(s, 'g')) {
        char *tmp = strtok(s, "Gg ");
        if(tmp == NULL) return -1;
        char *endptr;
        float fval;
        errno = 0;
        fval = strtof(tmp, &endptr);
        if(endptr == tmp || *endptr != '\0' || errno == ERANGE) {
            return -1;
        }
        kbps = (int)(fval * 1000000);
    }
    else {
        char *endptr;
        long val;
        errno = 0;
        val = strtol(s, &endptr, 10);
        if(endptr == s || *endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX) {
            return -1;
        }
        kbps = (int)val;
    }
    return kbps;
}

static int get_q_matrix(const char *str, u8 q_matrix[OAPV_BLK_D])
{
    int   t0, qcnt = 0;
    char *left;
    char *qstr = (char *)str;

    while(strlen(qstr) > 0 && qcnt < OAPV_BLK_D) {
        t0 = strtol(qstr, &left, 10);
        oapv_assert_rv(t0 >= 1 && t0 <= 255, -1);

        q_matrix[qcnt] = (u8)t0;
        qstr = left;
        qcnt++;
    }
    oapv_assert_rv(qcnt == OAPV_BLK_D, -1);
    return 0;
}

#define NAME_CMP(VAL)      else if(strcmp(name, VAL)== 0)
#define GET_INTEGER_OR_ERR(STR, F, ERR) { \
    char * left; (F) = strtol(STR, &left, 10); \
    if(strlen(left)>0) return (ERR); \
}
#define GET_INTEGER_MIN_OR_ERR(STR, F, MIN, ERR) { \
        GET_INTEGER_OR_ERR(STR, F, ERR); \
        if((F) < (MIN)) return (ERR); \
}
#define GET_INTEGER_MIN_MAX_OR_ERR(STR, F, MIN, MAX, ERR) { \
    GET_INTEGER_OR_ERR(STR, F, ERR); \
    if((F) < (MIN) || (F) > (MAX)) return (ERR); \
}
#define GET_FLOAT_OR_ERR(STR, F, ERR) { \
    char * left; (F) = strtof(STR, &left); \
    if(strlen(left)>0) return (ERR); \
}

int oapve_param_parse(oapve_param_t *param, const char *name,  const char *value)
{
    u8    q_matrix[OAPV_BLK_D];
    char  str_buf[64];
    int   ti0;
    float tf0;

    if(param == NULL || name == NULL || value == NULL) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }

    /* normalization of name and value ***************************************/
    // pass '-- prefix' if exists
    if(name[0] == '-' && name[1] == '-') { name += 2; }

    // replace '_' with '-'
    if(strlen(name) + 1 < sizeof(str_buf) && strchr(name, '_')) {
        char *c;
        strcpy(str_buf, name);
        while((c = strchr(str_buf, '_')) != 0) { *c = '-'; } // replace
        name = str_buf; // change address
    }

    /* parsing ***************************************************************/
    if(0){;}
    NAME_CMP("profile") {
        if(get_ival_from_skey(oapv_param_opts_profile, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->profile_idc = ti0;
    }
    NAME_CMP("level") {
        if(!strcmp(value, "auto")) {
            param->level_idc = OAPVE_PARAM_LEVEL_IDC_AUTO;
        }
        else {
            GET_FLOAT_OR_ERR(value, tf0, OAPV_ERR_INVALID_ARGUMENT);
            // validation check
            // level == [1, 1.1, 2, 2.1, 3, 3.1, 4, 4.1, 5, 5.1, 6, 6.1, 7, 7.1]
            if(tf0 == 1.0f || tf0 == 1.1f || tf0 == 2.0f || tf0 == 2.1f || \
                tf0 == 3.0f || tf0 == 3.1f || tf0 == 4.0f || tf0 == 4.1f ||\
                tf0 == 5.0f || tf0 == 5.1f || tf0 == 6.0f || tf0 == 6.1f ||\
                tf0 == 7.0f || tf0 == 7.1f) {
                param->level_idc = OAPV_LEVEL_TO_LEVEL_IDC(tf0);
            }
            else {
                return OAPV_ERR_INVALID_ARGUMENT;
            }
        }
    }
    NAME_CMP("band") {
        if (!strcmp(value, "auto")) {
            param->band_idc = OAPVE_PARAM_BAND_IDC_AUTO;
        }
        else {
            GET_INTEGER_MIN_MAX_OR_ERR(value, ti0, 0, 3, OAPV_ERR_INVALID_ARGUMENT);
            param->band_idc = ti0;
        }
    }
    NAME_CMP("preset") {
        if(get_ival_from_skey(oapv_param_opts_preset, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->preset = ti0;
    }
    NAME_CMP("width") {
        GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_WIDTH);
        oapv_assert_rv(ti0 > 0, OAPV_ERR_INVALID_WIDTH);
        param->w = ti0;
    }
    NAME_CMP("height") {
        GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_HEIGHT);
        oapv_assert_rv(ti0 > 0, OAPV_ERR_INVALID_HEIGHT);
        param->h = ti0;
    }
    NAME_CMP("fps") {
        if(strpbrk(value, "/") != NULL) {
            // parse into locals first so a bad value leaves param unchanged
            int num, den;
            if(sscanf(value, "%d/%d", &num, &den) != 2 || num <= 0 || den <= 0) {
                return OAPV_ERR_INVALID_FPS;
            }
            param->fps_num = num;
            param->fps_den = den;
        }
        else if(strpbrk(value, ".") != NULL) {
            GET_FLOAT_OR_ERR(value, tf0, OAPV_ERR_INVALID_FPS);
            // reject values that would overflow the fixed-point fps_num
            if(!(tf0 > 0.0f) || tf0 > (float)INT_MAX / 10000.0f) {
                return OAPV_ERR_INVALID_FPS;
            }
            param->fps_num = (int)(tf0 * 10000);
            param->fps_den = 10000;
        }
        else {
            GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_FPS);
            if(ti0 <= 0) {
                return OAPV_ERR_INVALID_FPS;
            }
            param->fps_num = ti0;
            param->fps_den = 1;
        }
    }
    NAME_CMP("qp") {
        if(!strcmp(value, "auto")) {
            param->qp = OAPVE_PARAM_QP_AUTO;
            param->rc_type = OAPV_RC_ABR;
        }
        else {
            //  QP value: 0 ~ (63 + (bitdepth - 10)*6)
            //     - 10bit input: 0 ~ 63"
            //     - 12bit input: 0 ~ 75"
            // max value cannot be decided without bitdepth value
            GET_INTEGER_MIN_MAX_OR_ERR(value, ti0, MIN_QUANT, MAX_QUANT(12), OAPV_ERR_INVALID_QP);
            param->qp = ti0;
            param->rc_type = OAPV_RC_CQP;
        }
    }
    NAME_CMP("qp-offset-c1") {
        GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_QP);
        param->qp_offset_c1 = ti0;
    }
    NAME_CMP("qp-offset-c2") {
        GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_QP);
        param->qp_offset_c2 = ti0;
    }
    NAME_CMP("qp-offset-c3") {
        GET_INTEGER_OR_ERR(value, ti0, OAPV_ERR_INVALID_QP);
        param->qp_offset_c3 = ti0;
    }
    NAME_CMP("bitrate") {
        if(strlen(value) > 0) {
            if(strlen(value) + 1 > sizeof(str_buf)) return OAPV_ERR_INVALID_ARGUMENT;
            strcpy(str_buf, value); // to maintain original value
            param->bitrate = kbps_str_to_int(str_buf); // unit: kbps
            if(param->bitrate <= 0) return OAPV_ERR_INVALID_ARGUMENT;
            param->rc_type = OAPV_RC_ABR;
        }
        else return OAPV_ERR_INVALID_ARGUMENT;
    }
    NAME_CMP("q-matrix-c0") {

        if(get_q_matrix(value, q_matrix)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        oapv_mcpy(param->q_matrix[Y_C], q_matrix, sizeof(u8)*OAPV_BLK_D);
        param->use_q_matrix = 1;
    }
    NAME_CMP("q-matrix-c1") {

        if(get_q_matrix(value, q_matrix)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        oapv_mcpy(param->q_matrix[U_C], q_matrix, sizeof(u8)*OAPV_BLK_D);
        param->use_q_matrix = 1;
    }
    NAME_CMP("q-matrix-c2") {

        if(get_q_matrix(value, q_matrix)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        oapv_mcpy(param->q_matrix[V_C], q_matrix, sizeof(u8)*OAPV_BLK_D);
        param->use_q_matrix = 1;
    }
    NAME_CMP("q-matrix-c3") {

        if(get_q_matrix(value, q_matrix)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        oapv_mcpy(param->q_matrix[X_C], q_matrix, sizeof(u8)*OAPV_BLK_D);
        param->use_q_matrix = 1;
    }
    NAME_CMP("tile-w") {
        GET_INTEGER_MIN_OR_ERR(value, ti0, OAPV_MIN_TILE_W, OAPV_ERR_INVALID_ARGUMENT);
        oapv_assert_rv((ti0 & (OAPV_MB_W - 1)) == 0, OAPV_ERR_INVALID_ARGUMENT);
        param->tile_w = ti0;
    }
    NAME_CMP("tile-h") {
        GET_INTEGER_MIN_OR_ERR(value, ti0, OAPV_MIN_TILE_H, OAPV_ERR_INVALID_ARGUMENT);
        oapv_assert_rv((ti0 & (OAPV_MB_W - 1)) == 0, OAPV_ERR_INVALID_ARGUMENT);
        param->tile_h = ti0;
    }
    NAME_CMP("color-primaries") {
        if(get_ival_from_skey(oapv_param_opts_color_primaries, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->color_primaries = ti0;
        param->color_description_present_flag = 1;
    }
    NAME_CMP("color-transfer") {
        if(get_ival_from_skey(oapv_param_opts_color_transfer, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->transfer_characteristics = ti0;
        param->color_description_present_flag = 1;
    }
    NAME_CMP("color-matrix") {
        if(get_ival_from_skey(oapv_param_opts_color_matrix, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->matrix_coefficients = ti0;
        param->color_description_present_flag = 1;
    }
    NAME_CMP("color-range") {
        if(get_ival_from_skey(oapv_param_opts_color_range, value, &ti0)) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        param->full_range_flag = ti0;
        param->color_description_present_flag = 1;
    }
    else {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    return OAPV_OK;
}

#define MAX_LEVEL_NUM 14
#define MAX_BAND_NUM  4

static float level_avail[MAX_LEVEL_NUM] = {
    1, 1.1, 2, 2.1, 3, 3.1, 4, 4.1, 5, 5.1, 6, 6.1, 7, 7.1
};

static int level_idc_to_level_idx(int level_idc)
{
    for (int i = 0; i < MAX_LEVEL_NUM; i++) {
        if (level_idc == OAPV_LEVEL_TO_LEVEL_IDC(level_avail[i])) {
            return i;
        }
    }

    return OAPV_ERR;
}

static int max_coded_data_rate[MAX_LEVEL_NUM][MAX_BAND_NUM] = {
    {     8000,    11000,     15000,     23000 },
    {    16000,    21000,     30000,     45000 },
    {    39000,    54000,     76000,    114000 },
    {    78000,   108000,    152000,    227000 },
    {   114000,   159000,    222000,    333000 },
    {   227000,   317000,    444000,    666000 },
    {   455000,   637000,    892000,   1338000 },
    {   910000,  1274000,   1784000,   2675000 },
    {  1820000,  2548000,   3567000,   5350000 },
    {  3639000,  5095000,   7133000,  10699000 },
    {  7278000, 10189000,  14265000,  21397000 },
    { 14556000, 20378000,  28529000,  42793000 },
    { 29111000, 40756000,  57058000,  85586000 },
    { 58222000, 81511000, 114115000, 171172000 }
};

static u64 max_luma_sample_rate[MAX_LEVEL_NUM] = {
    3041280,     6082560,    15667200,   31334400,
    66846720,    133693440,  265420800,  530841600,
    1061683200,  2123366400, 4777574400, 8493465600,
    16986931200, 33973862400
};

static int enc_update_param_level_band(oapve_param_t* param)
{
    int w = oapv_div_round_up(param->w, OAPV_MB_W) * OAPV_MB_W;
    int h = oapv_div_round_up(param->h, OAPV_MB_H) * OAPV_MB_H;
    // fps is required for rate control (ABR) and for automatic level selection;
    // it stays optional for fixed-QP with an explicit level
    if(param->rc_type != OAPV_RC_CQP || param->level_idc == OAPVE_PARAM_LEVEL_IDC_AUTO) {
        oapv_assert_rv(param->fps_num > 0 && param->fps_den > 0, OAPV_ERR_INVALID_FPS);
    }
    // otherwise fps is optional; guard the divisor when it is left unset
    double fps = (param->fps_den > 0) ? (double)param->fps_num / param->fps_den : 0.0;
    u64 luma_sample_rate = (int)((double)w * h * fps);
    int min_level_idx = 0;
    for (int i = 0 ; i < MAX_LEVEL_NUM ; i++) {
        if (luma_sample_rate <= max_luma_sample_rate[i]) {
            min_level_idx = i;
            break;
        }
    }

    if (param->bitrate > 0) {
        if (param->band_idc == OAPVE_PARAM_BAND_IDC_AUTO) {
            int is_found = 0;
            for (int i = min_level_idx; i < MAX_LEVEL_NUM; i++) {
                for (int j = 0; j < MAX_BAND_NUM; j++) {
                    if (param->bitrate <= max_coded_data_rate[i][j]) {
                        min_level_idx = i;
                        param->band_idc = j;
                        is_found = 1;
                        break;
                    }
                }
                if (is_found) {
                    break;
                }
            }
            oapv_assert_rv(is_found == 1, OAPV_ERR);
        }
        else {
            for (int i = min_level_idx; i < MAX_LEVEL_NUM; i++) {
                if (param->bitrate <= max_coded_data_rate[i][param->band_idc]) {
                    min_level_idx = i;
                    break;
                }
            }
        }
    }
    else if (param->band_idc == OAPVE_PARAM_BAND_IDC_AUTO) {
        param->band_idc = 2;
    }

    int min_level_idc = OAPV_LEVEL_TO_LEVEL_IDC(level_avail[min_level_idx]);

    if (param->level_idc == OAPVE_PARAM_LEVEL_IDC_AUTO) {
        param->level_idc = min_level_idc;
    }
    else {
        if (param->level_idc < min_level_idc) {
            return OAPV_ERR_INVALID_LEVEL;
        }
    }

    return OAPV_OK;
}

static int enc_update_param_bitrate(oapve_param_t* param)
{
    int level_idx = level_idc_to_level_idx(param->level_idc);

    // bound the rate-table indices before use
    oapv_assert_rv(level_idx >= 0 && level_idx < MAX_LEVEL_NUM, OAPV_ERR_INVALID_LEVEL);
    oapv_assert_rv(param->band_idc >= 0 && param->band_idc < MAX_BAND_NUM, OAPV_ERR_INVALID_ARGUMENT);

    if (param->bitrate == 0 && param->qp == OAPVE_PARAM_QP_AUTO) {
        param->bitrate = max_coded_data_rate[level_idx][param->band_idc];
    }
    else if (param->bitrate > 0) {
        if (param->bitrate > max_coded_data_rate[level_idx][param->band_idc]) {
            return OAPV_ERR_INVALID_LEVEL;
        }
    }

    return OAPV_OK;
}

static int enc_update_param_tile(oapve_ctx_t* ctx, oapve_param_t* param)
{
    /* frame dimensions must fit the 24-bit frame_info fields */
    oapv_assert_rv(param->w > 0 && param->w <= ((1 << 24) - 1), OAPV_ERR_INVALID_WIDTH);
    oapv_assert_rv(param->h > 0 && param->h <= ((1 << 24) - 1), OAPV_ERR_INVALID_HEIGHT);

    /* set various value */
    ctx->w = oapv_div_round_up(param->w, OAPV_MB_W) * OAPV_MB_W;
    ctx->h = oapv_div_round_up(param->h, OAPV_MB_H) * OAPV_MB_H;

    /* find correct tile width and height */
    int tile_w, tile_h;

    oapv_assert_rv(param->tile_w >= OAPV_MIN_TILE_W && param->tile_h >= OAPV_MIN_TILE_H, OAPV_ERR_INVALID_ARGUMENT);
    oapv_assert_rv(param->tile_w <= ctx->w && param->tile_h <= ctx->h, OAPV_ERR_INVALID_ARGUMENT);
    oapv_assert_rv((param->tile_w & (OAPV_MB_W - 1)) == 0 && (param->tile_h & (OAPV_MB_H - 1)) == 0, OAPV_ERR_INVALID_ARGUMENT);

    if (oapv_div_round_up(ctx->w, param->tile_w) > OAPV_MAX_TILE_COLS) {
        tile_w = oapv_div_round_up(ctx->w, OAPV_MAX_TILE_COLS);
        tile_w = oapv_div_round_up(tile_w, OAPV_MB_W) * OAPV_MB_W; // align to MB width
    }
    else {
        tile_w = param->tile_w;
    }
    param->tile_w = tile_w;

    if (oapv_div_round_up(ctx->h, param->tile_h) > OAPV_MAX_TILE_ROWS) {
        tile_h = oapv_div_round_up(ctx->h, OAPV_MAX_TILE_ROWS);
        tile_h = oapv_div_round_up(tile_h, OAPV_MB_H) * OAPV_MB_H; // align to MB height
    }
    else {
        tile_h = param->tile_h;
    }
    param->tile_h = tile_h;

    return OAPV_OK;
}

int oapve_param_update(oapve_ctx_t* ctx)
{
    int ret = OAPV_OK;
    int min_num_tiles = OAPV_MAX_TILES;
    // bound the frame count to the param[] array size
    oapv_assert_rv(ctx->cdesc.max_num_frms >= 1 && ctx->cdesc.max_num_frms <= OAPV_MAX_NUM_FRAMES,
                   OAPV_ERR_INVALID_ARGUMENT);
    for (int i = 0; i < ctx->cdesc.max_num_frms; i++) {
        ret = enc_update_param_tile(ctx, &ctx->cdesc.param[i]);
        oapv_assert_rv(ret == OAPV_OK, ret);
        int num_tiles = oapv_div_round_up(ctx->w, ctx->cdesc.param[i].tile_w) * oapv_div_round_up(ctx->h, ctx->cdesc.param[i].tile_h);
        min_num_tiles = oapv_min(min_num_tiles, num_tiles);

        ret = enc_update_param_level_band(&ctx->cdesc.param[i]);
        oapv_assert_rv(ret == OAPV_OK, ret);

        ret = enc_update_param_bitrate(&ctx->cdesc.param[i]);
        oapv_assert_rv(ret == OAPV_OK, ret);
    }

    if (ctx->cdesc.threads == OAPV_CDESC_THREADS_AUTO) {
        int num_cores = oapv_get_num_cpu_cores();
        ctx->threads = oapv_min(OAPV_MAX_THREADS, oapv_min(num_cores, min_num_tiles));
    }
    else {
        ctx->threads = ctx->cdesc.threads;
    }

    return ret;
}

/* APV family series information */
static int family_info[][2] = {
    {      (0 * 0),  38}, // minimum value undefined in family spec
    {  (960 * 540),  72}, // qHD
    { (1280 * 720),  98}, // 720p
    {(1920 * 1080), 198}, // FHD
    {(2048 * 1080), 211}, // 2K
    {(3840 * 2160), 796}, // UHD 4K
};

#define NUM_FAMILY_INFO ((int)(sizeof(family_info) / sizeof(family_info[0])))

static float get_key_bitrate(int w, int h)
{
    int idx, wh_hi, wh_lo, bit_hi, bit_lo;
    int wh = w * h;
    float key = 0.f;

    for(idx = 0; idx < NUM_FAMILY_INFO; idx++) {
        if(wh < family_info[idx][0]) {
            wh_hi  = family_info[idx][0]; // resolution of high-bound
            bit_hi = family_info[idx][1]; // Mbps for high-bound
            if(idx == 0) {
                // below the smallest tabulated resolution: interpolate from origin
                wh_lo  = 0;
                bit_lo = 0;
            }
            else {
                wh_lo  = family_info[idx-1][0]; // resolution of low-bound
                bit_lo = family_info[idx-1][1]; // Mbps of low-bound
            }

            float ratio = (float)(bit_hi - bit_lo) / (wh_hi - wh_lo);
            key   = bit_lo + (ratio * (wh - wh_lo));
            break;
        }
    }
    if(idx == NUM_FAMILY_INFO) {
        // needs to linear interpolation from the last element of the family table.
        int fidx = NUM_FAMILY_INFO - 1;
        wh_hi  = family_info[fidx][0];
        bit_hi = family_info[fidx][1];
        key    = bit_hi * ((float)wh / wh_hi);
    }
    return key;
}

int oapve_family_bitrate(int family, int w, int h, int fps_num, int fps_den, int * kbps)
{
    float key, ratio;

    // frame rate is a divisor below; reject non-positive values
    oapv_assert_rv(fps_num > 0 && fps_den > 0, OAPV_ERR_INVALID_FPS);

    switch(family) {
    case OAPV_FAMILY_422_LQ:
        ratio = 1.f / (1.4f * 1.4f);
        break;
    case OAPV_FAMILY_422_SQ:
        ratio = 1.f / 1.4f;
        break;
    case OAPV_FAMILY_422_HQ:
        ratio = 1.f;
        break;
    case OAPV_FAMILY_444_UQ:
        ratio = 1.5f;
        break;
    default: // invalid family
        return OAPV_ERR_INVALID_FAMILY; // unknown family
    }
    key = get_key_bitrate(w, h);
    *kbps = (int)(key * ratio * ((float)fps_num/fps_den)/30.f * 1000.f); // unit: kbps
    return OAPV_OK;
}

