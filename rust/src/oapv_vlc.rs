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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Rust port of oapv_vlc.c; generates the same symbols and is a bit-exact,
// drop-in replacement when CMake selects the Rust VLC implementation.
// Crate manifest and bindgen config live in rust/.

use crate::ffi;

pub(crate) use dec::{dec_vlc_au_info, dec_vlc_frame_header, dec_vlc_pbu_header, dec_vlc_pbu_size};

mod bits {
use super::ffi::oapv_bs_t;

pub unsafe fn bsw_is_align8(bs: *const oapv_bs_t) -> bool {
    (*bs).leftbits & 0x7 == 0
}

#[inline(always)]
unsafe fn bsw_flush_8byte(bs: *mut oapv_bs_t) {
    if (*bs).cur.add(8) <= (*bs).end {
        core::ptr::write_unaligned((*bs).cur as *mut [u8; 8], (*bs).code.to_be_bytes());
        (*bs).cur = (*bs).cur.add(8);
    } else {
        (*bs).cur = (*bs).cur.add(8);
    }
    (*bs).code = 0;
    (*bs).leftbits = 64;
}

#[inline(always)]
pub unsafe fn bsw_write_64bits(bs: *mut oapv_bs_t, mut code64: u64, mut nbits: i32) {
    code64 = code64.wrapping_shl((64 - nbits) as u32);
    if nbits < (*bs).leftbits {
        (*bs).code |= code64.wrapping_shr((64 - (*bs).leftbits) as u32);
        (*bs).leftbits -= nbits;
    } else {
        (*bs).code |= code64.wrapping_shr((64 - (*bs).leftbits) as u32);
        code64 = code64.wrapping_shl((*bs).leftbits as u32);
        nbits -= (*bs).leftbits;
        bsw_flush_8byte(bs);
        if nbits > 0 {
            (*bs).code |= code64.wrapping_shr((64 - (*bs).leftbits) as u32);
            (*bs).leftbits -= nbits;
        }
    }
}

pub unsafe fn bsr_is_unexpected_eob(bs: *const oapv_bs_t) -> bool {
    (*bs).is_eob != 0
}

pub unsafe fn bsr_move_byte_align(bs: *mut oapv_bs_t, byte: i64) {
    (*bs).cur = (*bs).cur.offset((byte - ((*bs).leftbits >> 3) as i64) as isize);
    (*bs).code = 0;
    (*bs).leftbits = 0;
}

pub unsafe fn bsr_get_left_byte(bs: *const oapv_bs_t) -> i64 {
    (*bs).end.offset_from((*bs).cur) as i64 + ((*bs).leftbits >> 3) as i64
}

// local bit-reader state over the unread bytes of the bitstream; reads go
// through the slice, so a bad length check cannot read past its end, and a
// &mut BsSt keeps the bit buffer in registers across the coefficient stores
pub struct BsSt<'a> {
    pub code: u64,
    pub leftbits: i32,
    data: &'a [u8],
    pos: usize,
    pub is_eob: i32,
}

impl<'a> BsSt<'a> {
    // the caller guarantees that cur..end of 'bs' is readable for 'a
    #[inline(always)]
    pub unsafe fn load(bs: *mut oapv_bs_t) -> BsSt<'a> {
        let len = (*bs).end.offset_from((*bs).cur);
        let data: &[u8] = if len > 0 { core::slice::from_raw_parts((*bs).cur, len as usize) } else { &[] };
        BsSt { code: (*bs).code, leftbits: (*bs).leftbits, data, pos: 0, is_eob: (*bs).is_eob }
    }

    #[inline(always)]
    pub unsafe fn store(&self, bs: *mut oapv_bs_t) {
        (*bs).code = self.code;
        (*bs).leftbits = self.leftbits;
        (*bs).cur = (*bs).cur.add(self.pos);
        (*bs).is_eob = self.is_eob;
    }

    #[inline(always)]
    pub fn flush_1byte(&mut self) {
        if let Some(&b) = self.data.get(self.pos) {
            self.code = (b as u64) << 56;
            self.pos += 1;
            self.leftbits = 8;
        } else {
            self.code = u64::MAX;
            self.leftbits = 8;
            self.is_eob = 1;
        }
    }

    #[inline(always)]
    pub fn read_1bit(&mut self) -> i32 {
        let bit = (self.code >> 63) as i32;
        self.code <<= 1;
        self.leftbits -= 1;
        bit
    }

    #[inline(always)]
    pub fn topup(&mut self) {
        let n_ = (63 - self.leftbits) >> 3;
        if let Some(w) = self.data[self.pos..].first_chunk::<8>() {
            let mut v = u64::from_be_bytes(*w);
            v &= !u64::MAX.wrapping_shr((n_ << 3) as u32);
            self.code |= v.wrapping_shr(self.leftbits as u32);
            self.pos += n_ as usize;
            self.leftbits += n_ << 3;
        } else {
            let mut n_ = n_;
            while n_ > 0 {
                let Some(&b) = self.data.get(self.pos) else { break };
                self.code |= (b as u64).wrapping_shl((56 - self.leftbits) as u32);
                self.pos += 1;
                self.leftbits += 8;
                n_ -= 1;
            }
        }
    }

    #[inline(always)]
    pub fn read_zero_prefix(&mut self, k: &mut i32) {
        loop {
            if self.leftbits == 0 {
                self.flush_1byte();
            }
            let z_ = (self.code | 1u64 << (63 - self.leftbits)).leading_zeros() as i32;
            if z_ < self.leftbits {
                *k += z_;
                self.code = (self.code << z_) << 1;
                self.leftbits -= z_ + 1;
                break;
            }
            *k += self.leftbits;
            self.leftbits = 0;
        }
    }

    #[inline(always)]
    pub fn dec_vlc_read(&mut self, mut k: i32) -> i32 {
        let symbol: u32;
        let mut parse_exp_golomb = false;

        if self.leftbits < 32 {
            self.topup();
        }
        if self.leftbits == 0 {
            self.flush_1byte();
        }
        let mut flag = self.read_1bit();

        if flag == 0 {
            if self.leftbits == 0 {
                self.flush_1byte();
            }
            flag = self.read_1bit();
            symbol = 1u32.wrapping_shl(k as u32);
            parse_exp_golomb = flag != 0;
        } else {
            symbol = 0;
        }
        let mut symbol = symbol;
        if parse_exp_golomb {
            self.read_zero_prefix(&mut k);
            if k >= 30 {
                return -1;
            }
            symbol = symbol.wrapping_add(1u32.wrapping_shl(k as u32));
        }

        if k > 0 {
            while self.leftbits < k {
                symbol = symbol.wrapping_add(self.code.wrapping_shr((64 - k) as u32) as u32);
                k -= self.leftbits;
                self.flush_1byte();
            }
            symbol = symbol.wrapping_add(self.code.wrapping_shr((64 - k) as u32) as u32);
            self.code <<= k;
            self.leftbits -= k;
        }
        symbol as i32
    }
}

}

