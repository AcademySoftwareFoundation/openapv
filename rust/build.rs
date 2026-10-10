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
use std::env;
use std::fs;
use std::path::PathBuf;

const WRAPPER_HEADERS: [&str; 7] = [
    "inc/oapv.h",
    "src/oapv_def.h",
    "src/oapv_metadata.h",
    "src/oapv_bs.h",
    "src/oapv_tbl.h",
    "src/oapv_port.h",
    "src/oapv_blk.h",
];

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.join("..");
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());

    let wrapper = out.join("wrapper.h");
    let content = WRAPPER_HEADERS
        .iter()
        .map(|h| format!("#include \"{}\"\n", root.join(h).display()))
        .collect::<String>();
    fs::write(&wrapper, content).unwrap();
    for f in ["src/lib.rs", "src/oapv.rs", "src/oapv_bs.rs", "src/oapv_vlc.rs"] {
        println!("cargo:rerun-if-changed={}", manifest.join(f).display());
    }

    let bindings = bindgen::Builder::default()
        .header(wrapper.to_str().unwrap())
        .clang_arg("-I").clang_arg(root.join("inc").to_str().unwrap())
        .clang_arg("-I").clang_arg(root.join("src").to_str().unwrap())
        .allowlist_type("oapv.*")
        .allowlist_type("OAPV.*")
        .clang_arg("-DOAPV_STATIC_DEFINE")
        .allowlist_var("OAPV.*|N_C|Y_C|U_C|V_C|DEC_.*|MIN_QUANT|oapv_tbl_.*|oapve_tbl_.*")
        .allowlist_function("oapv.*")
        .ctypes_prefix("core::ffi")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("unable to generate bindings");

    bindings.write_to_file(out.join("bindings.rs")).expect("could not write bindings");
}
