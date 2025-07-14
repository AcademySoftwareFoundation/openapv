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

#ifndef _OAPV_APP_UTIL_H_
#define _OAPV_APP_UTIL_H_

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#if LINUX
#include <signal.h>
#include <unistd.h>
#endif

#define VERBOSE_NONE   0
#define VERBOSE_ERROR  1
#define VERBOSE_SIMPLE 2
#define VERBOSE_FRAME  3

/* logging functions */
static void log_msg(char *filename, int line, const char *fmt, ...)
{
    char str[1024] = { '\0' };
    if(filename != NULL && line >= 0)
        sprintf(str, "[%s:%d] ", filename, line);
    va_list args;
    va_start(args, fmt);
    vsprintf(str + strlen(str), fmt, args);
    va_end(args);
    printf("%s", str);
}

static void log_line(char *pre)
{
    int       i, len;
    char      str[128] = { '\0' };
    const int chars = 80;
    for(i = 0; i < 3; i++) {
        str[i] = '=';
    }
    str[i] = '\0';

    len = (pre == NULL) ? 0 : (int)strlen(pre);
    if(len > 0) {
        sprintf(str + 3, " %s ", pre);
        len = (int)strlen(str);
    }

    for(i = len; i < chars; i++) {
        str[i] = '=';
    }
    str[chars] = '\0';
    printf("%s\n", str);
}

#if defined(__GNUC__)
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define logerr(args...)                   \
    {                                     \
        if(op_verbose >= VERBOSE_ERROR) { \
            log_msg(NULL, -1, args);      \
        }                                 \
    }
#define logv2(args...)                     \
    {                                      \
        if(op_verbose >= VERBOSE_SIMPLE) { \
            log_msg(NULL, -1, args);       \
        }                                  \
    }
#define logv3(args...)                    \
    {                                     \
        if(op_verbose >= VERBOSE_FRAME) { \
            log_msg(NULL, -1, args);      \
        }                                 \
    }
