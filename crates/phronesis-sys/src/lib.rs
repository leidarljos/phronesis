//! phronesis, compiled into the binary that links this crate.
//!
//! The law is the Janet policy pack and the interface is the Cap'n Proto
//! schema (`policy.capnp`, `util.capnp`): a check is a `ShellCheck`
//! message in, a `PolicyDecision` message out, built and read by
//! c-capnproto in `src/native/shell_check.c`. Nothing here decides
//! anything; this crate only carries phronesis so a seat binary needs no
//! meson build and no install prefix.
//!
//! The pack is embedded and written out once per pack version under the
//! cache directory, in the layout the loader trusts
//! (`<prefix>/share/phronesis/policy/`), and `PHRONESIS_PREFIX` is pointed
//! at it before the first check.

use std::ffi::{c_char, c_int, CString};
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

/// Opaque supervisor handle.
#[repr(C)]
pub struct Supervisor {
    _private: [u8; 0],
}

extern "C" {
    pub fn phronesis_version_string() -> *const c_char;
    pub fn phronesis_api_version() -> c_int;
    pub fn phronesis_supervisor_open(
        out: *mut *mut Supervisor,
        state_dir: *const c_char,
        runtime_dir: *const c_char,
    ) -> c_int;
    pub fn phronesis_supervisor_close(s: *mut Supervisor);
    pub fn phronesis_supervisor_bind(
        s: *mut Supervisor,
        agent_hex: *const c_char,
        mode: *const c_char,
        workspace: *const c_char,
        flags: c_int,
    ) -> c_int;
    /// Builds a `ShellCheck` from `argv` and `cwd`, runs the pack, and
    /// returns the packed `PolicyDecision` in `out` (malloc'd; free it).
    pub fn ljos_phronesis_check_shell(
        sup: *mut Supervisor,
        cwd: *const c_char,
        argv: *const *const c_char,
        argc: c_int,
        out: *mut *mut u8,
        out_len: *mut usize,
    ) -> c_int;
    /// Reads `decision` and `code` out of a packed `PolicyDecision`.
    pub fn ljos_phronesis_read_decision(
        input: *const u8,
        len: usize,
        decision: *mut u16,
        code: *mut u16,
    ) -> c_int;
}

/// The schema pin the embedded pack and generated C were made from.
pub const SCHEMA_PIN: &str = include_str!("../vendor/pack/SCHEMA_PIN");

/// The pack files, by their path under `share/phronesis/`.
const PACK: &[(&str, &str)] = &[
    ("SCHEMA_PIN", include_str!("../vendor/pack/SCHEMA_PIN")),
    ("policy/shell.janet", include_str!("../vendor/pack/policy/shell.janet")),
    (
        "policy/shell_view_layout.janet",
        include_str!("../vendor/pack/policy/shell_view_layout.janet"),
    ),
    ("policy/lib/layout.janet", include_str!("../vendor/pack/policy/lib/layout.janet")),
    (
        "policy/lib/python-law.janet",
        include_str!("../vendor/pack/policy/lib/python-law.janet"),
    ),
    (
        "policy/lib/shell-danger.janet",
        include_str!("../vendor/pack/policy/lib/shell-danger.janet"),
    ),
    (
        "policy/lib/shell-secret.janet",
        include_str!("../vendor/pack/policy/lib/shell-secret.janet"),
    ),
    ("policy/lib/voice-law.janet", include_str!("../vendor/pack/policy/lib/voice-law.janet")),
];

/// A short, stable name for this pack's content, so a new pack lands in a
/// new directory and an old binary keeps its own.
fn pack_hash() -> String {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for (name, text) in PACK {
        for b in name.bytes().chain([0]).chain(text.bytes()) {
            h ^= u64::from(b);
            h = h.wrapping_mul(0x0100_0000_01b3);
        }
    }
    format!("{h:016x}")
}

/// The version string phronesis reports.
#[must_use]
pub fn version() -> String {
    // SAFETY: the C side returns a static NUL-terminated string.
    unsafe { std::ffi::CStr::from_ptr(phronesis_version_string()) }
        .to_string_lossy()
        .into_owned()
}

/// Where the embedded pack lives, written on first call: a prefix whose
/// `share/phronesis/` holds the pack. `PHRONESIS_PREFIX` set by the caller
/// wins and nothing is written.
///
/// # Errors
///
/// The cache directory cannot be created or written.
pub fn pack_prefix() -> std::io::Result<PathBuf> {
    static PREFIX: OnceLock<std::io::Result<PathBuf>> = OnceLock::new();
    PREFIX
        .get_or_init(|| {
            if let Some(p) = std::env::var_os("PHRONESIS_PREFIX") {
                let p = PathBuf::from(p);
                if p.join("share/phronesis/policy/shell.janet").is_file() {
                    return Ok(p);
                }
            }
            let base = std::env::var_os("XDG_CACHE_HOME")
                .filter(|v| !v.is_empty())
                .map(PathBuf::from)
                .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".cache")))
                .ok_or_else(|| std::io::Error::other("no HOME"))?;
            let prefix = base.join("phronesis").join(pack_hash());
            write_pack(&prefix)?;
            Ok(prefix)
        })
        .as_ref()
        .map(Clone::clone)
        .map_err(|e| std::io::Error::new(e.kind(), e.to_string()))
}

