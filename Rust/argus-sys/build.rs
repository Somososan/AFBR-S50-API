extern crate bindgen;

use std::env;
use std::path::PathBuf;

use bindgen::CargoCallbacks;

fn main() {
    // This is the directory where the `c` library is located.
    let libdir_path = PathBuf::from(".")
        // Canonicalize the path as `rustc-link-search` requires an absolute
        // path.
        .canonicalize()
        .expect("cannot canonicalize path");

    // This is the path to the `c` headers file.
    let headers_path = libdir_path.join("wrapper.h");
    let headers_path_str = headers_path.to_str().expect("Path is not a valid string");

    // This is the path to the static library file.
    let lib_path = libdir_path.join("libafbrs50_m4_fpu.a");

    // Tell cargo to look for shared libraries in the specified directory
    println!("cargo:rustc-link-search={}", libdir_path.to_str().unwrap());
    println!("cargo:rustc-link-search={}", libdir_path.join("AFBR-S50/Include/api").to_str().unwrap());

    // Tell cargo to tell rustc to link our `hello` library. Cargo will
    // automatically know it must look for a `libhello.a` file.
    println!("cargo:rustc-link-lib=afbrs50_m4_fpu");

    // Tell cargo to invalidate the built crate whenever the header changes.
    println!("cargo:rerun-if-changed={}", headers_path_str);

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(headers_path_str)
        .clang_args(&[
            "-IC:/Users/C Karaliolios/Dropbox/Code/Werk/AFBR-S50-API/AFBR-S50/Include/api",
            "-IC:/Users/C Karaliolios/Dropbox/Code/Werk/AFBR-S50-API/AFBR-S50/Include/platform",
            "-IC:/Users/C Karaliolios/Dropbox/Code/Werk/AFBR-S50-API/AFBR-S50/Include/utility",
        ])
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(CargoCallbacks))
        .use_core()
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("bindings.rs");
    bindings
        .write_to_file(out_path)
        .expect("Couldn't write bindings!");
}
