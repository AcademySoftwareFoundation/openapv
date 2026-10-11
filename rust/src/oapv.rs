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

use core::ffi::c_int;
use core::ffi::c_void;
use crate::ffi;

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
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_16_avx);
    }
    #[cfg(target_arch = "aarch64")]
    if bd < 16 {
        return Some(ffi::oapv_blk_to_pic_16_neon);
    }
    Some(ffi::oapv_blk_to_pic_16)
}

unsafe fn get_blk_to_pic_p21x_y(bd: i32) -> ffi::oapv_fn_blk_to_pic_t {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_p21x_y_avx);
    }
    #[cfg(target_arch = "aarch64")]
    if bd < 16 {
        return Some(ffi::oapv_blk_to_pic_p21x_y_neon);
    }
    Some(ffi::oapv_blk_to_pic_p21x_y)
}

unsafe fn get_blk_to_pic_p21x_uv(bd: i32) -> ffi::oapv_fn_blk_to_pic_t {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    if bd < 16 && ffi::oapv_check_cpu_info_x86() >> 2 & 1 != 0 {
        return Some(ffi::oapv_blk_to_pic_p21x_uv_avx);
    }
    #[cfg(target_arch = "aarch64")]
    if bd < 16 {
        return Some(ffi::oapv_blk_to_pic_p21x_uv_neon);
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

// ---------------------------------------------------------------------------
// frame-info conversion helpers and stateless info() entry points
// ---------------------------------------------------------------------------

fn chroma_format_idc_to_color_format(cfi: i32) -> i32 {
    match cfi {
        0 => ffi::OAPV_CF_YCBCR400 as i32,
        1 => ffi::OAPV_CF_YCBCR420 as i32,
        2 => ffi::OAPV_CF_YCBCR422 as i32,
        3 => ffi::OAPV_CF_YCBCR444 as i32,
        _ => ffi::OAPV_CF_YCBCR4444 as i32,
    }
}

fn cs_set(f: i32, bit: i32, e: i32) -> i32 {
    (e << 14) | (bit << 8) | f
}

fn div_round_up(n: i32, d: i32) -> i32 {
    (n + d - 1) / d
}

fn pbu_type_is_frame(pbu_type: i32) -> bool {
    pbu_type == 1 || pbu_type == 2 || (pbu_type >= 25 && pbu_type <= 27)
}

#[no_mangle]
pub extern "C" fn oapvd_fi_to_finfo(fi: *mut ffi::oapv_fi_t, pbu_type: c_int, group_id: c_int, finfo: *mut ffi::oapv_frm_info_t) {
    ffi_try!({
        let fi = &*fi;
        let finfo = &mut *finfo;
        finfo.w = fi.frame_width as c_int;
        finfo.h = fi.frame_height as c_int;
        if fi.profile_idc == ffi::OAPV_PROFILE_444_16C12 as i32 || fi.profile_idc == ffi::OAPV_PROFILE_4444_16C12 as i32 {
            finfo.cs = cs_set(chroma_format_idc_to_color_format(fi.chroma_format_idc), 16, 0);
        } else {
            finfo.cs = cs_set(chroma_format_idc_to_color_format(fi.chroma_format_idc), fi.bit_depth, 0);
        }
        finfo.pbu_type = pbu_type;
        finfo.group_id = group_id;
        finfo.profile_idc = fi.profile_idc;
        finfo.level_idc = fi.level_idc;
        finfo.band_idc = fi.band_idc;
        finfo.chroma_format_idc = fi.chroma_format_idc;
        finfo.bit_depth = fi.bit_depth;
        finfo.capture_time_distance = fi.capture_time_distance;
        finfo.use_companding = fi.use_companding;
        // not derivable from frame_info() syntax
        finfo.tile_width_in_mbs = 0;
        finfo.tile_height_in_mbs = 0;
        finfo.tile_cols = 0;
        finfo.tile_rows = 0;
        finfo.num_tiles = 0;

        // frame_info() does not carry the fields below, so set the defaults that
        // apply when they are not signalled in the frame header
        finfo.color_description_present_flag = 0;
        finfo.color_primaries = 2; // unspecified
        finfo.transfer_characteristics = 2; // unspecified
        finfo.matrix_coefficients = 2; // unspecified
        finfo.full_range_flag = 0; // limited range
        finfo.use_q_matrix = 0;
        core::ptr::write_bytes(core::ptr::addr_of_mut!(finfo.q_matrix) as *mut u8, 16, core::mem::size_of_val(&finfo.q_matrix));
    }, ())
}

#[no_mangle]
pub extern "C" fn oapvd_fh_to_finfo(fh: *mut ffi::oapv_fh_t, pbu_type: c_int, group_id: c_int, finfo: *mut ffi::oapv_frm_info_t) {
    ffi_try!({
        let fh = &mut *fh;
        oapvd_fi_to_finfo(core::ptr::addr_of_mut!(fh.fi), pbu_type, group_id, finfo);
        let finfo = &mut *finfo;
        let pic_w_mb = ((fh.fi.frame_width as i32 + (16 - 1)) >> 4) as i32;
        let pic_h_mb = ((fh.fi.frame_height as i32 + (16 - 1)) >> 4) as i32;
        finfo.tile_width_in_mbs = fh.tile_width_in_mbs;
        finfo.tile_height_in_mbs = fh.tile_height_in_mbs;
        finfo.tile_cols = div_round_up(pic_w_mb, fh.tile_width_in_mbs);
        finfo.tile_rows = div_round_up(pic_h_mb, fh.tile_height_in_mbs);
        finfo.num_tiles = finfo.tile_cols * finfo.tile_rows;
        finfo.use_q_matrix = fh.use_q_matrix;
        let modv = (1 << 3) - 1;
        for c in 0..4usize {
            for i in 0..64usize {
                finfo.q_matrix[c][i] = fh.q_matrix[c][i >> 3][i & modv];
            }
        }
        finfo.color_description_present_flag = fh.color_description_present_flag;
        finfo.color_primaries = fh.color_primaries as u8;
        finfo.transfer_characteristics = fh.transfer_characteristics as u8;
        finfo.matrix_coefficients = fh.matrix_coefficients as u8;
        finfo.full_range_flag = fh.full_range_flag;
    }, ())
}

#[no_mangle]
pub extern "C" fn oapvd_info_pbu(pbu: *mut c_void, pbu_size: c_int, pbu_info: *mut ffi::oapv_pbu_info_t) -> c_int {
    ffi_try!({
        if pbu.is_null() || pbu_info.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if pbu_size < 4 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        ffi::oapv_bsr_init(&mut bs, pbu as *mut u8, pbu_size as u32, None);

        (*pbu_info).pbu_type = ffi::oapv_bsr_read(&mut bs, 8) as c_int;
        (*pbu_info).group_id = ffi::oapv_bsr_read(&mut bs, 16) as c_int;

        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvd_info_frame(pbu: *mut c_void, pbu_size: c_int, frm_info: *mut ffi::oapv_frm_info_t) -> c_int {
    ffi_try!({
        if pbu.is_null() || frm_info.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if pbu_size < (ffi::OAPV_PBU_HEADER_BYTE as c_int + ffi::OAPV_FRAME_INFO_BYTE as c_int) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        let mut pbuh: ffi::oapv_pbuh_t = core::mem::zeroed();
        let mut fh: ffi::oapv_fh_t = core::mem::zeroed();
        ffi::oapv_bsr_init(&mut bs, pbu as *mut u8, pbu_size as u32, None);

        let ret = ffi::oapvd_vlc_pbu_header(&mut bs, &mut pbuh);
        if ret != 0 {
            return ret;
        }
        if !pbu_type_is_frame(pbuh.pbu_type) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let ret = ffi::oapvd_vlc_frame_header(&mut bs, &mut fh, core::ptr::null_mut(), 0);
        if ret != 0 {
            return ret;
        }
        oapvd_fh_to_finfo(&mut fh, pbuh.pbu_type, pbuh.group_id, frm_info);
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvd_info_tile(pbu: *mut c_void, pbu_size: c_int, pos_tiles: *mut ffi::oapv_tile_pos_t, num_tiles: *mut c_int) -> c_int {
    ffi_try!({
        if pbu.is_null() || num_tiles.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if !pos_tiles.is_null() && *num_tiles < 0 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if pbu_size < (ffi::OAPV_PBU_HEADER_BYTE as c_int + ffi::OAPV_FRAME_INFO_BYTE as c_int) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        let mut pbuh: ffi::oapv_pbuh_t = core::mem::zeroed();
        let mut fh: ffi::oapv_fh_t = core::mem::zeroed();
        ffi::oapv_bsr_init(&mut bs, pbu as *mut u8, pbu_size as u32, None);

        let ret = ffi::oapvd_vlc_pbu_header(&mut bs, &mut pbuh);
        if ret != 0 {
            return ret;
        }
        if !pbu_type_is_frame(pbuh.pbu_type) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        // the tile sizes the frame header may carry are reported into the
        // caller's array, which must be cleared first since they are optional
        if !pos_tiles.is_null() {
            for i in 0..*num_tiles as usize {
                let e = pos_tiles.add(i);
                (*e).offset = 0;
                (*e).size = 0;
            }
        }
        let cap = if !pos_tiles.is_null() { *num_tiles } else { 0 };
        let ret = ffi::oapvd_vlc_frame_header(&mut bs, &mut fh, pos_tiles, cap);
        if ret != 0 {
            return ret;
        }

        let pic_w_mb = ((fh.fi.frame_width as i32 + (16 - 1)) >> 4) as i32;
        let pic_h_mb = ((fh.fi.frame_height as i32 + (16 - 1)) >> 4) as i32;

        let tile_cols = div_round_up(pic_w_mb, fh.tile_width_in_mbs);
        let tile_rows = div_round_up(pic_h_mb, fh.tile_height_in_mbs);

        let mut n: c_int = 0;
        let ret = oapv_validate_tile_topology(fh.fi.profile_idc, tile_cols, tile_rows, &mut n);
        if ret != 0 {
            return ret;
        }

        if pos_tiles.is_null() {
            // query the number of tiles only
            *num_tiles = n;
            return ffi::OAPV_OK as c_int;
        }
        if *num_tiles < n {
            // not enough capacity
            *num_tiles = n;
            return ffi::OAPV_ERR_REACHED_MAX as c_int;
        }

        let mut tpos = pos_tiles;
        for i in 0..tile_rows {
            for j in 0..tile_cols {
                (*tpos).x_mb = fh.tile_width_in_mbs * j;
                (*tpos).y_mb = fh.tile_height_in_mbs * i;

                if (*tpos).x_mb + fh.tile_width_in_mbs > pic_w_mb {
                    (*tpos).w_mb = pic_w_mb - (*tpos).x_mb;
                } else {
                    (*tpos).w_mb = fh.tile_width_in_mbs;
                }
                if (*tpos).y_mb + fh.tile_height_in_mbs > pic_h_mb {
                    (*tpos).h_mb = pic_h_mb - (*tpos).y_mb;
                } else {
                    (*tpos).h_mb = fh.tile_height_in_mbs;
                }
                (*tpos).idx = i * tile_cols + j;
                tpos = tpos.add(1);
            }
        }
        if fh.tile_size_present_in_fh_flag != 0 {
            // tile data follows the frame header; each tile unit is a 4-byte size
            // field plus the tile data of the size reported above
            let mut off: i64 = bs.cur.offset_from(bs.beg) as i64 - ((bs.leftbits >> 3) as i64);
            for i in 0..n as usize {
                off += ffi::OAPV_TILE_SIZE_LEN as i64 + (*pos_tiles.add(i)).size as i64;
                // the tile must lie within the PBU
                if off > pbu_size as i64 {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                }
                (*pos_tiles.add(i)).offset = (off - ffi::OAPV_TILE_SIZE_LEN as i64 - (*pos_tiles.add(i)).size as i64) as c_int;
            }
        }
        *num_tiles = n;

        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvd_info(au: *mut c_void, au_size: c_int, aui: *mut ffi::oapv_au_info_t) -> c_int {
    ffi_try!({
        if au.is_null() || aui.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        // read signature ('aPv1')
        if au_size <= 4 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let mut signature: u32 = 0;
        let ret = ffi::oapv_bsr_read_direct(au, 32, &mut signature);
        if ret != 0 {
            return ret;
        }
        if signature != 0x61507631 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let mut cur_read_size: u32 = 4;
        let mut frm_count: c_int = 0;

        // parse PBUs
        loop {
            let mut pbu_size: u32 = 0;
            let remain = (au_size as u32).wrapping_sub(cur_read_size);
            if remain < 8 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
            ffi::oapv_bsr_init(&mut bs, (au as *mut u8).add(cur_read_size as usize), remain, None);

            let ret = ffi::oapvd_vlc_pbu_size(&mut bs, &mut pbu_size);
            if ret != 0 {
                return ret;
            }
            let remain = remain - 4; // size of pbu_size syntax
            if !(pbu_size >= 4 && pbu_size <= remain) {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }

            let mut pbuh: ffi::oapv_pbuh_t = core::mem::zeroed();
            let ret = ffi::oapvd_vlc_pbu_header(&mut bs, &mut pbuh);
            if ret != 0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            if pbuh.pbu_type == ffi::OAPV_PBU_TYPE_AU_INFO as i32 {
                // parse access_unit_info in PBU
                let mut ai: ffi::oapv_aui_t = core::mem::zeroed();
                let ret = ffi::oapvd_vlc_au_info(&mut bs, &mut ai);
                if ret != 0 {
                    return ret;
                }

                (*aui).num_frms = ai.num_frames;
                for i in 0..ai.num_frames as usize {
                    oapvd_fi_to_finfo(
                        core::ptr::addr_of_mut!(ai.frame_info[i]),
                        ai.pbu_type[i],
                        ai.group_id[i],
                        core::ptr::addr_of_mut!((*aui).frm_info[i]),
                    );
                }
                return ffi::OAPV_OK as c_int; // found access_unit_info, no need to read more PBUs
            }
            if pbu_type_is_frame(pbuh.pbu_type) {
                // parse frame_header in PBU to report the tile information, the
                // color description, and the quantization matrix as well as
                // frame_info
                if frm_count >= ffi::OAPV_MAX_NUM_FRAMES as i32 {
                    return ffi::OAPV_ERR_REACHED_MAX as c_int;
                }
                let mut fh: ffi::oapv_fh_t = core::mem::zeroed();
                let ret = ffi::oapvd_vlc_frame_header(&mut bs, &mut fh, core::ptr::null_mut(), 0);
                if ret != 0 {
                    return ret;
                }
                oapvd_fh_to_finfo(&mut fh, pbuh.pbu_type, pbuh.group_id, core::ptr::addr_of_mut!((*aui).frm_info[frm_count as usize]));
                frm_count += 1;
            }
            (*aui).num_frms = frm_count;
            cur_read_size = cur_read_size.wrapping_add(pbu_size + 4); // 4 bytes for pbu_size syntax itself
            if cur_read_size >= au_size as u32 {
                break;
            }
        }
        ffi::OAPV_OK as c_int
    }, -1)
}
