//! Compile the vendored C into one static library and embed the pack.
//!
//! Four trees: the phronesis library sources, the Janet amalgamation, the
//! c-capnproto runtime and the capnp-janet runtime, plus the C generated
//! from the schema and the ShellCheck shim. Flags follow meson.build in the
//! repository root. The pack under `policy/` is written into OUT_DIR as a
//! table of embedded files; the library writes it to a cache directory at
//! run time and names it through `PHRONESIS_JANET_PACK`.

use std::path::{Path, PathBuf};

fn files_in(dir: &str, ext: &str, skip: &[&str]) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = std::fs::read_dir(dir)
        .unwrap_or_else(|e| panic!("{dir}: {e}"))
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == ext))
        .filter(|p| {
            let name = p.file_name().unwrap().to_string_lossy();
            !skip.contains(&name.as_ref())
        })
        .collect();
    out.sort();
    out
}

fn main() {
    for dir in ["vendor", "csrc", "policy"] {
        println!("cargo:rerun-if-changed={dir}");
    }
    let janet = "vendor/janet";
    let capn = "vendor/c-capnproto";
    let cj = "vendor/capnp-janet";
    let ph = "vendor/phronesis";
    let gen = "vendor/capnp";

    // Janet, as meson builds janet_amalg: a host without processes.
    cc::Build::new()
        .file(format!("{janet}/janet.c"))
        .include(janet)
        .define("JANET_API", "")
        .define("JANET_NO_PROCESSES", None)
        .define("JANET_SPAWN_NO_CHDIR", None)
        .warnings(false)
        .compile("janet_amalg");

    // The Cap'n Proto C runtime, static.
    cc::Build::new()
        .files(files_in(capn, "c", &[]))
        .include(capn)
        .define("CAPNP_C_STATIC", None)
        .warnings(false)
        .compile("capnp_c");

    // capnp-janet: the runtime, and the Janet module the pack loads.
    cc::Build::new()
        .files(files_in(&format!("{cj}/src"), "c", &[]))
        .include(format!("{cj}/include"))
        .include(capn)
        .include(janet)
        .define("JANET_API", "")
        .define("CAPNP_C_STATIC", None)
        .warnings(false)
        .compile("capnp_janet");

    // phronesis itself, the generated schema C and the ShellCheck shim.
    // PHRONESIS_ARGV_LIST follows the wrap's generator, which types
    // List(Text) as capn_ptr_list. The default pack path is unreachable on
    // purpose: the embedded pack is named through the environment.
    let mut lib = cc::Build::new();
    lib.files(files_in(&format!("{ph}/src"), "c", &[]))
        .files(files_in(gen, "c", &[]))
        .file("csrc/shell_encode.c")
        .include(format!("{ph}/include"))
        .include(format!("{ph}/src"))
        .include(gen)
        .include(capn)
        .include(format!("{cj}/include"))
        .include(janet)
        .define("_GNU_SOURCE", None)
        .define("JANET_API", "")
        .define("CAPNP_C_STATIC", None)
        .define("PHRONESIS_ARGV_LIST", "1")
        .define(
            "PHRONESIS_DEFAULT_JANET_PACK",
            "\"/nonexistent/phronesis/policy/shell.janet\"",
        )
        .flag_if_supported("-std=gnu11")
        .warnings(false);
    lib.compile("phronesis");

    for l in ["dl", "m", "pthread"] {
        println!("cargo:rustc-link-lib={l}");
    }

    embed_pack(Path::new("policy"));
}

/// Write `pack.rs`: the pack files as `(relative path, bytes)` pairs, in a
/// fixed order, so the hash of the pack is stable across builds.
fn embed_pack(packdir: &Path) {
    let out = PathBuf::from(std::env::var("OUT_DIR").unwrap());
    let mut files: Vec<String> = Vec::new();
    for entry in ["shell.janet", "seat.janet"] {
        assert!(packdir.join(entry).is_file(), "policy/{entry} missing");
        files.push(entry.to_string());
    }
    let mut libs = files_in(&packdir.join("lib").display().to_string(), "janet", &[]);
    libs.sort();
    for p in libs {
        files.push(format!("lib/{}", p.file_name().unwrap().to_string_lossy()));
    }
    let root = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let mut rs = String::from("pub(crate) const PACK: &[(&str, &[u8])] = &[\n");
    for f in &files {
        rs.push_str(&format!(
            "    ({f:?}, include_bytes!({:?})),\n",
            format!("{root}/policy/{f}")
        ));
    }
    rs.push_str("];\n");
    std::fs::write(out.join("pack.rs"), rs).unwrap();
}
