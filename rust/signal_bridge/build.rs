fn main() {
    cxx_build::bridge("src/lib.rs").compile("signal_bridge");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
