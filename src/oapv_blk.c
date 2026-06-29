/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
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

/*****************************************************************************/
/* macro for companding (compressing and expanding)                          */
/*****************************************************************************/
/* Compressing 16-bit pixel values to 12-bit by the companding method:
   if 'count_leading_zero()' can be supported, the following pseudo code could be efficient.

        if(p16 < 1024) p12 = p16;
        else {
            int k = (16 - count_leading_zero(p16)) - 9;
            p12 = (p16 >> k) + (512 * k);
        }
*/
// input (p16): 16bit unsigned, return: 12bit unsigned
#define OAPV_COMP_16C12(p16) \
    (((p16) < 1024)? (p16) : \
    ((p16) < 2048)? 1024 + (((p16) - 1024) >> 1) : \
    ((p16) < 4096)? 1536 + (((p16) - 2048) >> 2) : \
    ((p16) < 8192)? 2048 + (((p16) - 4096) >> 3) : \
    ((p16) < 16384)? 2560 + (((p16) - 8192) >> 4) : \
    ((p16) < 32768)? 3072 + (((p16) - 16384) >> 5) : \
    /*((p16) < 65537)?*/ 3584 + (((p16) - 32768) >> 6))


/* Expanding 12-bit pixel values to 16-bit by the companding method:
   the following pseudo code is an equation form'

        if(p12 < 1024) p16 = p12;
        else {
            int m = (int)(p12 / 512);
            p16 = (p12 - m * 512) << m;
        }
*/
// input (p12): 12bit unsigned, return: 16bit unsigned
#define OAPV_COMP_12E16(p12) \
    (((p12) < 1024)? (p12) : \
    ((p12) < 1536)? 1024 + (((p12) - 1024) << 1) : \
    ((p12) < 2048)? 2048 + (((p12) - 1536) << 2) : \
    ((p12) < 2560)? 4096 + (((p12) - 2048) << 3) : \
    ((p12) < 3072)? 8192 + (((p12) - 2560) << 4) : \
    ((p12) < 3584)? 16384 + (((p12) - 3072) << 5) : \
    /*((p12) < 4096)?*/ 32768 + (((p12) - 3584) << 6))

void oapv_blk_from_pic_16(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val)
{
    u16      *s = (u16 *)pic;
    s16      *d = (s16 *)blk;

    for(int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                d[i] = s[i] - mid_val;
            }

        s = (u16 *)(((u8 *)s) + pic_s);
        d = (s16 *)(((u8 *)d) + blk_s);
    }
}

void oapv_blk_from_pic_16C12(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val)
{
    u16      *s = (u16 *)pic;
    s16      *d = (s16 *)blk;

    u16 p12;

    for(int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            p12 = OAPV_COMP_16C12(s[i]);
            d[i] = p12 - mid_val;
        }
        s = (u16 *)(((u8 *)s) + pic_s);
        d = (s16 *)(((u8 *)d) + blk_s);
    }
}

void oapv_blk_from_pic_p21x_y(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val)
{
    u16      *s = (u16 *)pic;
    s16      *d = (s16 *)blk;
    int       shift_pic_bits = 16 - bd;

    for(int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            d[i] = (s16)(s[i] >> shift_pic_bits) - mid_val;
        }
        s = (u16 *)(((u8 *)s) + pic_s);
        d = (s16 *)(((u8 *)d) + blk_s);
    }
}

void oapv_blk_from_pic_p21x_uv(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val)
{
    u16      *s = (u16 *)pic + pic_x;
    s16      *d = (s16 *)blk;
    int       shift_pic_bits = 16 - bd;

    for(int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            d[i] = (s16)(s[i * 2] >> shift_pic_bits) - mid_val;
        }
        s = (u16 *)(((u8 *)s) + pic_s);
        d = (s16 *)(((u8 *)d) + blk_s);
    }
}

void oapv_blk_to_pic_16(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const int max_val = (1 << bd) - 1;
    const int mid_val = (1 << (bd - 1));
    s16      *s = (s16 *)blk;
    u16      *d = (u16 *)pic;

    for(int j = 0; j < h; j++) {
        for(int i = 0; i < w; i++) {
            d[i] = oapv_clip3(0, max_val, s[i] + mid_val);
        }
        s = (s16 *)(((u8 *)s) + blk_s);
        d = (u16 *)(((u8 *)d) + pic_s);
    }
}
void oapv_blk_to_pic_12E16(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const int max_val = (1 << bd) - 1;
    const int mid_val = (1 << (bd - 1));
    s16      *s = (s16 *)blk;
    u16      *d = (u16 *)pic;

    u16 p12;
    for(int j = 0; j < h; j++) {
        for(int i = 0; i < w; i++) {
            p12 = oapv_clip3(0, max_val, s[i] + mid_val);
            d[i] = OAPV_COMP_12E16(p12);
        }
        s = (s16 *)(((u8 *)s) + blk_s);
        d = (u16 *)(((u8 *)d) + pic_s);
    }
}

void oapv_blk_to_pic_p21x_y(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const int max_val = (1 << bd) - 1;
    const int mid_val = (1 << (bd - 1));
    s16      *s = (s16 *)blk;
    u16      *d = (u16 *)pic;
    int       shift_pic_bits = 16 - bd;

    for(int j = 0; j < h; j++) {
        for(int i = 0; i < w; i++) {
            d[i] = oapv_clip3(0, max_val, s[i] + mid_val) << shift_pic_bits;
        }
        s = (s16 *)(((u8 *)s) + blk_s);
        d = (u16 *)(((u8 *)d) + pic_s);
    }
}

void oapv_blk_to_pic_p21x_uv(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd)
{
    const int max_val = (1 << bd) - 1;
    const int mid_val = (1 << (bd - 1));
    s16      *s = (s16 *)blk;
    int       shift_pic_bits = 16 - bd;

    // pic_x is x-offset value from left boundary of picture in unit of pixel.
    // the 'pic' address has calculated by
    // pic = (s16*)((u8*)origin + y_pel*pic_s) + pic_x;
    // in case of P210 color format,
    // since 'pic_s' is byte size of stride including all U and V pixel values,
    // y-offset calculation is correct.
    // however, the adding only pic_x is not enough to address the correct pixel
    // position of U or V because U & V use the same buffer plane
    // in interleaved way,
    // so, the 'pic' address should be increased by 'pic_x' to address pixel
    // position correctly.
    u16      *d = (u16 *)pic + pic_x; // p210 pixel value needs 0~65535 range

    for(int j = 0; j < h; j++) {
        for(int i = 0; i < w; i++) {
            d[i * 2] = ((u16)oapv_clip3(0, max_val, s[i] + mid_val)) << shift_pic_bits;
        }
        s = (s16 *)(((u8 *)s) + blk_s);
        d = (u16 *)(((u8 *)d) + pic_s);
    }
}
