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

#ifndef OPENAPV_OAPV_TQ_H_
#define OPENAPV_OAPV_TQ_H_

#include "oapv_def.h"

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

extern const oapv_fn_tx_t    oapv_tbl_fn_tx[2];
extern const oapv_fn_quant_t oapv_tbl_fn_quant[2];
extern const int             oapv_quant_scale[6];

void oapv_trans(oapve_ctx_t *ctx, s16 *coef, int log2_w, int log2_h, int bit_depth);
void oapv_itx_get_wo_sft(s16 *src, s16 *dst, s32 *dst32, int shift, int line);
void oapve_init_rdoq(oapve_core_t* core, int bit_depth, int ch_type);
int  oapve_rdoq(oapve_core_t* core, s16* src_coef, s16* dst_coef, int log2_cuw, int log2_cuh, int ch_type, int bit_depth, double lambda);

///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////
#define ITX_SHIFT1            (7)                    /* shift after 1st IT stage */
#define ITX_SHIFT2(bit_depth) (12 - (bit_depth - 8)) /* shift after 2nd IT stage */

#define ITX_CLIP(x) \
    (s16)(((x) < MIN_TX_VAL) ? MIN_TX_VAL : (((x) > MAX_TX_VAL) ? MAX_TX_VAL : (x)))

#define MAX_TX_DYNAMIC_RANGE_32 (31)
#define MAX_TX_VAL_32           (2147483647)
#define MIN_TX_VAL_32           (-2147483647 - 1)
#define ITX_CLIP_32(x) \
    (s32)(((x) <= MIN_TX_VAL_32) ? MIN_TX_VAL_32 : (((x) >= MAX_TX_VAL_32) ? MAX_TX_VAL_32 : (x)))

extern const oapv_fn_itx_part_t oapv_tbl_fn_itx_part[2];
extern const oapv_fn_itx_t      oapv_tbl_fn_itx[2];
extern const oapv_fn_dquant_t   oapv_tbl_fn_dquant[2];
extern const oapv_fn_itx_adj_t  oapv_tbl_fn_itx_adj[2];

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

#endif /* OPENAPV_OAPV_TQ_H_ */
