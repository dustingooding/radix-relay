use std::env;

fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++20")
        .compile("test_crate");

    // Ensure consistent MSVC runtime library linking on Windows
    if env::var("TARGET")
        .unwrap_or_default()
        .contains("windows-msvc")
    {
        let cflags = env::var("CFLAGS").unwrap_or_default();
        let cxxflags = env::var("CXXFLAGS").unwrap_or_default();

        // If debug runtime flags are detected, configure linker accordingly
        if cflags.contains("MDd") || cxxflags.contains("MDd") {
            println!("cargo:rustc-link-arg=/nodefaultlib:msvcrt");
            println!("cargo:rustc-link-arg=/defaultlib:msvcrtd");
            println!("cargo:warning=Using MSVC debug runtime (MSVCRTD) for CXX bridge");
        } else if cflags.contains("MD") || cxxflags.contains("MD") {
            println!("cargo:rustc-link-arg=/defaultlib:msvcrt");
            println!("cargo:warning=Using MSVC release runtime (MSVCRT) for CXX bridge");
        }
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-env-changed=CFLAGS");
    println!("cargo:rerun-if-env-changed=CXXFLAGS");
}
