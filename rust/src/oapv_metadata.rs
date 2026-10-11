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

// Rust port of oapv_metadata.c (metadata container API); generates the same
// symbols and is a drop-in replacement when CMake selects the Rust
// implementation. Crate manifest and bindgen config live in rust/.

use core::ffi::c_int;
use core::ffi::c_void;
use crate::ffi;

unsafe fn ops_malloc(ctx: *mut ffi::oapvm_ctx_t, size: u32) -> *mut c_void {
    match (*ctx).ops_mem.malloc {
        Some(f) => f((*ctx).ops_mem.udata, size),
        None => core::ptr::null_mut(),
    }
}

unsafe fn ops_free(ctx: *mut ffi::oapvm_ctx_t, ptr: *mut c_void) {
    if let Some(f) = (*ctx).ops_mem.free {
        f((*ctx).ops_mem.udata, ptr);
    }
}

unsafe fn meta_mdp_find_ud(md: *mut ffi::oapv_md_t, uuid: *const u8, prev_mdp: *mut *mut ffi::oapv_mdp_t) -> *mut ffi::oapv_mdp_t {
    let mut mdp = (*md).md_payload;
    *prev_mdp = core::ptr::null_mut();
    while !mdp.is_null() {
        if (*mdp).pld_type == ffi::OAPV_METADATA_USER_DEFINED as u32 {
            let usd = (*mdp).pld_data as *const ffi::oapv_md_usd_t;
            // uuid occupies the first 16 bytes of the payload
            if (*mdp).pld_size >= 16 {
                let a = core::slice::from_raw_parts(uuid, 16);
                let b = core::slice::from_raw_parts(core::ptr::addr_of!((*usd).uuid) as *const u8, 16);
                if a == b {
                    return mdp;
                }
            }
        }
        *prev_mdp = mdp;
        mdp = (*mdp).next;
    }
    core::ptr::null_mut()
}

unsafe fn meta_mdp_find_non_ud(md: *mut ffi::oapv_md_t, mdt: c_int, prev_mdp: *mut *mut ffi::oapv_mdp_t) -> *mut ffi::oapv_mdp_t {
    let mut mdp = (*md).md_payload;
    *prev_mdp = core::ptr::null_mut();
    while !mdp.is_null() {
        if (*mdp).pld_type == mdt as u32 {
            break;
        }
        *prev_mdp = mdp;
        mdp = (*mdp).next;
    }
    mdp
}

unsafe fn meta_find_mdp(md: *mut ffi::oapv_md_t, mdt: c_int, uuid: *const u8) -> *mut ffi::oapv_mdp_t {
    let mut prev_mdp: *mut ffi::oapv_mdp_t = core::ptr::null_mut();
    if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int {
        meta_mdp_find_ud(md, uuid, &mut prev_mdp)
    } else {
        meta_mdp_find_non_ud(md, mdt, &mut prev_mdp)
    }
}

unsafe fn meta_md_find_mdp_with_prev(
    md: *mut ffi::oapv_md_t,
    mdt: c_int,
    uuid: *const u8,
    prev_mdp: *mut *mut ffi::oapv_mdp_t,
) -> *mut ffi::oapv_mdp_t {
    if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int {
        meta_mdp_find_ud(md, uuid, prev_mdp)
    } else {
        meta_mdp_find_non_ud(md, mdt, prev_mdp)
    }
}

unsafe fn meta_rm_mdp(ctx: *mut ffi::oapvm_ctx_t, md: *mut ffi::oapv_md_t, mdt: c_int, uuid: *const u8) -> c_int {
    let mut mdp_prev: *mut ffi::oapv_mdp_t = core::ptr::null_mut();
    let mdp = meta_md_find_mdp_with_prev(md, mdt, uuid, &mut mdp_prev);
    if mdp.is_null() {
        return ffi::OAPV_ERR_NOT_FOUND as c_int;
    }
    if mdp_prev.is_null() {
        (*md).md_payload = (*mdp).next;
    } else {
        (*mdp_prev).next = (*mdp).next;
    }

    ops_free(ctx, (*mdp).pld_data);
    ops_free(ctx, mdp as *mut c_void);
    (*md).mdp_num -= 1;
    ffi::OAPV_OK as c_int
}