#else
#define __FILENAME__ \
    (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define logerr(args, ...)                         \
    {                                             \
        if(op_verbose >= VERBOSE_ERROR) {         \
            log_msg(NULL, -1, args, __VA_ARGS__); \
        }                                         \
    }
#define logv2(args, ...)                          \
    {                                             \
        if(op_verbose >= VERBOSE_SIMPLE) {        \
            log_msg(NULL, -1, args, __VA_ARGS__); \
        }                                         \
    }
#define logv3(args, ...)                          \
    {                                             \
        if(op_verbose >= VERBOSE_FRAME) {         \
            log_msg(NULL, -1, args, __VA_ARGS__); \
        }                                         \
    }
#endif
#define logv2_line(pre)                    \
    {                                      \
        if(op_verbose >= VERBOSE_SIMPLE) { \
            log_line(pre);                 \
        }                                  \
    }
#define logv3_line(pre)                   \
    {                                     \
        if(op_verbose >= VERBOSE_FRAME) { \
            log_line(pre);                \
        }                                 \
    }

/* assert function */
#include <assert.h>
#define assert_r(x)    \
    {                  \
        if(!(x)) {     \
            assert(x); \
            return;    \
        }              \
    }
#define assert_rv(x, r) \
    {                   \
        if(!(x)) {      \
            assert(x);  \
            return (r); \
        }               \
    }
#define assert_g(x, g) \
    {                  \
        if(!(x)) {     \
            assert(x); \
            goto g;    \
        }              \
    }
#define assert_gv(x, r, v, g) \
    {                         \
        if(!(x)) {            \
            assert(x);        \
            (r) = (v);        \
            goto g;           \
        }                     \
    }

static int op_verbose = VERBOSE_SIMPLE;

/* Clocks */
#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>

typedef DWORD oapv_clk_t;
#define OAPV_CLK_PER_SEC  (1000)
#define OAPV_CLK_PER_MSEC (1)
#define OAPV_CLK_MAX      ((oapv_clk_t)(-1))
#define oapv_clk_get()    GetTickCount()

#elif __linux__ || __CYGWIN__ || __APPLE__
#include <time.h>
#include <sys/time.h>
typedef unsigned long oapv_clk_t;
#define OAPV_CLK_MAX      ((oapv_clk_t)(-1))
#define OAPV_CLK_PER_SEC  (10000)
#define OAPV_CLK_PER_MSEC (10)
static oapv_clk_t     oapv_clk_get(void)
{
    oapv_clk_t     clk;
    struct timeval t;
    gettimeofday(&t, NULL);
    clk = t.tv_sec * 10000L + t.tv_usec / 100L;
    return clk;
}

#else
#error THIS PLATFORM CANNOT SUPPORT CLOCK
#endif

static __inline oapv_clk_t oapv_clk_diff(oapv_clk_t t1, oapv_clk_t t2)
{
    return (((t2) >= (t1)) ? ((t2) - (t1)) : ((OAPV_CLK_MAX - (t1)) + (t2)));
}

static __inline oapv_clk_t oapv_clk_from(oapv_clk_t from)
{
    oapv_clk_t now = oapv_clk_get();
    return oapv_clk_diff(from, now);
}

static __inline oapv_clk_t oapv_clk_msec(oapv_clk_t clk)
{
    return ((oapv_clk_t)((clk + (OAPV_CLK_PER_MSEC / 2)) / OAPV_CLK_PER_MSEC));
}

static __inline oapv_clk_t oapv_clk_sec(oapv_clk_t clk)
{
    return ((oapv_clk_t)((clk + (OAPV_CLK_PER_SEC / 2)) / OAPV_CLK_PER_SEC));
}

#define CLIP_VAL(n, min, max) (((n) > (max)) ? (max) : (((n) < (min)) ? (min) : (n)))
#define ALIGN_VAL(val, align) ((((val) + (align) - 1) / (align)) * (align))

/* Function for atomic increment:
   This function might need to modify according to O/S or CPU platform
*/
static int atomic_inc(volatile int *pcnt)
{
    int ret;
    ret = *pcnt;
    ret++;
    *pcnt = ret;
    return ret;
}

/* Function for atomic decrement:
   This function might need to modify according to O/S or CPU platform
*/
static int atomic_dec(volatile int *pcnt)
{
    int ret;
    ret = *pcnt;
    ret--;
    *pcnt = ret;
    return ret;
}

/* Function to allocate memory for picture buffer:
   This function might need to modify according to O/S or CPU platform
*/
static void *picbuf_alloc(int size)
{
    return malloc(size);
}

/* Function to free memory allocated for picture buffer:
   This function might need to modify according to O/S or CPU platform
*/
static void picbuf_free(void *p)
{
    if(p) {
        free(p);
    }
}

static int imgb_addref(oapv_imgb_t *imgb)
{
    assert_rv(imgb, OAPV_ERR_INVALID_ARGUMENT);
    return atomic_inc(&imgb->refcnt);
}

static int imgb_getref(oapv_imgb_t *imgb)
{
    assert_rv(imgb, OAPV_ERR_INVALID_ARGUMENT);
    return imgb->refcnt;
}

static int imgb_release(oapv_imgb_t *imgb)
{
    int refcnt, i;
    assert_rv(imgb, OAPV_ERR_INVALID_ARGUMENT);
    refcnt = atomic_dec(&imgb->refcnt);
    if(refcnt == 0) {
        for(i = 0; i < OAPV_MAX_CC; i++) {
            if(imgb->baddr[i])
                picbuf_free(imgb->baddr[i]);
        }
        free(imgb);
    }
    return refcnt;
}

oapv_imgb_t *imgb_create(int w, int h, int cs)
{
    int          i, bd;
    oapv_imgb_t *imgb;

    imgb = (oapv_imgb_t *)malloc(sizeof(oapv_imgb_t));
    if(imgb == NULL)
        goto ERR;
    memset(imgb, 0, sizeof(oapv_imgb_t));

    bd = OAPV_CS_GET_BYTE_DEPTH(cs); /* byte unit */

    imgb->w[0] = w;
    imgb->h[0] = h;
    switch(OAPV_CS_GET_FORMAT(cs)) {
    case OAPV_CF_YCBCR400:
        imgb->w[1] = imgb->w[2] = w;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 1;
        break;
    case OAPV_CF_YCBCR420:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = (h + 1) >> 1;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR422:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR444:
        imgb->w[1] = imgb->w[2] = w;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR4444:
        imgb->w[1] = imgb->w[2] = imgb->w[3] = w;
        imgb->h[1] = imgb->h[2] = imgb->h[3] = h;
        imgb->np = 4;
        break;
    case OAPV_CF_PLANAR2:
        imgb->w[1] = w;
        imgb->h[1] = h;
        imgb->np = 2;
        break;
    default:
        logv3("unsupported color format\n");
        goto ERR;
    }

    for(i = 0; i < imgb->np; i++) {
        // width and height need to be aligned to macroblock size
        imgb->aw[i] = ALIGN_VAL(imgb->w[i], OAPV_MB_W);
        imgb->s[i] = imgb->aw[i] * bd;
        imgb->ah[i] = ALIGN_VAL(imgb->h[i], OAPV_MB_H);
        imgb->e[i] = imgb->ah[i];

        imgb->bsize[i] = imgb->s[i] * imgb->e[i];
        imgb->a[i] = imgb->baddr[i] = picbuf_alloc(imgb->bsize[i]);
        assert_g(imgb->a[i] != NULL, ERR);
        memset(imgb->a[i], 0, imgb->bsize[i]);
    }
    imgb->cs = cs;
    imgb->addref = imgb_addref;
    imgb->getref = imgb_getref;
    imgb->release = imgb_release;

    imgb->addref(imgb); /* increase reference count */
    return imgb;

ERR:
    logerr("cannot create image buffer\n");
    if(imgb) {
        for(int i = 0; i < OAPV_MAX_CC; i++) {
            if(imgb->a[i])
                picbuf_free(imgb->a[i]);
        }
        free(imgb);
    }
    return NULL;
}

static int imgb_read(FILE *fp, oapv_imgb_t *img, int width, int height, int is_y4m)
{
    int            f_w, f_h;
    unsigned char *p8;

    /* handling Y4M frame header */
    char           t_buf[10];
    if(is_y4m) {
        if(6 != fread(t_buf, 1, 6, fp))
            return -1;
        if(memcmp(t_buf, "FRAME", 5)) {
            logerr("Loss of framing in Y4M input data\n");
            return -1;
        }
        if(t_buf[5] != '\n') {
            logerr("Error parsing Y4M frame header\n");
            return -1;
        }
    }

    /* reading YUV format */
    int chroma_format = OAPV_CS_GET_FORMAT(img->cs);
    int bit_depth = OAPV_CS_GET_BIT_DEPTH(img->cs);
    int w_shift = (chroma_format == OAPV_CF_YCBCR420) || ((chroma_format == OAPV_CF_YCBCR422) || (chroma_format == OAPV_CF_PLANAR2)) ? 1 : 0;
    int h_shift = chroma_format == OAPV_CF_YCBCR420 ? 1 : 0;

    if(bit_depth == 8) {
        f_w = width;
        f_h = height;
    }
    else if(bit_depth >= 10 && bit_depth <= 14) {
        f_w = width * sizeof(short);
        f_h = height;
    }
    else {
        logerr("unsupported bit-depth (%d)\n", bit_depth);
        return -1;
    }

    p8 = (unsigned char *)img->a[0];
    for(int j = 0; j < f_h; j++) {
        if(fread(p8, 1, f_w, fp) != (unsigned)f_w) {
            return -1;
        }
        p8 += img->s[0];
    }

    if(chroma_format == OAPV_CF_PLANAR2) {
        p8 = (unsigned char *)img->a[1];
        for(int j = 0; j < f_h; j++) {
            if(fread(p8, 1, f_w, fp) != (unsigned)f_w) {
                return -1;
            }
            p8 += img->s[1];
        }
    }
    else if(chroma_format != OAPV_CF_YCBCR400) {
        f_w = f_w >> w_shift;
        f_h = f_h >> h_shift;

        p8 = (unsigned char *)img->a[1];
        for(int j = 0; j < f_h; j++) {
            if(fread(p8, 1, f_w, fp) != (unsigned)f_w) {
                return -1;
            }
            p8 += img->s[1];
        }

        p8 = (unsigned char *)img->a[2];
        for(int j = 0; j < f_h; j++) {
            if(fread(p8, 1, f_w, fp) != (unsigned)f_w) {
                return -1;
            }
            p8 += img->s[2];
        }
    }

    if(chroma_format == OAPV_CF_YCBCR4444) {
        f_w = f_w >> w_shift;
        f_h = f_h >> h_shift;

        p8 = (unsigned char *)img->a[3];
        for(int j = 0; j < f_h; j++) {
            if(fread(p8, 1, f_w, fp) != (unsigned)f_w) {
                return -1;
            }
            p8 += img->s[3];
        }
    }

    return 0;
}

static int imgb_write(char *fname, oapv_imgb_t *imgb)
{
    unsigned char *p8;
    int            i, j, bd;
    FILE          *fp;

    int            chroma_format = OAPV_CS_GET_FORMAT(imgb->cs);
    int            bit_depth = OAPV_CS_GET_BIT_DEPTH(imgb->cs);

    fp = fopen(fname, "ab");
    if(fp == NULL) {
        logerr("cannot open file = %s\n", fname);
        return -1;
    }
    if(bit_depth == 8 && (chroma_format == OAPV_CF_YCBCR400 || chroma_format == OAPV_CF_YCBCR420 || chroma_format == OAPV_CF_YCBCR422 ||
                          chroma_format == OAPV_CF_YCBCR444 || chroma_format == OAPV_CF_YCBCR4444)) {
        bd = 1;
    }
    else if(bit_depth >= 10 && bit_depth <= 14 && (chroma_format == OAPV_CF_YCBCR400 || chroma_format == OAPV_CF_YCBCR420 || chroma_format == OAPV_CF_YCBCR422 || chroma_format == OAPV_CF_YCBCR444 || chroma_format == OAPV_CF_YCBCR4444)) {
        bd = 2;
    }
    else if(bit_depth >= 10 && chroma_format == OAPV_CF_PLANAR2) {
        bd = 2;
    }
    else {
        logerr("cannot support the color space\n");
        fclose(fp);
        return -1;
    }

    for(i = 0; i < imgb->np; i++) {
        p8 = (unsigned char *)imgb->a[i] + (imgb->s[i] * imgb->y[i]) + (imgb->x[i] * bd);

        for(j = 0; j < imgb->h[i]; j++) {
            fwrite(p8, imgb->w[i] * bd, 1, fp);
            p8 += imgb->s[i];
        }
    }

    fclose(fp);
    return 0;
}

static void imgb_cpy_plane(oapv_imgb_t *dst, oapv_imgb_t *src)
{
    int            i, j;
    unsigned char *s, *d;
    int            numbyte = OAPV_CS_GET_BYTE_DEPTH(src->cs);

    for(i = 0; i < src->np; i++) {
        s = (unsigned char *)src->a[i];
        d = (unsigned char *)dst->a[i];

        for(j = 0; j < src->ah[i]; j++) {
            memcpy(d, s, numbyte * src->aw[i]);
            s += src->s[i];
            d += dst->s[i];
        }
    }
}

static void imgb_cpy_shift_left_8b(oapv_imgb_t *dst, oapv_imgb_t *src, int shift)
{
    int            i, j, k;

    unsigned char *s;
    short         *d;

    for(i = 0; i < dst->np; i++) {
        s = (unsigned char *)src->a[i];
        d = (short *)dst->a[i];

        for(j = 0; j < src->ah[i]; j++) {
            for(k = 0; k < src->aw[i]; k++) {
                d[k] = (short)(s[k] << shift);
            }
            s = s + src->s[i];
            d = (short *)(((unsigned char *)d) + dst->s[i]);
        }
    }
}

static void imgb_cpy_shift_right_8b(oapv_imgb_t *dst, oapv_imgb_t *src, int shift)
{
    int            i, j, k, t0, add;

    short         *s;
    unsigned char *d;

    if(shift)
        add = 1 << (shift - 1);
    else
        add = 0;

    for(i = 0; i < dst->np; i++) {
        s = (short *)src->a[i];
        d = (unsigned char *)dst->a[i];

        for(j = 0; j < src->ah[i]; j++) {
            for(k = 0; k < src->aw[i]; k++) {
                t0 = ((s[k] + add) >> shift);
                d[k] = (unsigned char)(CLIP_VAL(t0, 0, 255));
            }
            s = (short *)(((unsigned char *)s) + src->s[i]);
            d = d + dst->s[i];
        }
    }
}

static void imgb_cpy_shift_left(oapv_imgb_t *dst, oapv_imgb_t *src, int shift)
{
    int             i, j, k;

    unsigned short *s;
    unsigned short *d;

    for(i = 0; i < dst->np; i++) {
        s = (unsigned short *)src->a[i];
        d = (unsigned short *)dst->a[i];

        for(j = 0; j < src->h[i]; j++) {
            for(k = 0; k < src->w[i]; k++) {
                d[k] = (unsigned short)(s[k] << shift);
            }
            s = (unsigned short *)(((unsigned char *)s) + src->s[i]);
            d = (unsigned short *)(((unsigned char *)d) + dst->s[i]);
        }
    }
}

static void imgb_cpy_shift_right(oapv_imgb_t *dst, oapv_imgb_t *src, int shift)
{
    int             i, j, k, t0, add;

    int             clip_min = 0;
    int             clip_max = 0;

    unsigned short *s;
    unsigned short *d;

    if(shift)
        add = 1 << (shift - 1);
    else
        add = 0;

    clip_max = (1 << (OAPV_CS_GET_BIT_DEPTH(dst->cs))) - 1;

    for(i = 0; i < dst->np; i++) {
        s = (unsigned short *)src->a[i];
        d = (unsigned short *)dst->a[i];

        for(j = 0; j < src->h[i]; j++) {
            for(k = 0; k < src->w[i]; k++) {
                t0 = ((s[k] + add) >> shift);
                d[k] = (CLIP_VAL(t0, clip_min, clip_max));
            }
            s = (unsigned short *)(((unsigned char *)s) + src->s[i]);
            d = (unsigned short *)(((unsigned char *)d) + dst->s[i]);
        }
    }
}

static void imgb_cpy(oapv_imgb_t *dst, oapv_imgb_t *src)
{
    int i, bd_src, bd_dst;
    bd_src = OAPV_CS_GET_BIT_DEPTH(src->cs);
    bd_dst = OAPV_CS_GET_BIT_DEPTH(dst->cs);

    if(src->cs == dst->cs) {
        imgb_cpy_plane(dst, src);
    }
    else if(bd_src == 8 && bd_dst > 8) {
        imgb_cpy_shift_left_8b(dst, src, bd_dst - bd_src);
    }
    else if(bd_src > 8 && bd_dst == 8) {
        imgb_cpy_shift_right_8b(dst, src, bd_src - bd_dst);
    }
    else if(bd_src < bd_dst) {
        imgb_cpy_shift_left(dst, src, bd_dst - bd_src);
    }
    else if(bd_src > bd_dst) {
        imgb_cpy_shift_right(dst, src, bd_src - bd_dst);
    }
    else {
        logerr("ERROR: unsupported image copy\n");
        return;
    }
    for(i = 0; i < OAPV_MAX_CC; i++) {
        dst->x[i] = src->x[i];
        dst->y[i] = src->y[i];
        dst->w[i] = src->w[i];
        dst->h[i] = src->h[i];
        dst->ts[i] = src->ts[i];
    }
}

static void measure_psnr(oapv_imgb_t *org, oapv_imgb_t *rec, double psnr[4], int bit_depth)
{
    double sum[4], mse[4];

    if(bit_depth == 8) {
        unsigned char *o, *r;
        int            i, j, k;

        for(i = 0; i < org->np; i++) {
            o = (unsigned char *)org->a[i];
            r = (unsigned char *)rec->a[i];
            sum[i] = 0;

            for(j = 0; j < org->h[i]; j++) {
                for(k = 0; k < org->w[i]; k++) {
                    sum[i] += (o[k] - r[k]) * (o[k] - r[k]);
                }

                o += org->s[i];
                r += rec->s[i];
            }
            mse[i] = sum[i] / (org->w[i] * org->h[i]);
            psnr[i] = (mse[i] == 0.0) ? 100. : fabs(10 * log10(((255 * 255) / mse[i])));
        }
    }
    else {
        /* more than 8bit, ex) 10bit */
        unsigned short *o, *r;
        int             i, j, k;
        int             factor = 1 << (bit_depth - 8);
        factor *= factor;
        for(i = 0; i < org->np; i++) {
            o = (unsigned short *)org->a[i];
            r = (unsigned short *)rec->a[i];
            sum[i] = 0;
            for(j = 0; j < org->h[i]; j++) {
                for(k = 0; k < org->w[i]; k++) {
                    if(OAPV_CS_GET_FORMAT(org->cs) == OAPV_CF_PLANAR2) {
                        sum[i] += (((int)o[k] - (int)r[k]) >> 6) * (((int)o[k] - (int)r[k]) >> 6);
                    }
                    else {
                        sum[i] += (o[k] - r[k]) * (o[k] - r[k]);
                    }
                }
                o = (unsigned short *)((unsigned char *)o + org->s[i]);
                r = (unsigned short *)((unsigned char *)r + rec->s[i]);
            }
            mse[i] = sum[i] / (org->w[i] * org->h[i]);
            psnr[i] = (mse[i] == 0.0) ? 100. : fabs(10 * log10(((255 * 255 * factor) / mse[i])));
        }
    }
}

static int write_data(char *fname, unsigned char *data, int size)
{
    FILE *fp;

    fp = fopen(fname, "ab");
    if(fp == NULL) {
        logerr("cannot open the output file=%s\n", fname);
        return -1;
    }
    fwrite(data, 1, size, fp);
    fclose(fp);
    return 0;
}

static int clear_data(char *fname)
{
    FILE *fp;
    fp = fopen(fname, "wb");
    if(fp == NULL) {
        logerr("cannot remove file (%s)\n", fname);
        return -1;
    }
    fclose(fp);
    return 0;
}
static unsigned char char_to_hex(char a)
{
    unsigned char ret;

    switch(a) {
    case 'a':
    case 'A':
        ret = 10;
        break;
    case 'b':
    case 'B':
        ret = 11;
        break;
    case 'c':
    case 'C':
        ret = 12;
        break;
    case 'd':
    case 'D':
        ret = 13;
        break;
    case 'e':
    case 'E':
        ret = 14;
        break;
    case 'f':
    case 'F':
        ret = 15;
        break;
    default:
        ret = (unsigned char)a - '0';
        break;
    }
    return ret;
}

#endif /* _OAPV_APP_UTIL_H_ */
