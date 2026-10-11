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

// Rust counterparts of the helper macros and inline functions of
// oapv_def.h and oapv_util.h that bindgen cannot translate, shared by the
// modules of the crate.

use crate::ffi;

pub const OAPV_KPARAM_DC_MIN: i32 = ffi::OAPV_KPARAM_DC_MIN as i32;
pub const OAPV_KPARAM_DC_MAX: i32 = ffi::OAPV_KPARAM_DC_MAX as i32;
pub const OAPV_KPARAM_AC_MIN: i32 = ffi::OAPV_KPARAM_AC_MIN as i32;
pub const OAPV_KPARAM_AC_MAX: i32 = ffi::OAPV_KPARAM_AC_MAX as i32;
pub const OAPV_KPARAM_RUN_MIN: i32 = ffi::OAPV_KPARAM_RUN_MIN as i32;
pub const OAPV_KPARAM_RUN_MAX: i32 = ffi::OAPV_KPARAM_RUN_MAX as i32;
pub const MIN_QUANT: i32 = ffi::MIN_QUANT as i32;
pub const MAX_TX_VAL: i32 = (1 << ffi::MAX_TX_DYNAMIC_RANGE) - 1;
pub const MIN_TX_VAL: i32 = -(1 << ffi::MAX_TX_DYNAMIC_RANGE);

pub fn max_quant(bd: i32) -> i32 {
    63 + (bd - 10) * 6
}

pub fn oapv_min(a: i32, b: i32) -> i32 {
    if a < b { a } else { b }
}

pub fn oapv_max(a: i32, b: i32) -> i32 {
    if a > b { a } else { b }
}

pub fn oapv_abs32(a: i32) -> i32 {
    (a ^ (a >> 31)) - (a >> 31)
}

pub fn oapv_abs16(a: i32) -> i32 {
    (a ^ (a >> 15)) - (a >> 15)
}

pub fn oapv_clip3(min: i32, max: i32, val: i32) -> i32 {
    oapv_max(min, oapv_min(max, val))
}

pub fn oapv_set_sign(val: i32, sign: i32) -> i32 {
    if sign != 0 { -val } else { val }
}

pub fn oapv_set_sign16(val: i32, sign: i32) -> i32 {
    (val ^ (((sign << 15) as i16 as i32) >> 15)) + sign
}

pub fn oapv_get_sign32(val: i32) -> i32 {
    (val >> 31) & 1
}

pub fn oapv_get_sign16(val: i32) -> i32 {
    (val >> 15) & 1
}

pub fn oapv_align_value(val: i32, align: i32) -> i32 {
    (val + align - 1) / align * align
}

pub fn oapv_div_round_up(n: i32, d: i32) -> i32 {
    (n + d - 1) / d
}

pub fn kparam_dc_of(level: i32) -> i32 {
    oapv_min(level >> 1, OAPV_KPARAM_DC_MAX)
}

pub fn kparam_ac_of(level: i32) -> i32 {
    oapv_min(level >> 2, OAPV_KPARAM_AC_MAX)
}

pub fn kparam_run_of(run: i32) -> i32 {
    oapv_min(run >> 2, OAPV_KPARAM_RUN_MAX)
}

pub fn cs_get_format(cs: i32) -> i32 {
    cs & 0xFF
}

pub fn cs_set(f: i32, bit: i32, e: i32) -> i32 {
    (e << 14) | (bit << 8) | f
}

pub fn chroma_format_idc_to_color_format(cfi: i32) -> i32 {
    (match cfi {
        0 => ffi::OAPV_CF_YCBCR400,
        1 => ffi::OAPV_CF_YCBCR420,
        2 => ffi::OAPV_CF_YCBCR422,
        3 => ffi::OAPV_CF_YCBCR444,
        _ => ffi::OAPV_CF_YCBCR4444,
    }) as i32
}

pub fn color_format_to_chroma_format_idc(cf: i32) -> i32 {
    match cf as u32 {
        ffi::OAPV_CF_PLANAR2 => 2,
        ffi::OAPV_CF_YCBCR400 => 0,
        ffi::OAPV_CF_YCBCR420 => 1,
        ffi::OAPV_CF_YCBCR422 => 2,
        ffi::OAPV_CF_YCBCR444 => 3,
        _ => 4,
    }
}

pub fn get_chroma_sft_w(cfi: i32) -> i32 {
    if cfi == 0 || cfi == 1 || cfi == 2 { 1 } else { 0 }
}

pub fn get_chroma_sft_h(cfi: i32) -> i32 {
    if cfi == 0 || cfi == 1 { 1 } else { 0 }
}

pub fn get_num_comp(cfi: i32) -> i32 {
    match cfi {
        0 => 1,
        4 => 4,
        _ => 3,
    }
}

pub fn pbu_type_is_frame(pbu_type: i32) -> bool {
    match pbu_type as u32 {
        ffi::OAPV_PBU_TYPE_PRIMARY_FRAME
        | ffi::OAPV_PBU_TYPE_NON_PRIMARY_FRAME
        | ffi::OAPV_PBU_TYPE_PREVIEW_FRAME
        | ffi::OAPV_PBU_TYPE_DEPTH_FRAME
        | ffi::OAPV_PBU_TYPE_ALPHA_FRAME => true,
        _ => false,
    }
}

pub fn is_unconst_profile(idc: i32) -> bool {
    match idc as u32 {
        ffi::OAPV_PROFILE_422_10_UNCONST
        | ffi::OAPV_PROFILE_422_12_UNCONST
        | ffi::OAPV_PROFILE_444_10_UNCONST
        | ffi::OAPV_PROFILE_444_12_UNCONST
        | ffi::OAPV_PROFILE_4444_10_UNCONST
        | ffi::OAPV_PROFILE_4444_12_UNCONST
        | ffi::OAPV_PROFILE_400_10_UNCONST => true,
        _ => false,
    }
}

pub fn oapv_validate_tile_topology(profile_idc: i32, tile_cols: i32, tile_rows: i32, num_tiles: Option<&mut i32>) -> i32 {
    let unconst = is_unconst_profile(profile_idc);
    if tile_cols < 1 || (!unconst && tile_cols > ffi::OAPV_MAX_TILE_COLS as i32) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    if tile_rows < 1 || (!unconst && tile_rows > ffi::OAPV_MAX_TILE_ROWS as i32) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    // cols and rows can be up to 2^20 each for UNCONST profiles
    let n = tile_cols as i64 * tile_rows as i64;
    if n > if unconst { i32::MAX as i64 } else { ffi::OAPV_MAX_TILES as i64 } {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    if let Some(p) = num_tiles {
        *p = n as i32;
    }
    ffi::OAPV_OK as i32
}

pub fn oapv_validate_payload_bounds(remaining: u64, header: u64, payload: u64) -> i32 {
    if remaining < header || payload > remaining - header {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    ffi::OAPV_OK as i32
}
