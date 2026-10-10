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
 * Rust port of oapv_bs.c (bitstream read/write primitives); generates the
 * same symbols and is a bit-exact, drop-in replacement when CMake selects
 * the Rust implementation. Crate manifest and bindgen config live in
 * the rust/ crate.
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

use self::ffi::oapv_bs_t;

///////////////////////////////////////////////////////////////////////////////
// encoder side
///////////////////////////////////////////////////////////////////////////////

unsafe fn bsw_get_sink_byte(bs: *const oapv_bs_t) -> c_int {
    (64 - (*bs).leftbits + 7) >> 3
}

unsafe extern "C" fn bsw_flush(bs: *mut oapv_bs_t, bytes: c_int) -> c_int {
    let mut bytes = bytes;
    if bytes == 0 {
        bytes = bsw_get_sink_byte(bs);
    }

    if (*bs).cur.add(bytes as usize) > (*bs).end {
        return -1;
    }

    while bytes > 0 {
        *(*bs).cur = ((*bs).code >> 56) as u8;
        (*bs).cur = (*bs).cur.add(1);
        (*bs).code <<= 8;
        bytes -= 1;
    }

    (*bs).leftbits = 64;

    0
}

#[no_mangle]
pub extern "C" fn oapv_bsw_init(bs: *mut oapv_bs_t, buf: *mut u8, size: c_int, fn_flush: ffi::oapv_bs_fn_flush_t) {
    ffi_try!({
        (*bs).size = size as u32;
        (*bs).beg = buf;
        (*bs).cur = buf;
        (*bs).end = buf.add(size as usize);
        (*bs).code = 0;
        (*bs).leftbits = 64;
        (*bs).fn_flush = fn_flush.or(Some(bsw_flush));
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsw_deinit(bs: *mut oapv_bs_t) {
    ffi_try!({
        if let Some(f) = (*bs).fn_flush {
            f(bs, 0);
        }
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsw_sink(bs: *mut oapv_bs_t) -> *mut core::ffi::c_void {
    ffi_try!({
        if (*bs).cur.add(bsw_get_sink_byte(bs) as usize) >= (*bs).end {
            return core::ptr::null_mut();
        }
        if let Some(f) = (*bs).fn_flush {
            f(bs, 0);
        }
        (*bs).code = 0;
        (*bs).leftbits = 64;
        (*bs).cur as *mut core::ffi::c_void
    }, core::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn oapv_bsw_write_direct(addr: *mut core::ffi::c_void, val: u32, len: c_int) -> c_int {
    ffi_try!({
        if len & 0x7 != 0 {
            return -1;
        }
        let p = addr as *mut u8;
        let mut v = val.wrapping_shl((32 - len) as u32);
        for i in 0..(len >> 3) as usize {
            *p.add(i) = (v >> 24) as u8;
            v <<= 8;
        }
        0
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapv_bsw_write1(bs: *mut oapv_bs_t, val: c_int) -> c_int {
    ffi_try!({
        (*bs).leftbits -= 1;
        (*bs).code |= ((val & 0x1) as u64) << (*bs).leftbits;

        if (*bs).leftbits == 0 {
            if (*bs).cur >= (*bs).end {
                return -1;
            }
            if let Some(f) = (*bs).fn_flush {
                f(bs, 0);
            }

            (*bs).code = 0;
            (*bs).leftbits = 64;
        }

        0
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapv_bsw_write(bs: *mut oapv_bs_t, val: u32, len: c_int) -> c_int {
    ffi_try!({
        let leftbits = (*bs).leftbits;
        let code_t = (val as u64).wrapping_shl((64 - len) as u32);
        (*bs).code |= code_t.wrapping_shr((64 - leftbits) as u32);

        if len < leftbits {
            (*bs).leftbits -= len;
        } else {
            if let Some(f) = (*bs).fn_flush {
                f(bs, 8);
            }
            (*bs).code = code_t.wrapping_shl(leftbits as u32);
            (*bs).leftbits = 64 - (len - leftbits);
        }
        0
    }, -1)
}

///////////////////////////////////////////////////////////////////////////////
// decoder side
///////////////////////////////////////////////////////////////////////////////

unsafe fn bsr_skip_code(bs: *mut oapv_bs_t, size: c_int) {
    if size == 64 {
        (*bs).code = 0;
        (*bs).leftbits = 0;
    } else {
        (*bs).code <<= size;
        (*bs).leftbits -= size;
    }
}

unsafe extern "C" fn bsr_flush(bs: *mut oapv_bs_t, byte: c_int) -> c_int {
    let mut shift = 56i32;
    let mut code: u64 = 0;

    let remained = (*bs).end.offset_from((*bs).cur) as c_int;
    let mut byte = byte;
    if byte > remained {
        byte = remained;
    }

    if byte <= 0 {
        (*bs).code = 0;
        (*bs).leftbits = 0;
        (*bs).is_eob = 1;
        return -1;
    }

    (*bs).leftbits = byte << 3;

    while byte > 0 {
        code |= (*(*bs).cur as u64) << shift;
        (*bs).cur = (*bs).cur.add(1);
        byte -= 1;
        shift -= 8;
    }
    (*bs).code = code;
    0
}

#[no_mangle]
pub extern "C" fn oapv_bsr_init(bs: *mut oapv_bs_t, buf: *mut u8, size: u32, fn_flush: ffi::oapv_bs_fn_flush_t) {
    ffi_try!({
        core::ptr::write_bytes(bs as *mut u8, 0, core::mem::size_of::<oapv_bs_t>());

        (*bs).size = size;
        (*bs).cur = buf;
        (*bs).beg = buf;
        (*bs).end = buf.add(size as usize);
        (*bs).code = 0;
        (*bs).leftbits = 0;
        (*bs).is_eob = 0;
        (*bs).fn_flush = fn_flush.or(Some(bsr_flush));
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsr_align8(bs: *mut oapv_bs_t) {
    ffi_try!({
        let size = (*bs).leftbits & 0x7;

        (*bs).code <<= size;
        (*bs).leftbits -= size;
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsr_skip(bs: *mut oapv_bs_t, size: c_int) {
    ffi_try!({
        let mut size = size;
        if (*bs).leftbits < size {
            size -= (*bs).leftbits;
            if let Some(f) = (*bs).fn_flush {
                if f(bs, 8) != 0 {
                    return;
                }
            }
            if (*bs).leftbits < size {
                (*bs).is_eob = 1;
                return;
            }
        }
        bsr_skip_code(bs, size);
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsr_sink(bs: *mut oapv_bs_t) -> *mut core::ffi::c_void {
    ffi_try!({
        if (*bs).leftbits & 7 != 0 {
            return core::ptr::null_mut();
        }
        (*bs).cur = (*bs).cur.sub(((*bs).leftbits >> 3) as usize);
        (*bs).code = 0;
        (*bs).leftbits = 0;
        (*bs).cur as *mut core::ffi::c_void
    }, core::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn oapv_bsr_move(bs: *mut oapv_bs_t, pos: *mut u8) {
    ffi_try!({
        (*bs).code = 0;
        (*bs).leftbits = 0;
        (*bs).cur = pos;
    }, ())
}

#[no_mangle]
pub extern "C" fn oapv_bsr_read(bs: *mut oapv_bs_t, size: c_int) -> u32 {
    ffi_try!({
        let mut code: u32 = 0;
        let mut size = size;

        if (*bs).leftbits < size {
            code = ((*bs).code >> (64 - size)) as u32;
            size -= (*bs).leftbits;
            if let Some(f) = (*bs).fn_flush {
                if f(bs, 8) != 0 {
                    return u32::MAX;
                }
            }
            if (*bs).leftbits < size {
                (*bs).is_eob = 1;
                return u32::MAX;
            }
        }
        code |= ((*bs).code >> (64 - size)) as u32;

        bsr_skip_code(bs, size);

        code
    }, u32::MAX)
}

#[no_mangle]
pub extern "C" fn oapv_bsr_read1(bs: *mut oapv_bs_t) -> c_int {
    ffi_try!({
        if (*bs).leftbits == 0 {
            if let Some(f) = (*bs).fn_flush {
                if f(bs, 8) != 0 {
                    return -1;
                }
            }
        }
        let code = ((*bs).code >> 63) as c_int;

        (*bs).code <<= 1;
        (*bs).leftbits -= 1;

        code
    }, -1)
}

#[no_mangle]
pub extern "C" fn oapv_bsr_read_direct(addr: *const core::ffi::c_void, len: c_int, out: *mut u32) -> c_int {
    ffi_try!({
        if addr.is_null() || out.is_null() || len <= 0 || len > 32 {
            return ffi::OAPV_ERR_INVALID_ARGUMENT as c_int;
        }

        let mut code: u32 = 0;
        let mut shift = 24i32;
        let mut p = addr as *const u8;
        let mut byte = (len + 7) >> 3;

        while byte > 0 {
            code |= (*p as u32) << shift;
            shift -= 8;
            byte -= 1;
            p = p.add(1);
        }
        *out = code >> (32 - len);
        0
    }, ffi::OAPV_ERR_UNEXPECTED as c_int)
}
