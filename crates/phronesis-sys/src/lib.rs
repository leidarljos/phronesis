//! phronesis, compiled from vendored C, with its Janet pack embedded.
//!
//! [`ffi`] is the C API as `include/phronesis/supervisor.h` declares it,
//! plus the two ShellCheck shims from `csrc/shell_encode.c`. [`Supervisor`]
//! wraps the calls a shell guard makes: open a seat, bind it to a
//! workspace, judge one argv. [`install_pack`] writes the embedded pack to
//! the cache in the layout phronesis trusts and names it through the
//! environment, once per pack content.

use std::ffi::{CStr, CString};
use std::io;
use std::path::{Path, PathBuf};

/// The phronesis version this crate carries.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

include!(concat!(env!("OUT_DIR"), "/pack.rs"));

/// The embedded pack: `(path under policy/, bytes)`, `shell.janet` and
/// `seat.janet` first, then `lib/*.janet` in name order.
#[must_use]
pub fn pack_files() -> &'static [(&'static str, &'static [u8])] {
    PACK
}

/// The C API.
pub mod ffi {
    use libc::{c_char, c_int, pid_t};

    /// An opaque seat.
    #[repr(C)]
    #[allow(non_camel_case_types)]
    pub struct phronesis_supervisor_t {
        _private: [u8; 0],
    }

    extern "C" {
        pub fn phronesis_version_string() -> *const c_char;
        pub fn phronesis_api_version() -> c_int;
        pub fn phronesis_supervisor_open(
            out: *mut *mut phronesis_supervisor_t,
            state_dir: *const c_char,
            runtime_dir: *const c_char,
        ) -> c_int;
        pub fn phronesis_supervisor_bind(
            s: *mut phronesis_supervisor_t,
            agent_id: *const c_char,
            mode: *const c_char,
            workspace: *const c_char,
            pid: pid_t,
        ) -> c_int;
        pub fn phronesis_supervisor_close(s: *mut phronesis_supervisor_t);
        /// Build a ShellCheck for `argv` in `cwd` and judge it; `out` is a
        /// malloc'd PolicyDecision message the caller frees.
        pub fn ljos_phronesis_check_shell(
            s: *mut phronesis_supervisor_t,
            cwd: *const c_char,
            argv: *const *const c_char,
            argc: c_int,
            out: *mut *mut u8,
            out_len: *mut usize,
        ) -> c_int;
        /// Read the decision, its reason code and the pack's text out of a
        /// PolicyDecision message.
        pub fn ljos_phronesis_read_decision(
            input: *const u8,
            in_len: usize,
            decision: *mut u16,
            code: *mut u16,
            reason: *mut c_char,
            reason_len: usize,
        ) -> c_int;
    }
}

/// The version string the linked library reports.
#[must_use]
pub fn linked_version() -> String {
    // SAFETY: the library returns a pointer to a static NUL-terminated string.
    unsafe { CStr::from_ptr(ffi::phronesis_version_string()) }
        .to_string_lossy()
        .into_owned()
}

/// A pack's outcome for one line.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Decision {
    Deny,
    Allow,
    Prompt,
}

/// What the pack said about one argv.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Verdict {
    pub decision: Decision,
    /// The PolicyReason code, as the schema numbers it.
    pub code: u16,
    /// The pack's own words, empty when it gave none.
    pub reason: String,
}

/// The entries the pack offers.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Entry {
    /// The default law, `shell.janet`.
    Shell,
    /// The default law plus the seat's package and Python rules,
    /// `seat.janet`.
    Seat,
}

impl Entry {
    #[must_use]
    pub fn file_name(self) -> &'static str {
        match self {
            Entry::Shell => "shell.janet",
            Entry::Seat => "seat.janet",
        }
    }
}

fn hash_pack() -> u64 {
    // FNV-1a over every file's name and bytes: the directory name changes
    // with the pack, so two crate versions never share one.
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    let mut eat = |bytes: &[u8]| {
        for b in bytes {
            h ^= u64::from(*b);
            h = h.wrapping_mul(0x0100_0000_01b3);
        }
    };
    for (name, bytes) in PACK {
        eat(name.as_bytes());
        eat(&[0]);
        eat(bytes);
        eat(&[0]);
    }
    h
}

fn cache_base() -> PathBuf {
    std::env::var_os("XDG_CACHE_HOME")
        .map(PathBuf::from)
        .filter(|p| p.is_absolute())
        .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".cache")))
        .unwrap_or_else(std::env::temp_dir)
        .join("phronesis-sys")
}