mod enc {
use super::bits::*;
use super::ffi;
use crate::oapv_util::*;
use core::ffi::c_int;

const ENC_PREFIX_VLC: [[u16; 2]; 3] = [[1, 0xFF], [0, 0], [0, 1]];

unsafe fn add_bits_to_code(val: u64, nb: i32, code: u64) -> u64 {
    code << nb | val
}

#[inline(always)]
unsafe fn enc_vlc_write_to_code(_bs: *mut ffi::oapv_bs_t, val: i32, mut k: i32, nbits: *mut i32) -> u64 {
    let mut code64: u64 = 0;
    let mut symbol = val as u32;
    let mut nb: i32 = 0;
    let vlc_idx = oapv_min(val >> k, 2) as usize;

    while symbol >= 1u32.wrapping_shl(k as u32) {
        symbol = symbol.wrapping_sub(1u32.wrapping_shl(k as u32));
        if nb < 2 {
            code64 = add_bits_to_code(ENC_PREFIX_VLC[vlc_idx][nb as usize] as u64, 1, code64);
        } else {
            code64 = add_bits_to_code(0, 1, code64);
            k += 1;
        }
        nb += 1;
    }
    if nb < 2 {
        code64 = add_bits_to_code(ENC_PREFIX_VLC[vlc_idx][nb as usize] as u64, 1, code64);
    } else {
        code64 = add_bits_to_code(1, 1, code64);
    }
    nb += 1;
    if k > 0 {
        code64 = add_bits_to_code(symbol as u64, k, code64);
        nb += k;
    }
    *nbits = nb;
    code64
}

unsafe fn enc_vlc_quantization_matrix(bs: *mut ffi::oapv_bs_t, ctx: *mut ffi::oapve_ctx_t, fh: *mut ffi::oapv_fh_t) -> i32 {
    let num_c = (*ctx).num_c as usize;
    for cidx in 0..num_c {
        for y in 0..8 {
            for x in 0..8 {
                crate::oapv_bs::bsw_write(bs, (*fh).q_matrix[cidx][y][x] as u32, 8);
            }
        }
    }
    0
}

unsafe fn enc_vlc_tile_info(bs: *mut ffi::oapv_bs_t, ctx: *mut ffi::oapve_ctx_t, fh: *mut ffi::oapv_fh_t) -> i32 {
    crate::oapv_bs::bsw_write(bs, (*fh).tile_width_in_mbs as u32, 20);
    crate::oapv_bs::bsw_write(bs, (*fh).tile_height_in_mbs as u32, 20);
    crate::oapv_bs::bsw_write(bs, (*fh).tile_size_present_in_fh_flag as u32, 1);
    if (*fh).tile_size_present_in_fh_flag != 0 {
        for i in 0..(*ctx).num_tiles as usize {
            crate::oapv_bs::bsw_write(bs, (*(*ctx).tile.add(i)).tile_size as u32, 32);
        }
    }
    0
}

#[no_mangle]
pub extern "C" fn oapve_vlc_dc_coef(bs: *mut ffi::oapv_bs_t, dc_diff: c_int, kparam_dc: *mut c_int) -> c_int {
    ffi_try!({
        let mut nbits: i32 = 0;
        let abs_dc_diff = oapv_abs32(dc_diff);

        let mut code64 = enc_vlc_write_to_code(bs, abs_dc_diff, *kparam_dc, &mut nbits);

        if abs_dc_diff != 0 {
            let sign_dc_diff = oapv_get_sign32(dc_diff);
            code64 = add_bits_to_code(sign_dc_diff as u64, 1, code64);
            *kparam_dc = kparam_dc_of(abs_dc_diff);
            nbits += 1;
        } else {
            *kparam_dc = OAPV_KPARAM_DC_MIN;
        }
        bsw_write_64bits(bs, code64, nbits);

        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}


#[no_mangle]
pub extern "C" fn oapve_vlc_ac_coef(bs: *mut ffi::oapv_bs_t, coef: *mut i16, kparam_ac: *mut c_int) {
    ffi_try!({
        let mut first_ac = 1;
        let mut run: i32 = 0;
        let scanp = ffi::oapv_tbl_scan.as_ptr();
        let mut k_run = OAPV_KPARAM_RUN_MIN;
        let mut k_ac = *kparam_ac;
        let mut code64: u64;
        let mut nbits: i32;

        let tbl = &*core::ptr::addr_of!(ffi::oapve_tbl_vlc_code);
        for scan_pos in 1..64i32 {
            let c = *coef.add(*scanp.add(scan_pos as usize) as usize) as i32;
            let level: i32;
            if c != 0 {
                let e = tbl.get_unchecked(run as usize).get_unchecked(k_run as usize);
                code64 = e[0] as u64;
                nbits = e[1] as i32;
                k_run = kparam_run_of(run);
                run = 0;

                bsw_write_64bits(bs, code64, nbits);

                level = oapv_abs16(c);
                if level < 101 {
                    let e = tbl.get_unchecked((level - 1) as usize).get_unchecked(k_ac as usize);
                    code64 = e[0] as u64;
                    nbits = e[1] as i32;
                } else {
                    code64 = enc_vlc_write_to_code(bs, level - 1, k_ac, &mut nbits);
                }
                k_ac = kparam_ac_of(level);
                if first_ac != 0 {
                    first_ac = 0;
                    *kparam_ac = k_ac;
                }
                let sign = oapv_get_sign16(c);
                code64 = add_bits_to_code(sign as u64, 1, code64);
                nbits += 1;

                bsw_write_64bits(bs, code64, nbits);
            } else {
                run += 1;
            }
        }
        let code64: u64;
        let nbits: i32;
        if run > 0 {
            code64 = ffi::oapve_tbl_vlc_code[run as usize][k_run as usize][0] as u64;
            nbits = ffi::oapve_tbl_vlc_code[run as usize][k_run as usize][1] as i32;
            bsw_write_64bits(bs, code64, nbits);
        }
    }, ())
}

#[no_mangle]
pub extern "C" fn oapve_set_frame_header(ctx: *mut ffi::oapve_ctx_t, fh: *mut ffi::oapv_fh_t) {
    ffi_try!({
        let param = (*ctx).param;

        core::ptr::write_bytes(fh as *mut u8, 0, core::mem::size_of::<ffi::oapv_fh_t>());
        (*fh).fi.profile_idc = (*param).profile_idc;
        (*fh).fi.level_idc = (*param).level_idc;
        (*fh).fi.band_idc = (*param).band_idc;
        (*fh).fi.frame_width = (*param).w as u32;
        (*fh).fi.frame_height = (*param).h as u32;
        (*fh).fi.chroma_format_idc = (*ctx).cfi;
        (*fh).fi.bit_depth = (*ctx).bit_depth;
        (*fh).fi.use_companding = (*ctx).use_companding;

        (*fh).tile_width_in_mbs = (*param).tile_w / ffi::OAPV_MB_W as i32;
        (*fh).tile_height_in_mbs = (*param).tile_h / ffi::OAPV_MB_H as i32;
        (*fh).color_description_present_flag = (*param).color_description_present_flag;
        (*fh).color_primaries = (*param).color_primaries as i32;
        (*fh).transfer_characteristics = (*param).transfer_characteristics as i32;
        (*fh).matrix_coefficients = (*param).matrix_coefficients as i32;
        (*fh).full_range_flag = (*param).full_range_flag;

        (*fh).use_q_matrix = (*param).use_q_matrix;
        if (*fh).use_q_matrix == 0 {
            for cidx in 0..(*ctx).num_c as usize {
                for y in 0..8 {
                    for x in 0..8 {
                        (*fh).q_matrix[cidx][y][x] = 16;
                    }
                }
            }
        } else {
            let modv = (1 << 3) - 1;
            for c in 0..4usize {
                for i in 0..64 {
                    (*fh).q_matrix[c][i >> 3][i & modv] = (*param).q_matrix[c][i];
                }
            }
        }
        (*fh).tile_size_present_in_fh_flag = if (*ctx).tile_size_in_fh[(*ctx).frm_idx as usize] != 0 { 1 } else { 0 };
    }, ())
}

#[no_mangle]
pub extern "C" fn oapve_set_tile_header(ctx: *mut ffi::oapve_ctx_t, th: *mut ffi::oapv_th_t, tile_idx: c_int, qp: c_int) {
    ffi_try!({
        core::ptr::write_bytes(th as *mut u8, 0, core::mem::size_of::<ffi::oapv_th_t>());

        for c in 0..(*ctx).num_c as usize {
            (*th).tile_qp[c] = oapv_clip3(MIN_QUANT, max_quant((*ctx).bit_depth), qp + (*ctx).qp_offset[c as usize] as i32);
        }
        (*th).tile_index = tile_idx;

        for i in 0..4usize {
            (*th).tile_data_size[i] = 1;
        }
    }, ())
}

#[no_mangle]
pub extern "C" fn oapve_vlc_frame_info(bs: *mut ffi::oapv_bs_t, fi: *mut ffi::oapv_fi_t) -> c_int {
    ffi_try!({
        crate::oapv_bs::bsw_write(bs, (*fi).profile_idc as u32, 8);
        crate::oapv_bs::bsw_write(bs, (*fi).level_idc as u32, 8);
        crate::oapv_bs::bsw_write(bs, (*fi).band_idc as u32, 3);
        crate::oapv_bs::bsw_write(bs, 0, 5);
        crate::oapv_bs::bsw_write(bs, (*fi).frame_width as u32, 24);
        crate::oapv_bs::bsw_write(bs, (*fi).frame_height as u32, 24);
        crate::oapv_bs::bsw_write(bs, (*fi).chroma_format_idc as u32, 4);
        crate::oapv_bs::bsw_write(bs, ((*fi).bit_depth - 8) as u32, 4);
        crate::oapv_bs::bsw_write(bs, (*fi).capture_time_distance as u32, 8);
        crate::oapv_bs::bsw_write1(bs, (*fi).use_companding);
        crate::oapv_bs::bsw_write(bs, 0, 7);
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_frame_header(bs: *mut ffi::oapv_bs_t, ctx: *mut ffi::oapve_ctx_t, fh: *mut ffi::oapv_fh_t) -> c_int {
    ffi_try!({
        if !bsw_is_align8(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        oapve_vlc_frame_info(bs, &mut (*fh).fi);
        crate::oapv_bs::bsw_write(bs, 0, 8);
        crate::oapv_bs::bsw_write1(bs, (*fh).color_description_present_flag);
        if (*fh).color_description_present_flag != 0 {
            crate::oapv_bs::bsw_write(bs, (*fh).color_primaries as u32, 8);
            crate::oapv_bs::bsw_write(bs, (*fh).transfer_characteristics as u32, 8);
            crate::oapv_bs::bsw_write(bs, (*fh).matrix_coefficients as u32, 8);
            crate::oapv_bs::bsw_write1(bs, (*fh).full_range_flag);
        }
        crate::oapv_bs::bsw_write1(bs, (*fh).use_q_matrix);
        if (*fh).use_q_matrix != 0 {
            enc_vlc_quantization_matrix(bs, ctx, fh);
        }
        enc_vlc_tile_info(bs, ctx, fh);

        crate::oapv_bs::bsw_write(bs, 0, 8);
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_tile_size(bs: *mut ffi::oapv_bs_t, tile_size: c_int) -> c_int {
    ffi_try!({
        if !bsw_is_align8(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        crate::oapv_bs::bsw_write(bs, tile_size as u32, 32);
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_tile_header(ctx: *mut ffi::oapve_ctx_t, bs: *mut ffi::oapv_bs_t, th: *mut ffi::oapv_th_t) -> c_int {
    ffi_try!({
        if !bsw_is_align8(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        (*th).tile_header_size = 5;
        (*th).tile_header_size += (*ctx).num_c * 5;

        crate::oapv_bs::bsw_write(bs, (*th).tile_header_size as u32, 16);
        crate::oapv_bs::bsw_write(bs, (*th).tile_index as u32, 16);
        for c in 0..(*ctx).num_c as usize {
            crate::oapv_bs::bsw_write(bs, (*th).tile_data_size[c] as u32, 32);
        }
        for c in 0..(*ctx).num_c as usize {
            crate::oapv_bs::bsw_write(bs, (*th).tile_qp[c] as u32, 8);
        }
        crate::oapv_bs::bsw_write(bs, (*th).reserved_zero_8bits as u32, 8);

        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_au_info(bs: *mut ffi::oapv_bs_t, ctx: *mut ffi::oapve_ctx_t, frms: *mut ffi::oapv_frms_t, bs_fi_pos: *mut *mut ffi::oapv_bs_t) -> c_int {
    ffi_try!({
        crate::oapv_bs::bsw_write(bs, (*frms).num_frms as u32, 16);
        for fidx in 0..(*frms).num_frms as usize {
            crate::oapv_bs::bsw_write(bs, (*frms).frm[fidx].pbu_type as u32, 8);
            crate::oapv_bs::bsw_write(bs, (*frms).frm[fidx].group_id as u32, 16);
            crate::oapv_bs::bsw_write(bs, 0, 8);
            let dst = *bs_fi_pos.add(core::mem::size_of::<ffi::oapv_bs_t>() * fidx);
            core::ptr::copy_nonoverlapping(bs, dst, 1);
            oapve_vlc_frame_info(bs, &mut (*ctx).fh.fi);
        }

        crate::oapv_bs::bsw_write(bs, 0, 8);
        while !bsw_is_align8(bs) {
            crate::oapv_bs::bsw_write1(bs, 0);
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_pbu_size(bs: *mut ffi::oapv_bs_t, pbu_size: c_int) -> c_int {
    ffi_try!({
        if !bsw_is_align8(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        crate::oapv_bs::bsw_write(bs, pbu_size as u32, 32);
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_pbu_header(bs: *mut ffi::oapv_bs_t, pbu_type: c_int, group_id: c_int) -> c_int {
    ffi_try!({
        crate::oapv_bs::bsw_write(bs, pbu_type as u32, 8);
        crate::oapv_bs::bsw_write(bs, group_id as u32, 16);
        crate::oapv_bs::bsw_write(bs, 0, 8);

        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_metadata(md: *mut ffi::oapv_md_t, bs: *mut ffi::oapv_bs_t) -> c_int {
    ffi_try!({
        let bs_pos_md = crate::oapv_bs::bsw_sink(bs) as *mut u8;
        if bs_pos_md.is_null() {
            return ffi::OAPV_ERR_OUT_OF_BS_BUF as c_int;
        }

        crate::oapv_bs::bsw_write(bs, 0, 32);

        let mut mdp = (*md).md_payload;
        while !mdp.is_null() {
            let mut mdp_pltype = (*mdp).pld_type;
            while mdp_pltype >= 255 {
                crate::oapv_bs::bsw_write(bs, 0xFF, 8);
                mdp_pltype -= 255;
            }
            crate::oapv_bs::bsw_write(bs, mdp_pltype, 8);

            let mut mdp_size = (*mdp).pld_size;
            while mdp_size >= 255 {
                crate::oapv_bs::bsw_write(bs, 0xFF, 8);
                mdp_size -= 255;
            }
            crate::oapv_bs::bsw_write(bs, mdp_size, 8);

            for i in 0..(*mdp).pld_size as isize {
                let payload_data = (*mdp).pld_data as *mut u8;
                crate::oapv_bs::bsw_write(bs, *payload_data.offset(i) as u32, 8);
            }

            mdp = (*mdp).next;
        }
        let bs_pos_end = crate::oapv_bs::bsw_sink(bs) as *mut u8;
        if bs_pos_end.is_null() {
            return ffi::OAPV_ERR_OUT_OF_BS_BUF as c_int;
        }
        let md_size = (bs_pos_end.offset_from(bs_pos_md) as u32) - 4;
        crate::oapv_bs::bsw_write_direct(bs_pos_md as *mut core::ffi::c_void, md_size, 32);

        return ffi::OAPV_OK as c_int;
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline(always)]
unsafe fn get_vlc_rate(mut val: i32, mut k: i32) -> i32 {
    if val < 100 && k < 5 {
        return unsafe {
            let tbl = &*core::ptr::addr_of!(ffi::oapve_tbl_vlc_code);
            tbl.get_unchecked(val as usize).get_unchecked(k as usize)[1] as i32
        };
    }

    let mut code_len = 0;
    code_len += 1;
    if val < (1 << k) {
        code_len += k;
    } else {
        val -= 1 << k;
        code_len += 1;
        if val < (1 << k) {
            code_len += k;
        } else {
            val -= 1 << k;
            while val >= (1 << k) {
                code_len += 1;
                val -= 1 << k;
                k += 1;
            }
            code_len += k + 1;
        }
    }

    code_len
}

#[no_mangle]
pub extern "C" fn oapve_vlc_get_level_cost(coef: c_int, k: c_int, lambda: f64) -> f64 {
    ffi_try!({
        let mut rate: i32 = get_vlc_rate(coef, k);
        if coef != 0 {
            rate += 1;
        }
        return rate as f64 * lambda;
    }, 0.0)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_get_run_cost(run: c_int, k: c_int, lambda: f64) -> f64 {
    ffi_try!({
        let rate: i32 = get_vlc_rate(run, k);
        return rate as f64 * lambda;
    }, 0.0)
}

#[no_mangle]
pub extern "C" fn oapve_vlc_get_coef_rate(core: *mut ffi::oapve_core_t, coef: *mut i16, c: c_int) -> c_int {
    ffi_try!({
        let mut rate = 0;
        let mut prev_run = 0;
        let cu = c as usize;

        let mut level = oapv_abs32(*coef as i32 - (*core).prev_dc[cu]);
        let mut rice_level = (*core).kparam_dc[cu];

        rate += get_vlc_rate(level, rice_level);
        if level != 0 {
            rate += 1;
        }

        let scanp = ffi::oapv_tbl_scan.as_ptr();
        let mut run: u32 = 0;

        rice_level = (*core).kparam_ac[cu];
        for scan_pos in 1..64i32 {
            let coef_cur = *coef.add(*scanp.add(scan_pos as usize) as usize) as i32;
            if coef_cur != 0 {
                level = oapv_abs16(coef_cur);
                let rice_run = oapv_min(prev_run >> 2, 2);
                rice_level = oapv_clip3(OAPV_KPARAM_AC_MIN, OAPV_KPARAM_AC_MAX, rice_level);

                rate += get_vlc_rate(run as i32, rice_run);
                rate += get_vlc_rate(level - 1, rice_level);
                if level != 0 {
                    rate += 1;
                }

                prev_run = run as i32;
                run = 0;
                rice_level = level >> 2;
            } else {
                run += 1;
            }
        }
        if run != 0 {
            let rice_run = oapv_min(prev_run >> 2, 2);
            rate += get_vlc_rate(run as i32, rice_run);
        }

        return rate;
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

}

mod dec {
use super::bits::*;
use super::ffi;
use crate::oapv_util::*;
use core::ffi::c_int;

unsafe fn dec_vlc_q_matrix(bs: *mut ffi::oapv_bs_t, fh: *mut ffi::oapv_fh_t) -> i32 {
    let num_comp = get_num_comp((*fh).fi.chroma_format_idc);
    for cidx in 0..num_comp as usize {
        for y in 0..8 {
            for x in 0..8 {
                let v = crate::oapv_bs::bsr_read(bs, 8);
                (*fh).q_matrix[cidx][y][x] = v as u8;
                if (*fh).q_matrix[cidx][y][x] == 0 {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
                }
            }
        }
    }
    ffi::OAPV_OK as i32
}

unsafe fn dec_vlc_tile_info(bs: *mut ffi::oapv_bs_t, fh: *mut ffi::oapv_fh_t, pos_tiles: *mut ffi::oapv_tile_pos_t, cap: c_int) -> i32 {
    (*fh).tile_width_in_mbs = crate::oapv_bs::bsr_read(bs, 20) as i32;
    if (*fh).tile_width_in_mbs <= 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
    }

    (*fh).tile_height_in_mbs = crate::oapv_bs::bsr_read(bs, 20) as i32;
    if (*fh).tile_height_in_mbs <= 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
    }

    let pic_w = (((*fh).fi.frame_width + (ffi::OAPV_MB_W - 1)) >> ffi::OAPV_LOG2_MB_W) << ffi::OAPV_LOG2_MB_W;
    let pic_h = (((*fh).fi.frame_height + (ffi::OAPV_MB_H - 1)) >> ffi::OAPV_LOG2_MB_H) << ffi::OAPV_LOG2_MB_H;

    let tile_w = (*fh).tile_width_in_mbs as u32 * ffi::OAPV_MB_W;
    let tile_h = (*fh).tile_height_in_mbs as u32 * ffi::OAPV_MB_H;

    let tile_cols = ((pic_w + (tile_w - 1)) / tile_w) as i32;
    let tile_rows = ((pic_h + (tile_h - 1)) / tile_h) as i32;

    let ret = oapv_validate_tile_topology((*fh).fi.profile_idc, tile_cols, tile_rows, None);
    if ret != 0 {
        return ret;
    }

    (*fh).tile_size_present_in_fh_flag = crate::oapv_bs::bsr_read1(bs);

    if (*fh).tile_size_present_in_fh_flag != 0 {
        let num_tiles = tile_cols * tile_rows;
        for i in 0..num_tiles as usize {
            let tile_size = crate::oapv_bs::bsr_read(bs, 32);
            if tile_size == 0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
            }
            if !pos_tiles.is_null() && cap >= num_tiles {
                if tile_size > i32::MAX as u32 {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
                }
                (*pos_tiles.add(i)).size = tile_size as i32;
            }
        }
    }
    ffi::OAPV_OK as i32
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_dc_coef(bs: *mut ffi::oapv_bs_t, dc_diff: *mut c_int, kparam_dc: *mut c_int) -> c_int {
    ffi_try!({
        let mut s = BsSt::load(bs);
        let ret = dc_coef_body(&mut s, &mut *dc_diff, &mut *kparam_dc);
        s.store(bs);
        ret
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline(always)]
fn dc_coef_body(s: &mut BsSt, dc_diff: &mut c_int, kparam_dc: &mut c_int) -> i32 {
    let abs_dc_diff = s.dec_vlc_read(*kparam_dc);
    if abs_dc_diff < 0 || abs_dc_diff > 65535 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    if abs_dc_diff != 0 {
        if s.leftbits == 0 {
            s.flush_1byte();
        }
        let sign = s.read_1bit();
        *dc_diff = oapv_set_sign(abs_dc_diff, sign);
        *kparam_dc = kparam_dc_of(abs_dc_diff);
    } else {
        *dc_diff = 0;
        *kparam_dc = OAPV_KPARAM_DC_MIN;
    }

    if s.is_eob != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    ffi::OAPV_OK as i32
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_ac_coef(bs: *mut ffi::oapv_bs_t, coef: *mut i16, kparam_ac: *mut c_int) -> c_int {
    ffi_try!({
        let mut s = BsSt::load(bs);
        // the caller passes the 64 coefficients of one block
        let ret = ac_coef_body(&mut s, &mut *(coef as *mut [i16; 64]), &mut *kparam_ac, &*core::ptr::addr_of!(ffi::oapv_tbl_scan));
        s.store(bs);
        ret
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline(always)]
fn ac_coef_body(s: &mut BsSt, coef: &mut [i16; 64], kparam_ac: &mut c_int, scan: &[u8; 64]) -> i32 {
    let mut scan_pos_offset: i32 = 1;

    let mut first_ac = 1;
    let mut k_run = OAPV_KPARAM_RUN_MIN;
    let mut k_ac = *kparam_ac;

    loop {
        if s.leftbits < 32 {
            s.topup();
        }

        let run = s.dec_vlc_read(k_run);

        if run < 0 || run > 64 - scan_pos_offset {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
        }

        scan_pos_offset += run;
        if scan_pos_offset >= 64 {
            if scan_pos_offset != 64 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
            }
            break;
        }
        k_run = kparam_run_of(run);

        let mut level = s.dec_vlc_read(k_ac);
        if level < 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
        }
        level += 1;
        if s.leftbits == 0 {
            s.flush_1byte();
        }
        let flag = s.read_1bit();

        let val = oapv_set_sign16(level, flag);
        if val < MIN_TX_VAL || val > MAX_TX_VAL {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
        }

        k_ac = kparam_ac_of(level);
        if first_ac != 0 {
            first_ac = 0;
            *kparam_ac = k_ac;
        }

        coef[scan[scan_pos_offset as usize] as usize] = val as i16;
        scan_pos_offset += 1;

        if scan_pos_offset >= 64 {
            break;
        }
    }

    if s.is_eob != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM;
    }
    ffi::OAPV_OK as i32
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_au_size(bs: *mut ffi::oapv_bs_t, au_size: *mut u32) -> c_int {
    ffi_try!({
        let size = crate::oapv_bs::bsr_read(bs, 32);
        if size == 0 || size == 0xFFFFFFFFu32 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        if bsr_is_unexpected_eob(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        *au_size = size;
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline]
pub(crate) unsafe fn dec_vlc_pbu_size(bs: *mut ffi::oapv_bs_t, pbu_size: *mut u32) -> c_int {
    let size = crate::oapv_bs::bsr_read(bs, 32);
    if size == 0 || size == 0xFFFFFFFFu32 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    if bsr_is_unexpected_eob(bs) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    *pbu_size = size;
    ffi::OAPV_OK as c_int
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_pbu_size(bs: *mut ffi::oapv_bs_t, pbu_size: *mut u32) -> c_int {
    ffi_try!(dec_vlc_pbu_size(bs, pbu_size), ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline]
pub(crate) unsafe fn dec_vlc_pbu_header(bs: *mut ffi::oapv_bs_t, pbuh: *mut ffi::oapv_pbuh_t) -> c_int {
    (*pbuh).pbu_type = crate::oapv_bs::bsr_read(bs, 8) as i32;
    if (*pbuh).pbu_type == 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    if (*pbuh).pbu_type >= 3 && (*pbuh).pbu_type <= 24 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    if (*pbuh).pbu_type >= 28 && (*pbuh).pbu_type <= 64 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    if (*pbuh).pbu_type >= 68 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    (*pbuh).group_id = crate::oapv_bs::bsr_read(bs, 16) as i32;
    if (*pbuh).group_id < 0 || (*pbuh).group_id > ffi::OAPV_MAX_GROUP_ID as i32 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    let reserved_zero = crate::oapv_bs::bsr_read(bs, 8) as i32;
    if reserved_zero != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    if bsr_is_unexpected_eob(bs) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }
    ffi::OAPV_OK as c_int
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_pbu_header(bs: *mut ffi::oapv_bs_t, pbuh: *mut ffi::oapv_pbuh_t) -> c_int {
    ffi_try!(dec_vlc_pbu_header(bs, pbuh), ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_frame_info(bs: *mut ffi::oapv_bs_t, fi: *mut ffi::oapv_fi_t) -> c_int {
    ffi_try!({
        (*fi).profile_idc = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fi).level_idc = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fi).band_idc = crate::oapv_bs::bsr_read(bs, 3) as i32;

        let mut reserved_zero = crate::oapv_bs::bsr_read(bs, 5) as i32;
        if reserved_zero != 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        (*fi).frame_width = crate::oapv_bs::bsr_read(bs, 24);
        if (*fi).frame_width == 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        (*fi).frame_height = crate::oapv_bs::bsr_read(bs, 24);
        if (*fi).frame_height == 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        (*fi).chroma_format_idc = crate::oapv_bs::bsr_read(bs, 4) as i32;
        if (*fi).chroma_format_idc < 0 || (*fi).chroma_format_idc > 4 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        if (*fi).chroma_format_idc == 1 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        (*fi).bit_depth = crate::oapv_bs::bsr_read(bs, 4) as i32;
        if (*fi).bit_depth < 2 || (*fi).bit_depth > 8 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        (*fi).bit_depth += 8;

        (*fi).capture_time_distance = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fi).use_companding = crate::oapv_bs::bsr_read1(bs);
        if (*fi).profile_idc != ffi::OAPV_PROFILE_444_16C12 as i32 && (*fi).profile_idc != ffi::OAPV_PROFILE_4444_16C12 as i32 {
            if (*fi).use_companding != 0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
        }

        reserved_zero = crate::oapv_bs::bsr_read(bs, 7) as i32;
        if reserved_zero != 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        if (*fi).chroma_format_idc == 2 {
            if (*fi).frame_width & 0x1 != 0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
        }

        if bsr_is_unexpected_eob(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline]
pub(crate) unsafe fn dec_vlc_au_info(bs: *mut ffi::oapv_bs_t, aui: *mut ffi::oapv_aui_t) -> c_int {
    (*aui).num_frames = crate::oapv_bs::bsr_read(bs, 16) as i32;
    if (*aui).num_frames > ffi::OAPV_MAX_NUM_FRAMES as i32 {
        return ffi::OAPV_ERR_REACHED_MAX as c_int;
    }
    for fidx in 0..(*aui).num_frames as usize {
        (*aui).pbu_type[fidx] = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*aui).group_id[fidx] = crate::oapv_bs::bsr_read(bs, 16) as i32;
        let reserved_zero_8bits = crate::oapv_bs::bsr_read(bs, 8) as i32;
        if reserved_zero_8bits != 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let ret = oapvd_vlc_frame_info(bs, &mut (*aui).frame_info[fidx]);
        if ret != 0 {
            return ret;
        }
    }
    let reserved_zero_8bits = crate::oapv_bs::bsr_read(bs, 8) as i32;
    if reserved_zero_8bits != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    if bsr_is_unexpected_eob(bs) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    crate::oapv_bs::bsr_align8(bs);
    ffi::OAPV_OK as c_int
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_au_info(bs: *mut ffi::oapv_bs_t, aui: *mut ffi::oapv_aui_t) -> c_int {
    ffi_try!(dec_vlc_au_info(bs, aui), ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[inline]
pub(crate) unsafe fn dec_vlc_frame_header(bs: *mut ffi::oapv_bs_t, fh: *mut ffi::oapv_fh_t, pos_tiles: *mut ffi::oapv_tile_pos_t, cap: c_int) -> c_int {
    let ret = oapvd_vlc_frame_info(bs, &mut (*fh).fi);
    if ret != 0 {
        return ret;
    }

    let mut reserved_zero = crate::oapv_bs::bsr_read(bs, 8) as i32;
    if reserved_zero != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    (*fh).color_description_present_flag = crate::oapv_bs::bsr_read1(bs);
    if (*fh).color_description_present_flag != 0 {
        (*fh).color_primaries = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fh).transfer_characteristics = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fh).matrix_coefficients = crate::oapv_bs::bsr_read(bs, 8) as i32;
        (*fh).full_range_flag = crate::oapv_bs::bsr_read1(bs);
    } else {
        (*fh).color_primaries = 2;
        (*fh).transfer_characteristics = 2;
        (*fh).matrix_coefficients = 2;
        (*fh).full_range_flag = 0;
    }
    (*fh).use_q_matrix = crate::oapv_bs::bsr_read1(bs);
    if (*fh).use_q_matrix != 0 {
        let ret = dec_vlc_q_matrix(bs, fh);
        if ret != 0 {
            return ret;
        }
    }

    let ret = dec_vlc_tile_info(bs, fh, pos_tiles, cap);
    if ret != 0 {
        return ret;
    }

    reserved_zero = crate::oapv_bs::bsr_read(bs, 8) as i32;
    if reserved_zero != 0 {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    if bsr_is_unexpected_eob(bs) {
        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
    }

    crate::oapv_bs::bsr_align8(bs);

    if (*fh).use_q_matrix == 0 {
        let num_comp = get_num_comp((*fh).fi.chroma_format_idc);
        for cidx in 0..num_comp as usize {
            for y in 0..8 {
                for x in 0..8 {
                    (*fh).q_matrix[cidx][y][x] = 16;
                }
            }
        }
    }

    ffi::OAPV_OK as c_int
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_frame_header(bs: *mut ffi::oapv_bs_t, fh: *mut ffi::oapv_fh_t, pos_tiles: *mut ffi::oapv_tile_pos_t, cap: c_int) -> c_int {
    ffi_try!(dec_vlc_frame_header(bs, fh, pos_tiles, cap), ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_tile_size(bs: *mut ffi::oapv_bs_t, tile_size: *mut u32) -> c_int {
    ffi_try!({
        let size = crate::oapv_bs::bsr_read(bs, 32);
        if size == 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        if bsr_is_unexpected_eob(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        *tile_size = size;
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_tile_header(bs: *mut ffi::oapv_bs_t, num_comp: c_int, th: *mut ffi::oapv_th_t, tile_size: u32, bit_depth: c_int) -> c_int {
    ffi_try!({
        let mut read_size: u32 = 0;

        (*th).tile_header_size = crate::oapv_bs::bsr_read(bs, 16) as i32;

        if (*th).tile_header_size != 5 + num_comp * 5 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        read_size = read_size.wrapping_add((*th).tile_header_size as u32);

        (*th).tile_index = crate::oapv_bs::bsr_read(bs, 16) as i32;
        for c in 0..num_comp as usize {
            (*th).tile_data_size[c] = crate::oapv_bs::bsr_read(bs, 32);
            let ret = oapv_validate_payload_bounds(tile_size as u64, read_size as u64, (*th).tile_data_size[c] as u64);
            if ret != 0 {
                return ret;
            }
            read_size = read_size.wrapping_add((*th).tile_data_size[c]);
        }
        for c in 0..num_comp as usize {
            (*th).tile_qp[c] = crate::oapv_bs::bsr_read(bs, 8) as i32;
            if (*th).tile_qp[c] < MIN_QUANT || (*th).tile_qp[c] > max_quant(bit_depth) {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
        }
        (*th).reserved_zero_8bits = crate::oapv_bs::bsr_read(bs, 8) as i32;
        if (*th).reserved_zero_8bits != 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        if bsr_is_unexpected_eob(bs) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }

        crate::oapv_bs::bsr_align8(bs);
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_tile_dummy_data(bs: *mut ffi::oapv_bs_t) -> c_int {
    ffi_try!({
        while bsr_get_left_byte(bs) > 0 {
            crate::oapv_bs::bsr_read(bs, 8);
            if bsr_is_unexpected_eob(bs) {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_metadata(bs: *mut ffi::oapv_bs_t, pbu_size: u32, mid: ffi::oapvm_t, group_id: c_int) -> c_int {
    ffi_try!({
        let mut metadata_size = crate::oapv_bs::bsr_read(bs, 32);
        if pbu_size < 8 || metadata_size > (pbu_size - 8) {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        let bs_start_pos = crate::oapv_bs::bsr_sink(bs) as *mut u8;

        while metadata_size > 0 {
            let mut payload_type: u32 = 0;
            let mut payload_size: u32 = 0;
            let mut t0: u32;
            loop {
                t0 = crate::oapv_bs::bsr_read(bs, 8);
                if bsr_is_unexpected_eob(bs) {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                }
                if metadata_size == 0 {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                }
                metadata_size -= 1;
                if t0 == 0xFF {
                    if payload_type > 0xFFFFFFFFu32 - 255 {
                        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                    }
                    payload_type += 255;
                }
                if t0 != 0xFF {
                    break;
                }
            }
            if payload_type > 0xFFFFFFFFu32 - t0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            payload_type += t0;

            loop {
                t0 = crate::oapv_bs::bsr_read(bs, 8);
                if bsr_is_unexpected_eob(bs) {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                }
                if metadata_size == 0 {
                    return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                }
                metadata_size -= 1;
                if t0 == 0xFF {
                    if payload_size > 0xFFFFFFFFu32 - 255 {
                        return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
                    }
                    payload_size += 255;
                }
                if t0 != 0xFF {
                    break;
                }
            }
            if payload_size > 0xFFFFFFFFu32 - t0 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            payload_size += t0;
            if payload_size > metadata_size {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }

            if bsr_get_left_byte(bs) < payload_size as i64 {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            let mut payload_data = crate::oapv_bs::bsr_sink(bs) as *mut u8;
            if payload_size == 0 {
                payload_data = core::ptr::null_mut();
            }
            bsr_move_byte_align(bs, payload_size as i64);
            if !mid.is_null() {
                let mut ret = crate::oapv_metadata::meta_set(mid, group_id, payload_type as i32, payload_data as *mut core::ffi::c_void, payload_size as i32);
                if ret == ffi::OAPV_ERR_INVALID_ARGUMENT as i32 {
                    ret = ffi::OAPV_ERR_MALFORMED_BITSTREAM as i32;
                }
                if ret != 0 {
                    return ret;
                }
            }
            metadata_size -= payload_size;
        }
        let target_read_size: i64 = (pbu_size - 8) as i64;
        let filled = (crate::oapv_bs::bsr_sink(bs) as *mut u8).offset_from(bs_start_pos);
        let filler_size = target_read_size - filled as i64;
        if filler_size < 0 {
            return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
        }
        if filler_size > 0 {
            let ret = oapvd_vlc_filler(bs, filler_size as u32);
            if ret != 0 {
                return ret;
            }
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

#[no_mangle]
pub extern "C" fn oapvd_vlc_filler(bs: *mut ffi::oapv_bs_t, filler_size: u32) -> c_int {
    ffi_try!({
        let mut filler_size = filler_size;
        while filler_size > 0 {
            let val = crate::oapv_bs::bsr_read(bs, 8);
            if bsr_is_unexpected_eob(bs) {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            if val != 0xFF {
                return ffi::OAPV_ERR_MALFORMED_BITSTREAM as c_int;
            }
            filler_size -= 1;
        }
        ffi::OAPV_OK as c_int
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}

}
