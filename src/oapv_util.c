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

#include "oapv_util.h"
#include <math.h>

/* MD5 functions */
#define MD5FUNC(f, w, x, y, z, msg1, s, msg2) \
    (w += f(x, y, z) + msg1 + msg2, w = w << s | w >> (32 - s), w += x)
#define FF(x, y, z) (z ^ (x & (y ^ z)))
#define GG(x, y, z) (y ^ (z & (x ^ y)))
#define HH(x, y, z) (x ^ y ^ z)
#define II(x, y, z) (y ^ (x | ~z))

static void md5_trans(u32 *buf, u32 *msg)
{
    register u32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5FUNC(FF, a, b, c, d, msg[0], 7, 0xd76aa478);  /* 1 */
    MD5FUNC(FF, d, a, b, c, msg[1], 12, 0xe8c7b756); /* 2 */
    MD5FUNC(FF, c, d, a, b, msg[2], 17, 0x242070db); /* 3 */
    MD5FUNC(FF, b, c, d, a, msg[3], 22, 0xc1bdceee); /* 4 */

    MD5FUNC(FF, a, b, c, d, msg[4], 7, 0xf57c0faf);  /* 5 */
    MD5FUNC(FF, d, a, b, c, msg[5], 12, 0x4787c62a); /* 6 */
    MD5FUNC(FF, c, d, a, b, msg[6], 17, 0xa8304613); /* 7 */
    MD5FUNC(FF, b, c, d, a, msg[7], 22, 0xfd469501); /* 8 */

    MD5FUNC(FF, a, b, c, d, msg[8], 7, 0x698098d8);   /* 9 */
    MD5FUNC(FF, d, a, b, c, msg[9], 12, 0x8b44f7af);  /* 10 */
    MD5FUNC(FF, c, d, a, b, msg[10], 17, 0xffff5bb1); /* 11 */
    MD5FUNC(FF, b, c, d, a, msg[11], 22, 0x895cd7be); /* 12 */

    MD5FUNC(FF, a, b, c, d, msg[12], 7, 0x6b901122);  /* 13 */
    MD5FUNC(FF, d, a, b, c, msg[13], 12, 0xfd987193); /* 14 */
    MD5FUNC(FF, c, d, a, b, msg[14], 17, 0xa679438e); /* 15 */
    MD5FUNC(FF, b, c, d, a, msg[15], 22, 0x49b40821); /* 16 */

    /* Round 2 */
    MD5FUNC(GG, a, b, c, d, msg[1], 5, 0xf61e2562);   /* 17 */
    MD5FUNC(GG, d, a, b, c, msg[6], 9, 0xc040b340);   /* 18 */
    MD5FUNC(GG, c, d, a, b, msg[11], 14, 0x265e5a51); /* 19 */
    MD5FUNC(GG, b, c, d, a, msg[0], 20, 0xe9b6c7aa);  /* 20 */

    MD5FUNC(GG, a, b, c, d, msg[5], 5, 0xd62f105d);   /* 21 */
    MD5FUNC(GG, d, a, b, c, msg[10], 9, 0x2441453);   /* 22 */
    MD5FUNC(GG, c, d, a, b, msg[15], 14, 0xd8a1e681); /* 23 */
    MD5FUNC(GG, b, c, d, a, msg[4], 20, 0xe7d3fbc8);  /* 24 */

    MD5FUNC(GG, a, b, c, d, msg[9], 5, 0x21e1cde6);  /* 25 */
    MD5FUNC(GG, d, a, b, c, msg[14], 9, 0xc33707d6); /* 26 */
    MD5FUNC(GG, c, d, a, b, msg[3], 14, 0xf4d50d87); /* 27 */
    MD5FUNC(GG, b, c, d, a, msg[8], 20, 0x455a14ed); /* 28 */

    MD5FUNC(GG, a, b, c, d, msg[13], 5, 0xa9e3e905);  /* 29 */
    MD5FUNC(GG, d, a, b, c, msg[2], 9, 0xfcefa3f8);   /* 30 */
    MD5FUNC(GG, c, d, a, b, msg[7], 14, 0x676f02d9);  /* 31 */
    MD5FUNC(GG, b, c, d, a, msg[12], 20, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    MD5FUNC(HH, a, b, c, d, msg[5], 4, 0xfffa3942);   /* 33 */
    MD5FUNC(HH, d, a, b, c, msg[8], 11, 0x8771f681);  /* 34 */
    MD5FUNC(HH, c, d, a, b, msg[11], 16, 0x6d9d6122); /* 35 */
    MD5FUNC(HH, b, c, d, a, msg[14], 23, 0xfde5380c); /* 36 */

    MD5FUNC(HH, a, b, c, d, msg[1], 4, 0xa4beea44);   /* 37 */
    MD5FUNC(HH, d, a, b, c, msg[4], 11, 0x4bdecfa9);  /* 38 */
    MD5FUNC(HH, c, d, a, b, msg[7], 16, 0xf6bb4b60);  /* 39 */
    MD5FUNC(HH, b, c, d, a, msg[10], 23, 0xbebfbc70); /* 40 */

    MD5FUNC(HH, a, b, c, d, msg[13], 4, 0x289b7ec6); /* 41 */
    MD5FUNC(HH, d, a, b, c, msg[0], 11, 0xeaa127fa); /* 42 */
    MD5FUNC(HH, c, d, a, b, msg[3], 16, 0xd4ef3085); /* 43 */
    MD5FUNC(HH, b, c, d, a, msg[6], 23, 0x4881d05);  /* 44 */

    MD5FUNC(HH, a, b, c, d, msg[9], 4, 0xd9d4d039);   /* 45 */
    MD5FUNC(HH, d, a, b, c, msg[12], 11, 0xe6db99e5); /* 46 */
    MD5FUNC(HH, c, d, a, b, msg[15], 16, 0x1fa27cf8); /* 47 */
    MD5FUNC(HH, b, c, d, a, msg[2], 23, 0xc4ac5665);  /* 48 */

    /* Round 4 */
    MD5FUNC(II, a, b, c, d, msg[0], 6, 0xf4292244);   /* 49 */
    MD5FUNC(II, d, a, b, c, msg[7], 10, 0x432aff97);  /* 50 */
    MD5FUNC(II, c, d, a, b, msg[14], 15, 0xab9423a7); /* 51 */
    MD5FUNC(II, b, c, d, a, msg[5], 21, 0xfc93a039);  /* 52 */

    MD5FUNC(II, a, b, c, d, msg[12], 6, 0x655b59c3);  /* 53 */
    MD5FUNC(II, d, a, b, c, msg[3], 10, 0x8f0ccc92);  /* 54 */
    MD5FUNC(II, c, d, a, b, msg[10], 15, 0xffeff47d); /* 55 */
    MD5FUNC(II, b, c, d, a, msg[1], 21, 0x85845dd1);  /* 56 */

    MD5FUNC(II, a, b, c, d, msg[8], 6, 0x6fa87e4f);   /* 57 */
    MD5FUNC(II, d, a, b, c, msg[15], 10, 0xfe2ce6e0); /* 58 */
    MD5FUNC(II, c, d, a, b, msg[6], 15, 0xa3014314);  /* 59 */
    MD5FUNC(II, b, c, d, a, msg[13], 21, 0x4e0811a1); /* 60 */

    MD5FUNC(II, a, b, c, d, msg[4], 6, 0xf7537e82);   /* 61 */
    MD5FUNC(II, d, a, b, c, msg[11], 10, 0xbd3af235); /* 62 */
    MD5FUNC(II, c, d, a, b, msg[2], 15, 0x2ad7d2bb);  /* 63 */
    MD5FUNC(II, b, c, d, a, msg[9], 21, 0xeb86d391);  /* 64 */

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

static void md5_init(oapv_md5_t *md5)
{
    md5->h[0] = 0x67452301;
    md5->h[1] = 0xefcdab89;
    md5->h[2] = 0x98badcfe;
    md5->h[3] = 0x10325476;

    md5->bits[0] = 0;
    md5->bits[1] = 0;
}

static void md5_update(oapv_md5_t *md5, void *buf_t, u32 len)
{
    u8 *buf;
    u32 i, idx, part_len;

    buf = (u8 *)buf_t;

    idx = (u32)((md5->bits[0] >> 3) & 0x3f);

    md5->bits[0] += (len << 3);
    if(md5->bits[0] < (len << 3)) {
        (md5->bits[1])++;
    }

    md5->bits[1] += (len >> 29);
    part_len = 64 - idx;

    if(len >= part_len) {
        oapv_mcpy(md5->msg + idx, buf, part_len);
        md5_trans(md5->h, (u32 *)md5->msg);

        for(i = part_len; i + 63 < len; i += 64) {
            md5_trans(md5->h, (u32 *)(buf + i));
        }
        idx = 0;
    }
    else {
        i = 0;
    }

    if(len - i > 0) {
        oapv_mcpy(md5->msg + idx, buf + i, len - i);
    }
}

static void md5_update_16(oapv_md5_t *md5, void *buf_t, u32 len)
{
    u16 *buf;
    u32  i, idx, part_len, j;
    u8   t[512];

    buf = (u16 *)buf_t;
    idx = (u32)((md5->bits[0] >> 3) & 0x3f);

    len = len * 2;
    for(j = 0; j < len; j += 2) {
        t[j] = (u8)(*(buf));
        t[j + 1] = *(buf) >> 8;
        buf++;
    }

    md5->bits[0] += (len << 3);
    if(md5->bits[0] < (len << 3)) {
        (md5->bits[1])++;
    }

    md5->bits[1] += (len >> 29);
    part_len = 64 - idx;

    if(len >= part_len) {
        oapv_mcpy(md5->msg + idx, t, part_len);
        md5_trans(md5->h, (u32 *)md5->msg);

        for(i = part_len; i + 63 < len; i += 64) {
            md5_trans(md5->h, (u32 *)(t + i));
        }
        idx = 0;
    }
    else {
        i = 0;
    }

    if(len - i > 0) {
        oapv_mcpy(md5->msg + idx, t + i, len - i);
    }
}

static void md5_finish(oapv_md5_t *md5, u8 digest[16])
{
    u8 *pos;
    int cnt;

    cnt = (md5->bits[0] >> 3) & 0x3F;
    pos = md5->msg + cnt;
    *pos++ = 0x80;
    cnt = 64 - 1 - cnt;

    if(cnt < 8) {
        oapv_mset(pos, 0, cnt);
        md5_trans(md5->h, (u32 *)md5->msg);
        oapv_mset(md5->msg, 0, 56);
    }
    else {
        oapv_mset(pos, 0, cnt - 8);
    }

    oapv_mcpy((md5->msg + 14 * sizeof(u32)), &md5->bits[0], sizeof(u32));
    oapv_mcpy((md5->msg + 15 * sizeof(u32)), &md5->bits[1], sizeof(u32));

    md5_trans(md5->h, (u32 *)md5->msg);
    oapv_mcpy(digest, md5->h, 16);
    oapv_mset(md5, 0, sizeof(oapv_md5_t));
}

static unsigned char uuid_frm_hash[16] = {
    0xf8, 0x72, 0x1b, 0x3e, 0xcd, 0xee, 0x47, 0x21,
    0x98, 0x0d, 0x9b, 0x9e, 0x39, 0x20, 0x28, 0x49
};

// checks imgb self-consistency; returns OAPV_OK or a specific error code
int oapv_imgb_is_valid(oapv_imgb_t *imgb)
{
    if(imgb == NULL) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    if(imgb->np < 1 || imgb->np > OAPV_MAX_CC) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    for(int i = 0; i < imgb->np; i++) {
        if(imgb->a[i] == NULL) {
            return OAPV_ERR_INVALID_ARGUMENT;
        }
        if(imgb->aw[i] < 0 || (s64)imgb->s[i] < (s64)imgb->aw[i] * 2) {
            return OAPV_ERR_INVALID_WIDTH;
        }
        if(imgb->ah[i] < 0 || (s64)imgb->bsize[i] < (s64)imgb->ah[i] * imgb->s[i]) {
            return OAPV_ERR_INVALID_HEIGHT;
        }
    }
    return OAPV_OK;
}

int oapv_imgb_set_md5(oapv_imgb_t *imgb)
{
    oapv_md5_t md5[N_C];
    int        i, j, ret;

    ret = oapv_imgb_is_valid(imgb);
    if(OAPV_FAILED(ret)) {
        return ret;
    }
    memset(imgb->hash, 0, sizeof(imgb->hash));

    for(i = 0; i < imgb->np; i++) {
        md5_init(&md5[i]);

        for(j = 0; j < imgb->ah[i]; j++) {
            md5_update(&md5[i], ((u8 *)imgb->a[i]) + j * imgb->s[i], imgb->aw[i] * 2);
        }

        md5_finish(&md5[i], imgb->hash[i]);
    }
    return OAPV_OK;
}

void oapv_imgb_clr_md5(oapv_imgb_t *imgb)
{
    oapv_mset(imgb->hash, 0, sizeof(imgb->hash));
}

int oapv_set_md5_pld(oapvm_t mid, int group_id, oapv_imgb_t *rec)
{
    int ret = oapv_imgb_set_md5(rec);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    oapvm_ctx_t *mctx = oapvm_id_to_ctx(mid);
    oapv_assert_rv(mctx != NULL, OAPV_ERR_INVALID_ARGUMENT);
    u8 *mdp_data = oapv_ops_malloc(mctx, (16 * rec->np) + 16);
    oapv_assert_rv(mdp_data != NULL, OAPV_ERR_OUT_OF_MEMORY);
    memcpy(mdp_data, uuid_frm_hash, 16);
    for(int i = 0; i < rec->np; i++) {
        memcpy(mdp_data + ((i + 1) * 16), rec->hash[i], 16);
    }
    ret = oapvm_set(mid, group_id, OAPV_METADATA_USER_DEFINED, mdp_data, 16 * rec->np + 16);
    oapv_ops_free(mctx, mdp_data);
    oapv_assert_rv(OAPV_SUCCEEDED(ret), ret);
    return OAPV_OK;
}

void oapv_block_copy(s16 *src, int src_stride, s16 *dst, int dst_stride, int log2_copy_w, int log2_copy_h)
{
    int  h;
    int  copy_size = (1 << log2_copy_w) * (int)sizeof(s16);
    s16 *tmp_src = src;
    s16 *tmp_dst = dst;
    for(h = 0; h < (1 << log2_copy_h); h++) {
        oapv_mcpy(tmp_dst, tmp_src, copy_size);
        tmp_dst += dst_stride;
        tmp_src += src_stride;
    }
}

#if X86_SSE
#define OAPV_CPU_INFO_SSE2    0x7A // ((3 << 5) | 26)
#define OAPV_CPU_INFO_SSE3    0x40 // ((2 << 5) |  0)
#define OAPV_CPU_INFO_SSSE3   0x49 // ((2 << 5) |  9)
#define OAPV_CPU_INFO_SSE41   0x53 // ((2 << 5) | 19)
#define OAPV_CPU_INFO_OSXSAVE 0x5B // ((2 << 5) | 27)
#define OAPV_CPU_INFO_AVX     0x5C // ((2 << 5) | 28)
#define OAPV_CPU_INFO_AVX2    0x25 // ((1 << 5) |  5)

#if(defined(_WIN64) || defined(_WIN32)) && !defined(__GNUC__)
#include <intrin.h >
#elif defined(__GNUC__)
#ifndef _XCR_XFEATURE_ENABLED_MASK
#define _XCR_XFEATURE_ENABLED_MASK 0
#endif

static void __cpuid(int *info, int i)
{
    __asm__ __volatile__(
        "cpuid" : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(i), "c"(0));
}

static unsigned long long __xgetbv(unsigned int i)
{
    unsigned int eax, edx;
    // clang-format off
    __asm__ __volatile__(
        "xgetbv;" : "=a" (eax), "=d"(edx)
                  : "c" (i));
    return ((unsigned long long)edx << 32) | eax;
    // clang-format on
}
#endif

#define GET_CPU_INFO(A, B) ((B[((A >> 5) & 0x03)] >> (A & 0x1f)) & 1)

int oapv_check_cpu_info_x86()
{
    int support_sse = 0;
    int support_avx = 0;
    int support_avx2 = 0;
    int cpu_info[4] = { 0 };
    __cpuid(cpu_info, 0);
    int id_cnt = cpu_info[0];

    if(id_cnt >= 1) {
        __cpuid(cpu_info, 1);
        support_sse = GET_CPU_INFO(OAPV_CPU_INFO_SSE41, cpu_info);
        int os_use_xsave = GET_CPU_INFO(OAPV_CPU_INFO_OSXSAVE, cpu_info);
        int cpu_support_avx = GET_CPU_INFO(OAPV_CPU_INFO_AVX, cpu_info);

        if(os_use_xsave && cpu_support_avx) {
            unsigned long long xcr_feature_mask = __xgetbv(_XCR_XFEATURE_ENABLED_MASK);
            support_avx = ((xcr_feature_mask & 0x6) || 0) ? 1 : 0;
            if(id_cnt >= 7) {
                __cpuid(cpu_info, 7);
                support_avx2 = (support_avx && GET_CPU_INFO(OAPV_CPU_INFO_AVX2, cpu_info)) ? 1 : 0;
            }
        }
    }

    return ((support_avx2 << 2) | (support_avx << 1) | (support_sse << 0));
}
#endif

#if ENC_DEC_DUMP
#include <stdarg.h>
FILE *oapv_fp_dump;
int   oapv_is_dump;
;

void oapv_dump_string0(int cond, const char *fmt, ...)
{
    if(!oapv_is_dump)
        return;
    switch(cond) {
    case OAPV_DUMP_HLS:
        if(!DUMP_ENABLE_HLS)
            return;
        break;
    case OAPV_DUMP_COEF:
        if(!DUMP_ENABLE_COEF)
            return;
        break;
    default:
        break;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(oapv_fp_dump, fmt, args);
    fflush(oapv_fp_dump);
    va_end(args);
}

void oapv_dump_coef0(int dc, short *coef, int size, int x, int y, int c)
{
    if(!DUMP_ENABLE_COEF || !oapv_is_dump)
        return;

    fprintf(oapv_fp_dump, "x pos : % d y pos : % d comp : % d\n", x, y, c);
    fprintf(oapv_fp_dump, "coef:");
    fprintf(oapv_fp_dump, " %d", dc);
    for(int i = 0; i < size; i++) {
        fprintf(oapv_fp_dump, " %d", coef[i]);
    }
    fprintf(oapv_fp_dump, "\n");
    fflush(oapv_fp_dump);
}

void oapv_dump_create0(int is_enc)
{
    if(is_enc) {
        if(DUMP_ENABLE_HLS || DUMP_ENABLE_COEF) {
            oapv_fp_dump = fopen("enc_dump.txt", "w+");
        }
    }
    else {
        if(DUMP_ENABLE_HLS || DUMP_ENABLE_COEF) {
            oapv_fp_dump = fopen("dec_dump.txt", "w+");
        }
    }
    oapv_is_dump = 1;
}

void oapv_dump_delete0()
{
    if(DUMP_ENABLE_HLS || DUMP_ENABLE_COEF) {
        fclose(oapv_fp_dump);
    }
}
#endif
