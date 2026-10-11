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
// Crate root: pulls in the Rust ports of the C modules. Each module file
// mirrors one C source (oapv_vlc.rs <-> oapv_vlc.c) and provides the same
// symbols when CMake selects the Rust implementation (OAPV_USE_RUST).

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

use core::cell::Cell;
use core::ffi::c_int;

thread_local! {
    // set when a panic is caught on this thread, until the C side takes it
    static PANICKED: Cell<bool> = const { Cell::new(false) };
}

fn set_panic() {
    PANICKED.with(|p| p.set(true));
}

fn take_panic() -> bool {
    PANICKED.with(|p| p.replace(false))
}

// internal functions: catches a panic at the C ABI boundary, records it for
// the C caller and returns $on_err, the value the C function fails with
macro_rules! ffi_try {
    ($body:expr, $on_err:expr) => {{
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| { unsafe { $body } }));
        match r {
            Ok(v) => v,
            Err(_) => {
                crate::set_panic();
                $on_err
            }
        }
    }};
}

// public API: a panic caught here or in a callee ends the call with $on_panic
macro_rules! ffi_api {
    ($body:expr, $on_panic:expr) => {{
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| { unsafe { $body } }));
        let panicked = crate::take_panic();
        match r {
            Ok(v) if !panicked => v,
            _ => $on_panic,
        }
    }};
}

// lets the C side turn a panic caught on this thread into OAPV_ERR_INTERNAL_PANIC
#[no_mangle]
pub extern "C" fn oapv_rust_take_panic() -> c_int {
    take_panic() as c_int
}

mod ffi {
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod oapv;
mod oapv_bs;
mod oapv_metadata;
mod oapv_vlc;
