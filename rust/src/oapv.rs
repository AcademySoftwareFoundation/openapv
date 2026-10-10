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
/*
 * Rust port of selected oapv.c decoder functions; generates the same
 * symbols as the C definitions gated by OAPV_RUST_DEC in oapv.c.
 * Crate manifest and bindgen config live in rust/.
 */

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

use core::ffi::c_int;

macro_rules! ffi_try {
    ($body:expr, $on_err:expr) => {{
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| { unsafe { $body } }));
        match r {
            Ok(v) => v,
            Err(_) => $on_err,
        }
    }};
}

mod ffi {
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

const Y_C: usize = 0;
const U_C: usize = 1;
const V_C: usize = 2;
const OAPV_CF_PLANAR2: i32 = 20;
const DEC_TILE_STAT_FLAG_DO: i32 = 1 << 4;

fn cs_get_format(cs: c_int) -> i32 {
    cs & 0xFF
}

fn color_format_to_chroma_format_idc(cf: i32) -> i32 {
    if cf == OAPV_CF_PLANAR2 {
        2
    } else {
        match cf {
            10 => 0,
            11 => 1,
            12 => 2,
            13 => 3,
            _ => 4,
        }
    }
}

fn get_chroma_sft_w(cfi: i32) -> i32 {
    if cfi == 0 || cfi == 1 || cfi == 2 { 1 } else { 0 }
}

fn get_chroma_sft_h(cfi: i32) -> i32 {
    if cfi == 0 || cfi == 1 { 1 } else { 0 }
}

fn get_num_comp(cfi: i32) -> i32 {
    if cfi == 0 {
        1
    } else if cfi == 4 {
        4
    } else {
        3
    }
}

fn oapv_align_value(val: i32, align: i32) -> i32 {
    (val + align - 1) / align * align
}

fn oapv_validate_tile_topology(profile_idc: i32, tile_cols: i32, tile_rows: i32, num_tiles: *mut i32) -> i32 {
    if tile_cols < 1 || tile_rows < 1 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    let unconst = is_unconst_profile(profile_idc);
    if !unconst && (tile_cols > 20 || tile_rows > 20) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    let n = tile_cols as i64 * tile_rows as i64;
    let limit = if unconst { i32::MAX as i64 } else { 20 * 20 as i64 };
    if n > limit {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    if !num_tiles.is_null() {
        unsafe { *num_tiles = n as i32 };
    }
    ffi::OAPV_OK as i32
}

fn is_unconst_profile(idc: i32) -> bool {
    idc == ffi::OAPV_PROFILE_422_10_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_422_12_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_444_10_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_444_12_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_4444_10_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_4444_12_UNCONST as i32
        || idc == ffi::OAPV_PROFILE_400_10_UNCONST as i32
}

unsafe fn get_blk_to_pic_16(bd: i32) -> ffi::oapv_fn_blk_to_pic_t {
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_16_avx);
    }
    Some(ffi::oapv_blk_to_pic_16)
}

unsafe fn get_blk_to_pic_p21x_y(bd: i32) -> ffi::oapv_fn_blk_to_pic_t {
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_p21x_y_avx);
    }
    Some(ffi::oapv_blk_to_pic_p21x_y)
}

unsafe fn get_blk_to_pic_p21x_uv(bd: i32) -> ffi::oapv_fn_blk_to_pic_t {
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_p21x_uv_avx);
    }
    Some(ffi::oapv_blk_to_pic_p21x_uv)
}

unsafe fn ops_malloc(ctx: *mut ffi::oapvd_ctx_t, size: u32) -> *mut core::ffi::c_void {
    match (*ctx).ops_mem.malloc {
        Some(f) => f((*ctx).ops_mem.udata, size),
        None => core::ptr::null_mut(),
    }
}

unsafe fn ops_free(ctx: *mut ffi::oapvd_ctx_t, ptr: *mut core::ffi::c_void) {
    if let Some(f) = (*ctx).ops_mem.free {
        f((*ctx).ops_mem.udata, ptr);
    }
}