/// Write the embedded pack under `base` as `<hash>/share/phronesis/policy`,
/// the layout phronesis trusts, and return the prefix `<base>/<hash>`. A
/// prefix already holding the pack is left as it is.
///
/// # Errors
///
/// The directory cannot be made or a file cannot be written.
pub fn install_pack_in(base: &Path) -> io::Result<PathBuf> {
    let prefix = base.join(format!("{:016x}", hash_pack()));
    let policy = prefix.join("share/phronesis/policy");
    let done = prefix.join(".complete");
    if done.is_file() {
        return Ok(prefix);
    }
    std::fs::create_dir_all(policy.join("lib"))?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&prefix, std::fs::Permissions::from_mode(0o700))?;
    }
    for (name, bytes) in PACK {
        let path = policy.join(name);
        let tmp = path.with_extension(format!("tmp{}", std::process::id()));
        std::fs::write(&tmp, bytes)?;
        std::fs::rename(&tmp, &path)?;
    }
    std::fs::write(&done, b"")?;
    Ok(prefix)
}

/// [`install_pack_in`] under `$XDG_CACHE_HOME/phronesis-sys`, else
/// `~/.cache/phronesis-sys`.
///
/// # Errors
///
/// As [`install_pack_in`].
pub fn install_pack() -> io::Result<PathBuf> {
    install_pack_in(&cache_base())
}

/// Point phronesis at the embedded pack's `entry`, unless the process
/// already names a pack with `PHRONESIS_JANET_PACK`. Sets
/// `PHRONESIS_PREFIX`, `PHRONESIS_PACK_ROOT` and `PHRONESIS_JANET_PACK`.
///
/// # Errors
///
/// As [`install_pack`].
pub fn name_embedded_pack(entry: Entry) -> io::Result<PathBuf> {
    if let Some(named) = std::env::var_os("PHRONESIS_JANET_PACK") {
        return Ok(PathBuf::from(named));
    }
    let prefix = install_pack()?;
    let share = prefix.join("share/phronesis");
    let pack = share.join("policy").join(entry.file_name());
    std::env::set_var("PHRONESIS_PREFIX", &prefix);
    std::env::set_var("PHRONESIS_PACK_ROOT", &share);
    std::env::set_var("PHRONESIS_JANET_PACK", &pack);
    Ok(pack)
}

/// One open seat.
pub struct Supervisor {
    raw: *mut ffi::phronesis_supervisor_t,
}

// SAFETY: the seat is used from one thread at a time by its owner; the C
// library keeps no thread affinity.
unsafe impl Send for Supervisor {}

fn c_path(p: &Path) -> io::Result<CString> {
    CString::new(p.to_string_lossy().as_bytes())
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "path holds a NUL"))
}

impl Supervisor {
    /// Open a seat keeping its state under `state_dir` and its runtime
    /// records under `runtime_dir`.
    ///
    /// # Errors
    ///
    /// The library refused the directories.
    pub fn open(state_dir: &Path, runtime_dir: &Path) -> io::Result<Self> {
        let state = c_path(state_dir)?;
        let runtime = c_path(runtime_dir)?;
        let mut raw: *mut ffi::phronesis_supervisor_t = std::ptr::null_mut();
        // SAFETY: both strings are NUL-terminated and outlive the call.
        let rc =
            unsafe { ffi::phronesis_supervisor_open(&mut raw, state.as_ptr(), runtime.as_ptr()) };
        if rc != 0 || raw.is_null() {
            return Err(io::Error::other(format!("phronesis_supervisor_open: {rc}")));
        }
        Ok(Self { raw })
    }

