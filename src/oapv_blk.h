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

#ifndef __OAPV_BLK_H_3243243920473824893204832876483924637286__
#define __OAPV_BLK_H_3243243920473824893204832876483924637286__

#include "oapv_port.h"

void oapv_blk_from_imgb(oapv_imgb_t *imgb, int c, int x_l, int y_l, int w_l, int h_l, s16 *blk, int bd, int comp);
void oapv_blk_from_imgb_p21x(oapv_imgb_t *imgb, int c, int x_l, int y_l, int w_l, int h_l, s16 *blk, int bd, int comp);

void oapv_blk_from_pic_16(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val);
void oapv_blk_from_pic_16C12(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val);
void oapv_blk_from_pic_p21x_y(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val);
void oapv_blk_from_pic_p21x_uv(int w, int h, void *pic, int pic_x, int pic_s, void *blk, int blk_s, int bd, int mid_val);
void oapv_blk_to_pic_16(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd);
void oapv_blk_to_pic_12E16(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd);
void oapv_blk_to_pic_p21x_y(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd);
void oapv_blk_to_pic_p21x_uv(int w, int h, void *blk, int blk_s, void *pic, int pic_x, int pic_s, int bd);

#endif /* __OAPV_BLK_H_3243243920473824893204832876483924637286__ */