#[no_mangle]
pub extern "C" fn oapvd_set_tile_info(
    tile: *mut ffi::oapvd_tile_t,
    w_pel: c_int,
    h_pel: c_int,
    tile_w: c_int,
    tile_h: c_int,
    num_tile_cols: c_int,
    num_tiles: c_int,
) -> c_int {
    ffi_try!({
        for i in 0..num_tiles as usize {
            let t = tile.add(i);
            let tx = ((i % num_tile_cols as usize) as c_int) * tile_w;
            let ty = ((i / num_tile_cols as usize) as c_int) * tile_h;
            (*t).x = tx;
            (*t).y = ty;
            (*t).w = if tx + tile_w > w_pel { w_pel - tx } else { tile_w };
            (*t).h = if ty + tile_h > h_pel { h_pel - ty } else { tile_h };
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_frm_setup(ctx: *mut ffi::oapvd_ctx_t, cs: c_int) -> c_int {
    ffi_try!({
        if color_format_to_chroma_format_idc(cs_get_format(cs)) != (*ctx).fh.fi.chroma_format_idc {
            return ffi::OAPV_ERR_UNSUPPORTED_COLORSPACE;
        }

        (*ctx).cs = cs;
        (*ctx).bit_depth = (*ctx).fh.fi.bit_depth;
        (*ctx).cfi = (*ctx).fh.fi.chroma_format_idc;
        (*ctx).num_c = get_num_comp((*ctx).cfi);
        (*ctx).c_sft[Y_C][0] = 0;
        (*ctx).c_sft[Y_C][1] = 0;

        let cfi = color_format_to_chroma_format_idc(cs_get_format(cs));
        for c in 1..(*ctx).num_c as usize {
            (*ctx).c_sft[c][0] = get_chroma_sft_w(cfi);
            (*ctx).c_sft[c][1] = get_chroma_sft_h(cfi);
        }

        (*ctx).w = oapv_align_value((*ctx).fh.fi.frame_width as i32, ffi::OAPV_MB_W as i32);
        (*ctx).h = oapv_align_value((*ctx).fh.fi.frame_height as i32, ffi::OAPV_MB_H as i32);

        let is_16c12 = (*ctx).fh.fi.profile_idc == ffi::OAPV_PROFILE_444_16C12 as i32
            || (*ctx).fh.fi.profile_idc == ffi::OAPV_PROFILE_4444_16C12 as i32;
        if is_16c12 {
            (*ctx).disable_companding = (*ctx).force_disable_companding;
        }

        if cs_get_format(cs) == OAPV_CF_PLANAR2 {
            (*ctx).fn_blk_to_pic[Y_C] = get_blk_to_pic_p21x_y((*ctx).bit_depth);
            (*ctx).fn_blk_to_pic[U_C] = get_blk_to_pic_p21x_uv((*ctx).bit_depth);
            (*ctx).fn_blk_to_pic[V_C] = get_blk_to_pic_p21x_uv((*ctx).bit_depth);
        } else if is_16c12 {
            if (*ctx).disable_companding != 0 {
                let to_pic_16 = get_blk_to_pic_16((*ctx).bit_depth);
                for i in 0..(*ctx).num_c as usize {
                    (*ctx).fn_blk_to_pic[i] = to_pic_16;
                }
            } else {
                for i in 0..(*ctx).num_c as usize {
                    (*ctx).fn_blk_to_pic[i] = Some(ffi::oapv_blk_to_pic_12E16);
                }
            }
        } else {
            let to_pic_16 = get_blk_to_pic_16((*ctx).bit_depth);
            for i in 0..(*ctx).num_c as usize {
                (*ctx).fn_blk_to_pic[i] = to_pic_16;
            }
        }

        let tile_w = (*ctx).fh.tile_width_in_mbs * ffi::OAPV_MB_W as i32;
        let tile_h = (*ctx).fh.tile_height_in_mbs * ffi::OAPV_MB_H as i32;

        (*ctx).num_tile_cols = ((*ctx).w + (tile_w - 1)) / tile_w;
        (*ctx).num_tile_rows = ((*ctx).h + (tile_h - 1)) / tile_h;

        let ret = oapv_validate_tile_topology(
            (*ctx).fh.fi.profile_idc,
            (*ctx).num_tile_cols,
            (*ctx).num_tile_rows,
            &mut (*ctx).num_tiles,
        );
        if ret != 0 {
            return ret;
        }

        if (*ctx).num_tiles > (*ctx).tile_cap {
            let tile_bytes = core::mem::size_of::<ffi::oapvd_tile_t>() as i64 * (*ctx).num_tiles as i64;
            if tile_bytes > u32::MAX as i64 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
            }

            ops_free(ctx, (*ctx).tile as *mut core::ffi::c_void);
            (*ctx).tile = core::ptr::null_mut();
            (*ctx).tile_cap = 0;

            let t = ops_malloc(ctx, tile_bytes as u32);
            if t.is_null() {
                return ffi::OAPV_ERR_OUT_OF_MEMORY;
            }
            (*ctx).tile = t as *mut ffi::oapvd_tile_t;
            core::ptr::write_bytes((*ctx).tile as *mut u8, 0, tile_bytes as usize);
            (*ctx).tile_cap = (*ctx).num_tiles;
        }

        oapvd_set_tile_info((*ctx).tile, (*ctx).w, (*ctx).h, tile_w, tile_h, (*ctx).num_tile_cols, (*ctx).num_tiles);

        for i in 0..(*ctx).num_tiles as usize {
            let t = (*ctx).tile.add(i);
            (*t).bs_beg = core::ptr::null_mut();
            (*t).dst = core::ptr::null_mut();
        }
        (*(*ctx).tile).bs_beg = ffi::oapv_bsr_sink(core::ptr::addr_of_mut!((*ctx).bs)) as *mut u8;
        (*ctx).tile_idx = 0;

        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_frm_prepare(ctx: *mut ffi::oapvd_ctx_t, imgb: *mut ffi::oapv_imgb_t) -> c_int {
    ffi_try!({
        if imgb.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        if (*imgb).w[0] as u32 != (*ctx).fh.fi.frame_width || (*imgb).h[0] as u32 != (*ctx).fh.fi.frame_height {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        let ret = oapvd_frm_setup(ctx, (*imgb).cs);
        if ret != 0 {
            return ret;
        }

        let byte_depth = ((*ctx).fh.fi.bit_depth + 7) / 8;
        let planar2 = cs_get_format((*ctx).cs) == OAPV_CF_PLANAR2;
        let num_pln = if planar2 { 2 } else { (*ctx).num_c };

        if (*imgb).np < num_pln {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        for p in 0..num_pln as usize {
            let c = if planar2 && p > 0 { U_C } else { p };
            let w = (*ctx).w >> (*ctx).c_sft[c][0];
            let h = (*ctx).h >> (*ctx).c_sft[c][1];
            let row = w as i64 * byte_depth as i64 * if planar2 && p > 0 { 2 } else { 1 };

            let s_p = (*imgb).s[p] as i64;
            if s_p < row || ((*imgb).bsize[p] as i64) < s_p * (h as i64 - 1) + row {
                return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
        }

        (*ctx).imgb = imgb;
        if let Some(addref) = (*imgb).addref {
            addref(imgb);
        }

        for i in 0..(*ctx).num_tiles as usize {
            (*(*ctx).tile.add(i)).stat = DEC_TILE_STAT_FLAG_DO;
        }

        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}