    /// Bind the seat for `agent` (32 hex digits) in `mode` to `workspace`.
    /// A busy slot (`-4`) is tried again briefly; an already bound seat
    /// (`-2`) is not an error.
    ///
    /// # Errors
    ///
    /// Any other refusal.
    pub fn bind(&mut self, agent_hex: &str, mode: &str, workspace: &Path) -> io::Result<()> {
        let agent = CString::new(agent_hex)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "agent id holds a NUL"))?;
        let mode_c = CString::new(mode)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "mode holds a NUL"))?;
        let ws = c_path(workspace)?;
        let mut tries = 0u32;
        loop {
            // SAFETY: the seat is open and every string is NUL-terminated.
            let rc = unsafe {
                ffi::phronesis_supervisor_bind(
                    self.raw,
                    agent.as_ptr(),
                    mode_c.as_ptr(),
                    ws.as_ptr(),
                    0,
                )
            };
            match rc {
                0 | -2 => return Ok(()),
                -4 if tries < 20 => {
                    tries += 1;
                    std::thread::sleep(std::time::Duration::from_micros(
                        200 + u64::from(tries) * 100,
                    ));
                }
                other => {
                    return Err(io::Error::other(format!(
                        "phronesis_supervisor_bind: {other}"
                    )))
                }
            }
        }
    }

    /// Judge `argv` as run in `cwd`.
    ///
    /// # Errors
    ///
    /// The library gave no decision.
    pub fn check_shell(&mut self, cwd: &Path, argv: &[String]) -> io::Result<Verdict> {
        let cwd_c = c_path(cwd)?;
        let words: Vec<CString> = argv
            .iter()
            .map(|a| CString::new(a.as_str()).unwrap_or_default())
            .collect();
        let ptrs: Vec<*const libc::c_char> = words.iter().map(|c| c.as_ptr()).collect();
        let mut out: *mut u8 = std::ptr::null_mut();
        let mut out_len: usize = 0;
        let argc = libc::c_int::try_from(ptrs.len())
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "too many words"))?;
        // SAFETY: the seat is open; `ptrs` holds `argc` valid C strings; `out`
        // and `out_len` are valid for writes.
        let rc = unsafe {
            ffi::ljos_phronesis_check_shell(
                self.raw,
                cwd_c.as_ptr(),
                ptrs.as_ptr(),
                argc,
                &mut out,
                &mut out_len,
            )
        };
        if rc != 0 || out.is_null() || out_len == 0 {
            if !out.is_null() {
                // SAFETY: the shim malloc'd `out`.
                unsafe { libc::free(out.cast()) };
            }
            return Err(io::Error::other("phronesis gave no decision"));
        }
        let mut decision: u16 = 0;
        let mut code: u16 = 0;
        let mut reason = [0 as libc::c_char; 512];
        // SAFETY: `out` holds `out_len` bytes; `reason` is a buffer of its
        // stated length.
        let rrc = unsafe {
            ffi::ljos_phronesis_read_decision(
                out,
                out_len,
                &mut decision,
                &mut code,
                reason.as_mut_ptr(),
                reason.len(),
            )
        };
        // SAFETY: the shim malloc'd `out`, read once above.
        unsafe { libc::free(out.cast()) };
        if rrc != 0 {
            return Err(io::Error::other("phronesis decision did not parse"));
        }
        // SAFETY: the shim NUL-terminates `reason` within its length.
        let text = unsafe { CStr::from_ptr(reason.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        Ok(Verdict {
            decision: match decision {
                1 => Decision::Allow,
                2 => Decision::Prompt,
                _ => Decision::Deny,
            },
            code,
            reason: text,
        })
    }
}

impl Drop for Supervisor {
    fn drop(&mut self) {
        // SAFETY: `raw` came from a successful open and is closed once.
        unsafe { ffi::phronesis_supervisor_close(self.raw) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn seat(dir: &Path, entry: Entry) -> Supervisor {
        // The pack is named once per process; the tests share the entry.
        let prefix = install_pack_in(&dir.join("cache")).unwrap();
        let share = prefix.join("share/phronesis");
        std::env::set_var("PHRONESIS_PREFIX", &prefix);
        std::env::set_var("PHRONESIS_PACK_ROOT", &share);
        std::env::set_var(
            "PHRONESIS_JANET_PACK",
            share.join("policy").join(entry.file_name()),
        );
        let state = dir.join("state");
        let runtime = dir.join("runtime");
        std::fs::create_dir_all(&state).unwrap();
        std::fs::create_dir_all(&runtime).unwrap();
        let mut sup = Supervisor::open(&state, &runtime).unwrap();
        sup.bind("00000000000000000000000000000001", "seat", dir)
            .unwrap();
        sup
    }

    fn words(line: &str) -> Vec<String> {
        line.split_whitespace().map(str::to_string).collect()
    }

    #[test]
    fn the_linked_library_is_this_version() {
        assert_eq!(linked_version(), VERSION);
    }

    #[test]
    fn the_pack_is_written_once_in_the_trusted_layout() {
        let dir = tempfile::tempdir().unwrap();
        let prefix = install_pack_in(dir.path()).unwrap();
        assert!(prefix.join("share/phronesis/policy/shell.janet").is_file());
        assert!(prefix
            .join("share/phronesis/policy/lib/shell-danger.janet")
            .is_file());
        let again = install_pack_in(dir.path()).unwrap();
        assert_eq!(prefix, again);
        assert_eq!(
            std::fs::read(prefix.join("share/phronesis/policy/seat.janet")).unwrap(),
            pack_files()
                .iter()
                .find(|(n, _)| *n == "seat.janet")
                .unwrap()
                .1
        );
    }

    #[test]
    fn the_embedded_pack_judges_a_line() {
        let dir = tempfile::tempdir().unwrap();
        let mut sup = seat(dir.path(), Entry::Shell);
        let allowed = sup.check_shell(dir.path(), &words("ls -la")).unwrap();
        assert_eq!(allowed.decision, Decision::Allow, "{allowed:?}");
        let denied = sup.check_shell(dir.path(), &words("sudo id")).unwrap();
        assert_eq!(denied.decision, Decision::Deny, "{denied:?}");
        assert!(!denied.reason.is_empty(), "{denied:?}");
        let unstage = sup
            .check_shell(dir.path(), &words("git restore -S ."))
            .unwrap();
        assert_eq!(unstage.decision, Decision::Allow, "{unstage:?}");
        let discard = sup
            .check_shell(dir.path(), &words("git restore ."))
            .unwrap();
        assert_eq!(discard.decision, Decision::Deny, "{discard:?}");
    }
}