unsafe fn meta_get_num_mdp(ctx: *mut ffi::oapvm_ctx_t) -> c_int {
    let mut num = 0;
    for i in 0..(*ctx).num as usize {
        num += (*ctx).md_arr[i].mdp_num;
    }
    num
}

unsafe fn meta_find_md(ctx: *mut ffi::oapvm_ctx_t, group_id: c_int) -> *mut ffi::oapv_md_t {
    for n in 0..(*ctx).num as usize {
        if group_id == (*ctx).md_arr[n].group_id {
            return core::ptr::addr_of_mut!((*ctx).md_arr[n]);
        }
    }
    core::ptr::null_mut()
}

unsafe fn meta_verify_mdp_data(mdt: c_int, size: c_int, data: *mut u8) -> c_int {
    if mdt == ffi::OAPV_METADATA_ITU_T_T35 as c_int {
        if size == 0 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if *data == 0xFF {
            if size == 1 {
                return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
        }
    } else if mdt == ffi::OAPV_METADATA_MDCV as c_int {
        if size != 24 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
    } else if mdt == ffi::OAPV_METADATA_CLL as c_int {
        if size != 4 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
    } else if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int {
        if size < 16 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
    } else if mdt == ffi::OAPV_METADATA_FILLER as c_int {
        for i in 0..size as isize {
            if *data.offset(i) != 0xFF {
                return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
        }
    } else {
        return ffi::OAPV_OK as c_int; // undefined payload type
    }
    ffi::OAPV_OK as c_int
}

unsafe fn meta_free_md(ctx: *mut ffi::oapvm_ctx_t, md: *mut ffi::oapv_md_t) {
    let mut mdp = (*md).md_payload;
    while !mdp.is_null() {
        ops_free(ctx, (*mdp).pld_data);
        let mdp_t = mdp;
        mdp = (*mdp).next;
        ops_free(ctx, mdp_t as *mut c_void);
    }
}

#[no_mangle]
pub extern "C" fn oapvm_id_to_ctx(id: ffi::oapvm_t) -> *mut ffi::oapvm_ctx_t {
    ffi_try!({
        if id.is_null() {
            return core::ptr::null_mut();
        }
        let ctx = id as *mut ffi::oapvm_ctx_t;
        if (*ctx).magic != ffi::OAPVM_MAGIC_CODE {
            return core::ptr::null_mut();
        }
        ctx
    }, core::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn oapvm_write_mdcv(mdcv: *mut ffi::oapvm_payload_mdcv_t, data: *mut c_void, size: *mut c_int) -> c_int {
    ffi_try!({
        if mdcv.is_null() || data.is_null() || size.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        ffi::oapv_bsw_init(&mut bs, data as *mut u8, 24, None);

        for i in 0..3usize {
            let t = (*mdcv).primary_chromaticity_x[i];
            if !(t >= 0 && t <= 0xFFFF) {
                return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
            ffi::oapv_bsw_write(&mut bs, t as u32, 16);

            let t = (*mdcv).primary_chromaticity_y[i];
            if !(t >= 0 && t <= 0xFFFF) {
                return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
            ffi::oapv_bsw_write(&mut bs, t as u32, 16);
        }
        let t = (*mdcv).white_point_chromaticity_x;
        if !(t >= 0 && t <= 0xFFFF) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        ffi::oapv_bsw_write(&mut bs, t as u32, 16);

        let t = (*mdcv).white_point_chromaticity_y;
        if !(t >= 0 && t <= 0xFFFF) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        ffi::oapv_bsw_write(&mut bs, t as u32, 16);

        if (*mdcv).max_mastering_luminance > 0xFFFF_FFFF {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let tu32 = (*mdcv).max_mastering_luminance as u32;
        ffi::oapv_bsw_write(&mut bs, tu32, 32);

        if (*mdcv).min_mastering_luminance > 0xFFFF_FFFF {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let tu32 = (*mdcv).min_mastering_luminance as u32;
        ffi::oapv_bsw_write(&mut bs, tu32, 32);

        ffi::oapv_bsw_deinit(&mut bs);

        *size = 24; // MDCV payload has 24 bytes
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_read_mdcv(data: *mut c_void, size: c_int, mdcv: *mut ffi::oapvm_payload_mdcv_t) -> c_int {
    ffi_try!({
        if data.is_null() || mdcv.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if size < 24 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        ffi::oapv_bsr_init(&mut bs, data as *mut u8, size as u32, None);

        for i in 0..3usize {
            (*mdcv).primary_chromaticity_x[i] = ffi::oapv_bsr_read(&mut bs, 16) as c_int;
        }
        for i in 0..3usize {
            (*mdcv).primary_chromaticity_y[i] = ffi::oapv_bsr_read(&mut bs, 16) as c_int;
        }
        (*mdcv).white_point_chromaticity_x = ffi::oapv_bsr_read(&mut bs, 16) as c_int;
        (*mdcv).white_point_chromaticity_y = ffi::oapv_bsr_read(&mut bs, 16) as c_int;

        (*mdcv).max_mastering_luminance = ffi::oapv_bsr_read(&mut bs, 32) as core::ffi::c_ulong;
        (*mdcv).min_mastering_luminance = ffi::oapv_bsr_read(&mut bs, 32) as core::ffi::c_ulong;
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_write_cll(cll: *mut ffi::oapvm_payload_cll_t, data: *mut c_void, size: *mut c_int) -> c_int {
    ffi_try!({
        if cll.is_null() || data.is_null() || size.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        ffi::oapv_bsw_init(&mut bs, data as *mut u8, 4, None);

        let t = (*cll).max_cll;
        if !(t >= 0 && t <= 0xFFFF) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        ffi::oapv_bsw_write(&mut bs, t as u32, 16);

        let t = (*cll).max_fall;
        if !(t >= 0 && t <= 0xFFFF) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        ffi::oapv_bsw_write(&mut bs, t as u32, 16);

        ffi::oapv_bsw_deinit(&mut bs);

        *size = 4; // CLL payload has 4 bytes
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_read_cll(data: *mut c_void, size: c_int, cll: *mut ffi::oapvm_payload_cll_t) -> c_int {
    ffi_try!({
        if data.is_null() || cll.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if size < 4 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let mut bs: ffi::oapv_bs_t = core::mem::zeroed();
        ffi::oapv_bsr_init(&mut bs, data as *mut u8, size as u32, None);

        (*cll).max_cll = ffi::oapv_bsr_read(&mut bs, 16) as c_int;
        (*cll).max_fall = ffi::oapv_bsr_read(&mut bs, 16) as c_int;
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_set(mid: ffi::oapvm_t, group_id: c_int, mdt: c_int, data: *mut c_void, size: c_int) -> c_int {
    ffi_try!({
        let mut pld_data_new: *mut c_void = core::ptr::null_mut();
        let mut mdp_new: *mut ffi::oapv_mdp_t = core::ptr::null_mut();

        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if !((!data.is_null() && size > 0) || (data.is_null() && size == 0)) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        // the group ID is written into the metadata PBU header
        if !(group_id >= 0 && group_id <= ffi::OAPV_MAX_GROUP_ID as c_int) {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        let ret = meta_verify_mdp_data(mdt, size, data as *mut u8);
        if ret != 0 {
            return ret;
        }

        let mut uuid: *const u8 = core::ptr::null();
        if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int {
            uuid = data as *const u8;
        }

        if size > 0 {
            pld_data_new = ops_malloc(ctx, size as u32);
            if pld_data_new.is_null() {
                return ffi::OAPV_ERR_OUT_OF_MEMORY as c_int;
            }
            core::ptr::copy_nonoverlapping(data as *const u8, pld_data_new as *mut u8, size as usize);
        }

        // a new group is added only once the payload can be stored, so that a
        // failure leaves no empty group behind
        let mut md = meta_find_md(ctx, group_id);
        if md.is_null() && (*ctx).num >= ffi::OAPV_MAX_NUM_METAS as c_int {
            ops_free(ctx, mdp_new as *mut c_void);
            ops_free(ctx, pld_data_new);
            return ffi::OAPV_ERR_REACHED_MAX as c_int;
        }

        let mdp_t = if !md.is_null() { meta_find_mdp(md, mdt, uuid) } else { core::ptr::null_mut() };
        if mdp_t.is_null() {
            // add new one
            if meta_get_num_mdp(ctx) >= ffi::OAPV_MAX_NUM_META_PAYLOADS as c_int {
                ops_free(ctx, mdp_new as *mut c_void);
                ops_free(ctx, pld_data_new);
                return ffi::OAPV_ERR_REACHED_MAX as c_int;
            }
            mdp_new = ops_malloc(ctx, core::mem::size_of::<ffi::oapv_mdp_t>() as u32) as *mut ffi::oapv_mdp_t;
            if mdp_new.is_null() {
                ops_free(ctx, pld_data_new);
                return ffi::OAPV_ERR_OUT_OF_MEMORY as c_int;
            }
            if md.is_null() {
                md = core::ptr::addr_of_mut!((*ctx).md_arr[(*ctx).num as usize]);
                (*md).group_id = group_id;
                (*md).mdp_num = 0;
                (*md).md_payload = core::ptr::null_mut();
                (*ctx).num += 1;
            }
            (*mdp_new).pld_size = size as u32;
            (*mdp_new).pld_type = mdt as u32;
            (*mdp_new).pld_data = pld_data_new;
            (*mdp_new).next = (*md).md_payload; // add to head

            (*md).md_payload = mdp_new;
            (*md).mdp_num += 1;
        } else {
            // replace the existing one
            (*mdp_t).pld_size = size as u32;
            ops_free(ctx, (*mdp_t).pld_data);
            (*mdp_t).pld_data = pld_data_new;
        }
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_get(mid: ffi::oapvm_t, group_id: c_int, mdt: c_int, data: *mut *mut c_void, size: *mut c_int, uuid: *mut u8) -> c_int {
    ffi_try!({
        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if data.is_null() || size.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        // user-defined lookup compares a 16-byte uuid; it must be provided
        if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int && uuid.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let md = meta_find_md(ctx, group_id);
        let mdp = if !md.is_null() { meta_find_mdp(md, mdt, uuid) } else { core::ptr::null_mut() };
        if mdp.is_null() {
            return ffi::OAPV_ERR_NOT_FOUND as c_int;
        }

        *data = (*mdp).pld_data;
        *size = (*mdp).pld_size as c_int;
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_rem(mid: ffi::oapvm_t, group_id: c_int, mdt: c_int, uuid: *mut u8) -> c_int {
    ffi_try!({
        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        // user-defined lookup compares a 16-byte uuid; it must be provided
        if mdt == ffi::OAPV_METADATA_USER_DEFINED as c_int && uuid.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        let md = meta_find_md(ctx, group_id);
        if md.is_null() {
            return ffi::OAPV_ERR_NOT_FOUND as c_int;
        }
        meta_rm_mdp(ctx, md, mdt, uuid)
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_set_all(mid: ffi::oapvm_t, pld: *mut ffi::oapvm_payload_t, num_plds: c_int) -> c_int {
    ffi_try!({
        if pld.is_null() || num_plds < 0 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        for i in 0..num_plds as usize {
            let ret = oapvm_set(mid, (*pld.add(i)).group_id, (*pld.add(i)).type_, (*pld.add(i)).data, (*pld.add(i)).size);
            if ret != 0 {
                return ret;
            }
        }
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_get_all(mid: ffi::oapvm_t, pld: *mut ffi::oapvm_payload_t, num_plds: *mut c_int) -> c_int {
    ffi_try!({
        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if num_plds.is_null() {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }
        if pld.is_null() {
            let num_payload = meta_get_num_mdp(ctx);
            if num_payload > ffi::OAPV_MAX_NUM_META_PAYLOADS as c_int || num_payload < 0 {
                return ffi::OAPV_ERR_REACHED_MAX as c_int;
            }
            *num_plds = num_payload;
            return ffi::OAPV_OK as c_int;
        }
        let mut pld_cnt: c_int = 0;
        for i in 0..(*ctx).num as usize {
            let cur_md = core::ptr::addr_of_mut!((*ctx).md_arr[i]);
            let group_id = (*cur_md).group_id;
            let mut mdp = (*cur_md).md_payload;
            while !mdp.is_null() {
                if pld_cnt >= *num_plds {
                    return ffi::OAPV_ERR_REACHED_MAX as c_int;
                }
                let e = pld.add(pld_cnt as usize);
                (*e).group_id = group_id;
                (*e).size = (*mdp).pld_size as c_int;
                (*e).data = (*mdp).pld_data;
                (*e).type_ = (*mdp).pld_type as c_int;
                if (*e).type_ == ffi::OAPV_METADATA_USER_DEFINED as c_int {
                    core::ptr::copy_nonoverlapping((*mdp).pld_data as *const u8, core::ptr::addr_of_mut!((*e).uuid) as *mut u8, 16);
                }
                pld_cnt += 1;
                mdp = (*mdp).next;
            }
        }
        *num_plds = pld_cnt;
        ffi::OAPV_OK as c_int
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapvm_rem_all(mid: ffi::oapvm_t) {
    ffi_try!({
        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return;
        }
        for i in 0..(*ctx).num as usize {
            meta_free_md(ctx, core::ptr::addr_of_mut!((*ctx).md_arr[i]));
            core::ptr::write_bytes(core::ptr::addr_of_mut!((*ctx).md_arr[i]) as *mut u8, 0, core::mem::size_of::<ffi::oapv_md_t>());
        }
        (*ctx).num = 0;
    }, ())
}

unsafe fn meta_ctx_alloc(ops: *const ffi::oapv_ops_mem_t) -> *mut ffi::oapvm_ctx_t {
    let ctx = match (*ops).malloc {
        Some(f) => f((*ops).udata, core::mem::size_of::<ffi::oapvm_ctx_t>() as u32),
        None => core::ptr::null_mut(),
    } as *mut ffi::oapvm_ctx_t;

    if ctx.is_null() {
        return core::ptr::null_mut();
    }
    core::ptr::write_bytes(ctx as *mut u8, 0, core::mem::size_of::<ffi::oapvm_ctx_t>());
    (*ctx).ops_mem = *ops;

    ctx
}

unsafe fn meta_ctx_free(ctx: *mut ffi::oapvm_ctx_t) {
    if let Some(f) = (*ctx).ops_mem.free {
        f((*ctx).ops_mem.udata, ctx as *mut c_void);
    }
}

#[no_mangle]
pub extern "C" fn oapvm_create(cdesc: *mut ffi::oapvm_cdesc_t, err: *mut c_int) -> ffi::oapvm_t {
    ffi_try!({
        if cdesc.is_null() {
            if !err.is_null() {
                *err = ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
            }
            return core::ptr::null_mut();
        }

        let mut ops: ffi::oapv_ops_mem_t = core::mem::zeroed();
        let ret = ffi::oapv_ops_mem_set(&mut ops, (*cdesc).ops_mem);
        if ret != ffi::OAPV_OK as c_int {
            if !err.is_null() {
                *err = ret;
            }
            return core::ptr::null_mut();
        }

        let ctx = meta_ctx_alloc(&ops);
        if ctx.is_null() {
            if !err.is_null() {
                *err = ffi::OAPV_ERR_OUT_OF_MEMORY as c_int;
            }
            return core::ptr::null_mut();
        }
        core::ptr::copy_nonoverlapping(
            cdesc as *const u8,
            core::ptr::addr_of_mut!((*ctx).cdesc) as *mut u8,
            core::mem::size_of::<ffi::oapvm_cdesc_t>(),
        );

        (*ctx).magic = ffi::OAPVM_MAGIC_CODE;
        if !err.is_null() {
            *err = ffi::OAPV_OK as c_int;
        }
        ctx as ffi::oapvm_t
    }, core::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn oapvm_delete(mid: ffi::oapvm_t) {
    ffi_try!({
        let ctx = oapvm_id_to_ctx(mid);
        if ctx.is_null() {
            return;
        }
        for i in 0..(*ctx).num as usize {
            meta_free_md(ctx, core::ptr::addr_of_mut!((*ctx).md_arr[i]));
        }
        meta_ctx_free(ctx);
    }, ())
}
