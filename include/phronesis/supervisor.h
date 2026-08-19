/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file supervisor.h
 * @brief Stable C ABI for the phronesis multi-agent supervisor.
 *
 * @par Source of truth
 * This header is hand-maintained C. Unlike rgpot / featomic / metatensor
 * (Rust core + cbindgen), phronesis is implemented in C; the public API is
 * this file, not a generated binding. Document with Doxygen; Sphinx
 * (breathe) renders the reference.
 *
 * @par Stability
 * - Opaque handle @ref phronesis_supervisor_t may change layout freely.
 * - Public structs/enums and function signatures are ABI-stable within a
 *   @ref PHRONESIS_API_VERSION generation. Additive symbols are allowed;
 *   renames/removals/layout changes require an API version bump.
 * - Fixed-size char buffers in public structs are part of the ABI.
 * - ELF SONAME uses **package major** (``libphronesis.so.0`` while major is
 *   0), not API_VERSION. Embedders key on @ref PHRONESIS_API_VERSION for
 *   link-compat; SONAME is the distro package major.
 * - Single source: repo ``VERSION`` + ``API_VERSION`` files
 *   (``scripts/sync-version.sh`` / ``scripts/check-version.sh``).
 *
 * @par Cap'n Proto (product language)
 * Cap'n is **always** linked. Product API is ``interface Policyd``: one C
 * entry point per method. Params message root in, result message root out
 * (zero-copy mappable Cap'n segments). No CallEnvelope, no ok|err unions.
 * Every check/admit method returns Cap'n ``PolicyDecision`` (deny/allow/prompt);
 * protocol failure is fail-closed deny. Lifecycle open/start/stop stay C.
 * @ref phronesis_policy_check is CLI string bridge only.
 */

#ifndef PHRONESIS_SUPERVISOR_H
#define PHRONESIS_SUPERVISOR_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup version Version
 * @brief Package and ABI version macros / queries.
 * @{
 */

/** Semantic version string (major.minor.patch). */
#define PHRONESIS_VERSION "0.1.0"
/** Major package version component. */
#define PHRONESIS_VERSION_MAJOR 0
/** Minor package version component. */
#define PHRONESIS_VERSION_MINOR 1
/** Patch package version component. */
#define PHRONESIS_VERSION_PATCH 0

/**
 * Link-compatible API generation.
 *
 * Bump when removing symbols, changing layouts of public structs, or
 * changing semantics of existing return codes in a breaking way.
 * Additive APIs keep the same value.
 */
#define PHRONESIS_API_VERSION 3

/**
 * ELF visibility for the public ABI. Internal helpers stay hidden when the
 * shared library is built with default-hidden visibility (see meson.build).
 */
#if defined(__GNUC__) || defined(__clang__)
#define PHRONESIS_API __attribute__((visibility("default")))
#else
#define PHRONESIS_API
#endif

/**
 * @return Runtime package version string (matches @ref PHRONESIS_VERSION).
 */
PHRONESIS_API const char *phronesis_version_string(void);

/**
 * @return Runtime API generation (matches @ref PHRONESIS_API_VERSION).
 */
PHRONESIS_API int phronesis_api_version(void);

/** @} */

/**
 * @defgroup status Status codes
 * @brief Integer return codes for all supervisor entry points.
 * @{
 */

/** Success. */
#define PHRONESIS_OK            0
/** Invalid argument (null, empty, bad id, buffer size, path form). */
#define PHRONESIS_ERR_INVAL    (-1)
/** Agent id already registered / running. */
#define PHRONESIS_ERR_EXISTS   (-2)
/** Agent id not found. */
#define PHRONESIS_ERR_NOTFOUND (-3)
/** Filesystem or state I/O failure. */
#define PHRONESIS_ERR_IO       (-4)
/** Process spawn failure. */
#define PHRONESIS_ERR_SPAWN    (-5)
/** Supervisor or agent in wrong lifecycle state. */
#define PHRONESIS_ERR_STATE    (-6)
/** Policy denied (reserved for callers that treat deny as error). */
#define PHRONESIS_ERR_DENIED   (-7)

/** @} */

/**
 * @defgroup limits Buffer limits
 * @brief Fixed sizes that form part of the public ABI.
 * @{
 */

/** Max agent id length including trailing NUL. */
#define PHRONESIS_ID_MAX       64
/** Max mode string length including trailing NUL. */
#define PHRONESIS_MODE_MAX     32
/** Max path length including trailing NUL. */
#define PHRONESIS_PATH_MAX     512
/** Max log detail length including trailing NUL. */
#define PHRONESIS_DETAIL_MAX   512
/** Max tool name length including trailing NUL. */
#define PHRONESIS_TOOL_MAX     64
/** Max action name length including trailing NUL. */
#define PHRONESIS_ACTION_MAX   64
/** Max policy reason length including trailing NUL. */
#define PHRONESIS_REASON_MAX   128

/** @} */

/**
 * @defgroup types Public types
 * @{
 */

/**
 * Lifecycle state of a supervised agent process.
 */
typedef enum {
	/** No live process (never started, or reaped stopped). */
	PHRONESIS_AGENT_STOPPED = 0,
	/** Child process group is running. */
	PHRONESIS_AGENT_RUNNING = 1,
	/** Process exited with non-zero status or was kill-failed. */
	PHRONESIS_AGENT_FAILED = 2
} phronesis_agent_state_t;

/**
 * Outcome of a policy check.
 *
 * Tools are default-deny. High-risk actions may return #PHRONESIS_DECISION_PROMPT.
 * Workspace-rooted path ops may return #PHRONESIS_DECISION_ALLOW (lexical only).
 */
typedef enum {
	PHRONESIS_DECISION_DENY = 0,
	PHRONESIS_DECISION_ALLOW = 1,
	PHRONESIS_DECISION_PROMPT = 2
} phronesis_decision_t;

/**
 * Machine codes for @ref phronesis_policy_result_t — mirrors Cap'n PolicyReason
 * in policy.capnp (grokos-schema). Keep ordinals identical.
 */
typedef enum {
	PHRONESIS_REASON_UNSPECIFIED = 0,
	PHRONESIS_REASON_TOOLS_DEFAULT_DENY = 1,
	PHRONESIS_REASON_PATH_OUTSIDE_WORKSPACE = 2,
	PHRONESIS_REASON_PATH_UNDER_WORKSPACE_ALLOW = 3,
	PHRONESIS_REASON_INVALID_MESSAGE = 4,
	PHRONESIS_REASON_FIELD_TOO_LONG = 5,
	PHRONESIS_REASON_DENY_ALL = 6,
	PHRONESIS_REASON_HIGH_RISK_PROMPT = 7,
	PHRONESIS_REASON_SEAT_BOARD_ALLOW = 8,
	PHRONESIS_REASON_MODEL_START_ALLOW = 9,
	PHRONESIS_REASON_MISSING_TOOL_ACTION = 10,
	PHRONESIS_REASON_UNKNOWN_SEAT_ACTION = 11,
	PHRONESIS_REASON_PACK_MISSING = 12,
	PHRONESIS_REASON_PACK_LOAD_FAILED = 13,
	PHRONESIS_REASON_PACK_RUNTIME_ERROR = 14,
	PHRONESIS_REASON_PACK_BAD_RESULT = 15,
	PHRONESIS_REASON_SHELL_VIEW_BUILD_FAILED = 16,
	PHRONESIS_REASON_PYTHON_REQUIRES_UV_RUN = 17,
	PHRONESIS_REASON_PYTHON_DASH_C_DENIED = 18,
	PHRONESIS_REASON_PYTHON_MISSING_PEP723 = 19,
	PHRONESIS_REASON_SHELL_EXEC_ALLOW = 20,
	PHRONESIS_REASON_PACK_RELOADED = 21,
	PHRONESIS_REASON_PACK_PATH_INVALID = 22,
	/* Shell content pack danger (ordinals match Cap'n PolicyReason / schema 0.3.3) */
	PHRONESIS_REASON_SHELL_DANGEROUS_RUNNER = 23,
	PHRONESIS_REASON_SHELL_REMOTE_EXEC = 24,
	PHRONESIS_REASON_SHELL_PRIVILEGE_DENIED = 25,
	PHRONESIS_REASON_SHELL_GIT_DANGEROUS = 26,
	/* Secret material in argv / sensitive path / export */
	PHRONESIS_REASON_SHELL_SECRET_IN_ARGV = 27,
	PHRONESIS_REASON_PATH_SENSITIVE_DENY = 28,
	PHRONESIS_REASON_SECRET_EXPORT_DENIED = 29,
	/* checkAudio (meta #97 Track E) — ordinals match Cap'n PolicyReason */
	PHRONESIS_REASON_AUDIO_MIC_OPEN_DENY = 30,
	PHRONESIS_REASON_AUDIO_LISTEN_ARM_PROMPT = 31,
	PHRONESIS_REASON_AUDIO_ALWAYS_LISTEN_DENY = 32,
	PHRONESIS_REASON_AUDIO_NETWORK_STT_DENY = 33,
	PHRONESIS_REASON_AUDIO_INJECT_DENY = 34,
	PHRONESIS_REASON_AUDIO_FIXTURE_ALLOW = 35,
	PHRONESIS_REASON_AUDIO_UNKNOWN_ACTION = 36
} phronesis_policy_reason_t;

/**
 * Snapshot of one agent slot.
 *
 * All char fields are NUL-terminated. @a has_cgroup is 1 when stop used a
 * cgroup v2 kill path; 0 means process-group only.
 */
typedef struct {
	char id[PHRONESIS_ID_MAX];
	phronesis_agent_state_t state;
	pid_t pid;
	pid_t pgid;
	int exit_status;
	char mode[PHRONESIS_MODE_MAX];
	char workspace[PHRONESIS_PATH_MAX];
	/** 1 if agent was placed in a cgroup for stop; 0 = process-group only. */
	int has_cgroup;
} phronesis_agent_status_t;

/**
 * Internal/CLI bridge result. Product Cap'n path uses PolicyDecision on the wire
 * (decision + code); viewers map @a code to human text.
 */
typedef struct {
	phronesis_decision_t decision;
	phronesis_policy_reason_t code;
	/** Unused on Cap'n product path; CLI may leave empty. */
	char reason[PHRONESIS_REASON_MAX];
} phronesis_policy_result_t;

/** Set decision + PolicyReason code (reason left empty). */
PHRONESIS_API void phronesis_policy_result_set(phronesis_policy_result_t *out,
					     phronesis_decision_t decision,
					     phronesis_policy_reason_t code);

/**
 * Opaque supervisor handle.
 *
 * Layout is private. Obtain with @ref phronesis_supervisor_open and release with
 * @ref phronesis_supervisor_close.
 */
typedef struct phronesis_supervisor phronesis_supervisor_t;

/** @} */

/**
 * @defgroup lifecycle Supervisor lifecycle
 * @{
 */

/**
 * Open a supervisor bound to state and runtime directories.
 *
 * @param out           Receives the new handle on success; must not be NULL.
 * @param state_dir     Persistent state root, or NULL for env / XDG default.
 * @param runtime_dir   Runtime root, or NULL for env / XDG default.
 *                      Must not resolve under `/tmp`.
 * @return @ref PHRONESIS_OK or a negative @ref status code.
 *
 * @note Paths are created as needed. Concurrent opens of the same dirs from
 *       multiple processes are not coordinated (single-host TCB assumption).
 */
PHRONESIS_API int phronesis_supervisor_open(phronesis_supervisor_t **out,
					  const char *state_dir,
					  const char *runtime_dir);

/**
 * Close @a s and free all resources. Safe with NULL.
 */
PHRONESIS_API void phronesis_supervisor_close(phronesis_supervisor_t *s);

/**
 * @return Absolute path of the JSONL action log, or empty string if unset.
 *         Valid until @ref phronesis_supervisor_close.
 */
PHRONESIS_API const char *phronesis_supervisor_action_log_path(const phronesis_supervisor_t *s);

/**
 * @return Resolved state directory. Valid until close.
 */
PHRONESIS_API const char *phronesis_supervisor_state_dir(const phronesis_supervisor_t *s);

/**
 * @return Resolved runtime directory. Valid until close.
 */
PHRONESIS_API const char *phronesis_supervisor_runtime_dir(const phronesis_supervisor_t *s);

/** @} */

/**
 * @defgroup agents Agent control
 * @{
 */

/**
 * Start @a argv as a process-group leader under @a agent_id.
 *
 * @param s           Open supervisor.
 * @param agent_id    `[A-Za-z0-9_-]+`, length < @ref PHRONESIS_ID_MAX.
 * @param mode        Non-empty mode token (e.g. `"agent"`).
 * @param workspace   Absolute workspace root for policy path checks, or NULL.
 * @param argv        NULL-terminated argv; argv[0] is the executable.
 * @return @ref PHRONESIS_OK, @ref PHRONESIS_ERR_EXISTS, @ref PHRONESIS_ERR_SPAWN, etc.
 */
PHRONESIS_API int phronesis_supervisor_start(phronesis_supervisor_t *s,
					   const char *agent_id,
					   const char *mode,
					   const char *workspace,
					   char *const argv[]);

/**
 * Admit @a agent_id as a running slot without fork/exec.
 *
 * Used by sessiond after Cap'n admit: the agent process is launched on the
 * vat/proc plane, not by this supervisor. @a pid 0 leaves the slot running
 * with no process to reap or kill. A live @a pid is recorded and reaped
 * like @ref phronesis_supervisor_start.
 *
 * A running slot with an empty workspace may be filled by a later bind
 * (sessiond often admits first, the agent then bind-fills cwd). A
 * non-empty workspace is sticky and returns @ref PHRONESIS_ERR_EXISTS.
 * An empty incoming workspace on a later bind (including after stop)
 * leaves the committed root in place.
 *
 * @return @ref PHRONESIS_OK or @ref PHRONESIS_ERR_EXISTS / @ref PHRONESIS_ERR_INVAL.
 */
PHRONESIS_API int phronesis_supervisor_bind(phronesis_supervisor_t *s,
					  const char *agent_id,
					  const char *mode,
					  const char *workspace,
					  pid_t pid);

/**
 * Fill @a out with the current status of @a agent_id (reaps zombies).
 *
 * @return @ref PHRONESIS_OK or @ref PHRONESIS_ERR_NOTFOUND / @ref PHRONESIS_ERR_INVAL.
 */
PHRONESIS_API int phronesis_supervisor_status(phronesis_supervisor_t *s,
					    const char *agent_id,
					    phronesis_agent_status_t *out);

/**
 * Stop @a agent_id: SIGTERM→SIGKILL on the process group; best-effort
 * `cgroup.kill` when a writable cgroup v2 child was created at start.
 *
 * @return @ref PHRONESIS_OK or @ref PHRONESIS_ERR_NOTFOUND / @ref PHRONESIS_ERR_STATE.
 */
PHRONESIS_API int phronesis_supervisor_stop(phronesis_supervisor_t *s,
					  const char *agent_id);

/**
 * Append a structured line to the action log for @a agent_id.
 */
PHRONESIS_API int phronesis_supervisor_log(phronesis_supervisor_t *s,
					 const char *agent_id,
					 const char *kind,
					 const char *detail);

/**
 * Copy the last action-log line into @a buf (NUL-terminated, truncated).
 *
 * @return @ref PHRONESIS_OK or @ref PHRONESIS_ERR_IO / @ref PHRONESIS_ERR_INVAL.
 */
PHRONESIS_API int phronesis_supervisor_log_last(const phronesis_supervisor_t *s,
					      char *buf,
					      size_t buflen);

/** @} */

/**
 * @defgroup policy Policy checks
 * @{
 */

/**
 * Evaluate a tool/action/path against the fail-closed policy table.
 *
 * Tools default deny. High-risk actions → prompt. Path ops under the agent's
 * workspace root may allow (lexical allowlist: absolute paths only; rejects
 * `..` components; not realpath — symlink escape still open).
 * Seat and model tools require @a agent_id to name a running supervisor slot
 * (unset / unknown / non-running → deny).
 *
 * @param s         Open supervisor (used for agent workspace lookup).
 * @param agent_id  Agent whose workspace roots the check.
 * @param tool      Tool name (e.g. `"fs"`, `"shell"`).
 * @param action    Action name (e.g. `"read"`, `"exec"`).
 * @param path      Optional absolute path for path-scoped tools; may be NULL.
 * @param out       Receives decision + reason; must not be NULL.
 * @return @ref PHRONESIS_OK on a completed evaluation (including deny/prompt).
 *         Negative codes only for invalid inputs / missing agent.
 */
PHRONESIS_API int phronesis_policy_check(phronesis_supervisor_t *s,
				       const char *agent_id,
				       const char *tool,
				       const char *action,
				       const char *path,
				       phronesis_policy_result_t *out);

/** @} */

/**
 * @defgroup capnp Cap'n Policyd methods
 * @brief Cap'n params message in, Cap'n result message out (always).
 *
 * No Decision-in-int. Out root is always the method's result type
 * (PolicyDecision, PolicydStatus, or AgentStatus). On OOM *@a out may be NULL.
 * @{
 */

/** Max accepted Cap'n params / results body (bytes). */
#define PHRONESIS_POLICY_CAPNP_MAX_BODY (64 * 1024)

/** status() → out root PolicydStatus. */
PHRONESIS_API void phronesis_status(phronesis_supervisor_t *sup,
					  uint8_t **out,
					  size_t *out_len);

/** checkSeat → in SeatCheck, out PolicyDecision. */
PHRONESIS_API void phronesis_check_seat(phronesis_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** checkModel → in ModelCheck, out PolicyDecision. */
PHRONESIS_API void phronesis_check_model(phronesis_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** checkPath → in PathCheck, out PolicyDecision. */
PHRONESIS_API void phronesis_check_path(phronesis_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** checkShell → in ShellCheck, out PolicyDecision. */
PHRONESIS_API void phronesis_check_shell(phronesis_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** checkRisk → in RiskCheck, out PolicyDecision. */
PHRONESIS_API void phronesis_check_risk(phronesis_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/**
 * checkAudio → in AudioCheck, out PolicyDecision (meta #97 Track E).
 *
 * Product defaults (Janet pack @c audio-check / voice-law): micOpen /
 * alwaysListen / networkStt / inject deny; listenArm prompt; unknown deny.
 * Host TCB: truthy @c PHRONESIS_AUDIO_ALLOW allows all actions
 * (fixture/CI only; leave unset in production); @c PHRONESIS_DENY_ALL
 * still wins (same hard deny as every other Cap'n PolicyDecision entry).
 * No waveforms / PCM on the wire.
 */
PHRONESIS_API void phronesis_check_audio(phronesis_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** admitSeat → in AdmitSeat, out PolicyDecision. */
PHRONESIS_API void phronesis_admit_seat(phronesis_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** admitModel → in AdmitModel, out PolicyDecision. */
PHRONESIS_API void phronesis_admit_model(phronesis_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** agentStatus → in AgentQuery, out AgentStatus. */
PHRONESIS_API void phronesis_agent_status(phronesis_supervisor_t *sup,
						const uint8_t *in,
						size_t in_len,
						uint8_t **out,
						size_t *out_len);

/** reloadShellPack → in ReloadShellPack, out PolicyDecision. */
PHRONESIS_API void phronesis_reload_shell_pack(phronesis_supervisor_t *sup,
						     const uint8_t *in,
						     size_t in_len,
						     uint8_t **out,
						     size_t *out_len);

/**
 * Unload any loaded Janet packs and load @a path.
 *
 * @param path  Colon-separated list of absolute .janet pack files and/or
 *              absolute directories of top-level *.janet packs. Empty is
 *              invalid. Each file is opened under the pack root
 *              (install policy directory or @c PHRONESIS_PACK_ROOT;
 *              not "/"). Symlink steps in the pack tree fail the open.
 *              Each pack loads into
 *              its own sealed env; checkShell / checkAudio compose
 *              fail-closed across packs that define the entry
 *              (deny > prompt > allow).
 * @return @ref PHRONESIS_OK on successful load; @ref PHRONESIS_ERR_INVAL for bad path;
 *         @ref PHRONESIS_ERR_IO when a file cannot be loaded as a pack.
 *
 * Threading: not concurrent with checkShell. Product path is single-threaded
 * TCB per process (same as other policy methods).
 */
PHRONESIS_API int phronesis_policy_shell_pack_reload(const char *path);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PHRONESIS_SUPERVISOR_H */