fn write_pack(prefix: &Path) -> std::io::Result<()> {
    let root = prefix.join("share/phronesis");
    if PACK
        .iter()
        .all(|(name, text)| std::fs::read_to_string(root.join(name)).is_ok_and(|t| t == *text))
    {
        return Ok(());
    }
    for (name, text) in PACK {
        let path = root.join(name);
        if let Some(dir) = path.parent() {
            std::fs::create_dir_all(dir)?;
        }
        let tmp = path.with_extension("tmp");
        std::fs::write(&tmp, text)?;
        std::fs::rename(&tmp, &path)?;
    }
    Ok(())
}

/// Point the loader at the embedded pack, unless the caller already
/// chose a pack through `PHRONESIS_JANET_PACK`.
///
/// # Errors
///
/// As [`pack_prefix`].
pub fn ensure_pack_env() -> std::io::Result<PathBuf> {
    let prefix = pack_prefix()?;
    // Set before the first check, from the thread that opens the
    // supervisor; the loader reads these on each check.
    std::env::set_var("PHRONESIS_PREFIX", &prefix);
    std::env::set_var("PHRONESIS_PACK_ROOT", prefix.join("share/phronesis"));
    if std::env::var_os("PHRONESIS_JANET_PACK").is_none() {
        std::env::set_var(
            "PHRONESIS_JANET_PACK",
            prefix.join("share/phronesis/policy/shell.janet"),
        );
    }
    Ok(prefix)
}

/// The two numbers a `PolicyDecision` carries: `Decision` and
/// `PolicyReason`, as the schema's enum ordinals.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Verdict {
    pub decision: u16,
    pub code: u16,
}

impl Verdict {
    /// `Decision.allow` in the schema.
    pub const ALLOW: u16 = 1;
    /// `Decision.deny`.
    pub const DENY: u16 = 0;
    /// `Decision.prompt`.
    pub const PROMPT: u16 = 2;
}

/// One shell check: open a supervisor on `state_dir` and `runtime_dir`,
/// bind the host seat to `workspace`, send the `ShellCheck`, read the
/// `PolicyDecision`. `None` when phronesis refused to answer (a supervisor
/// that would not open, a pack that failed to load), which a caller
/// treats as deny.
#[must_use]
pub fn check_shell(
    state_dir: &Path,
    runtime_dir: &Path,
    workspace: &str,
    cwd: &str,
    argv: &[String],
) -> Option<Verdict> {
    ensure_pack_env().ok()?;
    let c = |s: &str| CString::new(s).ok();
    let (state, runtime) = (c(state_dir.to_str()?)?, c(runtime_dir.to_str()?)?);
    let mut sup: *mut Supervisor = std::ptr::null_mut();
    // SAFETY: valid NUL-terminated paths; `sup` is checked before use and
    // closed on every path out.
    let rc = unsafe { phronesis_supervisor_open(&mut sup, state.as_ptr(), runtime.as_ptr()) };
    if rc != 0 || sup.is_null() {
        return None;
    }
    let result = (|| {
        let hex = c(&format!("{:016x}{:016x}", 0u64, 1u64))?;
        let mode = c("seat")?;
        let ws = c(workspace)?;
        // SAFETY: `sup` is open; the strings outlive the call.
        let brc = unsafe { phronesis_supervisor_bind(sup, hex.as_ptr(), mode.as_ptr(), ws.as_ptr(), 0) };
        // -2 is PHRONESIS_ERR_EXISTS: the seat is already bound.
        if brc != 0 && brc != -2 {
            return None;
        }
        let args: Vec<CString> = argv.iter().map(|a| c(a).unwrap_or_default()).collect();
        let ptrs: Vec<*const c_char> = args.iter().map(|a| a.as_ptr()).collect();
        let cwd_c = c(cwd)?;
        let mut out: *mut u8 = std::ptr::null_mut();
        let mut out_len: usize = 0;
        // SAFETY: `ptrs` holds `args.len()` valid pointers; `out` is freed
        // below after it is read.
        let crc = unsafe {
            ljos_phronesis_check_shell(
                sup,
                cwd_c.as_ptr(),
                ptrs.as_ptr(),
                c_int::try_from(ptrs.len()).ok()?,
                &mut out,
                &mut out_len,
            )
        };
        if crc != 0 || out.is_null() || out_len == 0 {
            return None;
        }
        let mut decision: u16 = 0;
        let mut code: u16 = 0;
        // SAFETY: `out` is a malloc'd buffer of `out_len` bytes from the C side.
        let rrc = unsafe { ljos_phronesis_read_decision(out, out_len, &mut decision, &mut code) };
        // SAFETY: `out` came from malloc in shell_check.c and is not used again.
        unsafe { libc_free(out.cast()) };
        (rrc == 0).then_some(Verdict { decision, code })
    })();
    // SAFETY: `sup` was opened above and is used by nothing after this.
    unsafe { phronesis_supervisor_close(sup) };
    result
}

extern "C" {
    #[link_name = "free"]
    fn libc_free(p: *mut std::ffi::c_void);
}
