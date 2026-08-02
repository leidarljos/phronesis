/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file supervisor.h
 * @brief Stable C ABI for the grok-policyd multi-agent supervisor.
 *
 * @par Source of truth
 * This header is hand-maintained C. Unlike rgpot / featomic / metatensor
 * (Rust core + cbindgen), policyd is implemented in C; the public API is
 * this file, not a generated binding. Document with Doxygen; Sphinx
 * (breathe) renders the reference.
 *
 * @par Stability
 * - Opaque handle @ref grok_supervisor_t may change layout freely.
 * - Public structs/enums and function signatures are ABI-stable within a
 *   @ref GROK_POLICYD_API_VERSION generation. Additive symbols are allowed;
 *   renames/removals/layout changes require an API version bump.
 * - Fixed-size char buffers in public structs are part of the ABI.
 * - ELF SONAME uses **package major** (``libgrok_policyd.so.0`` while major is
 *   0), not API_VERSION. Embedders key on @ref GROK_POLICYD_API_VERSION for
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
 * @ref grok_policy_check is CLI string bridge only.
 */

#ifndef GROK_POLICYD_SUPERVISOR_H
#define GROK_POLICYD_SUPERVISOR_H

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
#define GROK_POLICYD_VERSION "0.1.0"
/** Major package version component. */
#define GROK_POLICYD_VERSION_MAJOR 0
/** Minor package version component. */
#define GROK_POLICYD_VERSION_MINOR 1
/** Patch package version component. */
#define GROK_POLICYD_VERSION_PATCH 0

/**
 * Link-compatible API generation.
 *
 * Bump when removing symbols, changing layouts of public structs, or
 * changing semantics of existing return codes in a breaking way.
 * Additive APIs keep the same value.
 */
#define GROK_POLICYD_API_VERSION 2

/**
 * ELF visibility for the public ABI. Internal helpers stay hidden when the
 * shared library is built with default-hidden visibility (see meson.build).
 */
#if defined(__GNUC__) || defined(__clang__)
#define GROK_POLICYD_API __attribute__((visibility("default")))
#else
#define GROK_POLICYD_API
#endif

/**
 * @return Runtime package version string (matches @ref GROK_POLICYD_VERSION).
 */
GROK_POLICYD_API const char *grok_policyd_version_string(void);

/**
 * @return Runtime API generation (matches @ref GROK_POLICYD_API_VERSION).
 */
GROK_POLICYD_API int grok_policyd_api_version(void);

/** @} */

/**
 * @defgroup status Status codes
 * @brief Integer return codes for all supervisor entry points.
 * @{
 */

/** Success. */
#define GROK_OK            0
/** Invalid argument (null, empty, bad id, buffer size, path form). */
#define GROK_ERR_INVAL    (-1)
/** Agent id already registered / running. */
#define GROK_ERR_EXISTS   (-2)
/** Agent id not found. */
#define GROK_ERR_NOTFOUND (-3)
/** Filesystem or state I/O failure. */
#define GROK_ERR_IO       (-4)
/** Process spawn failure. */
#define GROK_ERR_SPAWN    (-5)
/** Supervisor or agent in wrong lifecycle state. */
#define GROK_ERR_STATE    (-6)
/** Policy denied (reserved for callers that treat deny as error). */
#define GROK_ERR_DENIED   (-7)

/** @} */

/**
 * @defgroup limits Buffer limits
 * @brief Fixed sizes that form part of the public ABI.
 * @{
 */

/** Max agent id length including trailing NUL. */
#define GROK_ID_MAX       64
/** Max mode string length including trailing NUL. */
#define GROK_MODE_MAX     32
/** Max path length including trailing NUL. */
#define GROK_PATH_MAX     512
/** Max log detail length including trailing NUL. */
#define GROK_DETAIL_MAX   512
/** Max tool name length including trailing NUL. */
#define GROK_TOOL_MAX     64
/** Max action name length including trailing NUL. */
#define GROK_ACTION_MAX   64
/** Max policy reason length including trailing NUL. */
#define GROK_REASON_MAX   128

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
	GROK_AGENT_STOPPED = 0,
	/** Child process group is running. */
	GROK_AGENT_RUNNING = 1,
	/** Process exited with non-zero status or was kill-failed. */
	GROK_AGENT_FAILED = 2
} grok_agent_state_t;

/**
 * Outcome of a policy check.
 *
 * Tools are default-deny. High-risk actions may return #GROK_DECISION_PROMPT.
 * Workspace-rooted path ops may return #GROK_DECISION_ALLOW (lexical only).
 */
typedef enum {
	GROK_DECISION_DENY = 0,
	GROK_DECISION_ALLOW = 1,
	GROK_DECISION_PROMPT = 2
} grok_decision_t;

/**
 * Snapshot of one agent slot.
 *
 * All char fields are NUL-terminated. @a has_cgroup is 1 when stop used a
 * cgroup v2 kill path; 0 means process-group only.
 */
typedef struct {
	char id[GROK_ID_MAX];
	grok_agent_state_t state;
	pid_t pid;
	pid_t pgid;
	int exit_status;
	char mode[GROK_MODE_MAX];
	char workspace[GROK_PATH_MAX];
	/** 1 if agent was placed in a cgroup for stop; 0 = process-group only. */
	int has_cgroup;
} grok_agent_status_t;

/**
 * Result of @ref grok_policy_check.
 */
typedef struct {
	grok_decision_t decision;
	char reason[GROK_REASON_MAX];
} grok_policy_result_t;

/**
 * Opaque supervisor handle.
 *
 * Layout is private. Obtain with @ref grok_supervisor_open and release with
 * @ref grok_supervisor_close.
 */
typedef struct grok_supervisor grok_supervisor_t;

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
 * @return @ref GROK_OK or a negative @ref status code.
 *
 * @note Paths are created as needed. Concurrent opens of the same dirs from
 *       multiple processes are not coordinated (single-host TCB assumption).
 */
GROK_POLICYD_API int grok_supervisor_open(grok_supervisor_t **out,
					  const char *state_dir,
					  const char *runtime_dir);

/**
 * Close @a s and free all resources. Safe with NULL.
 */
GROK_POLICYD_API void grok_supervisor_close(grok_supervisor_t *s);

/**
 * @return Absolute path of the JSONL action log, or empty string if unset.
 *         Valid until @ref grok_supervisor_close.
 */
GROK_POLICYD_API const char *grok_supervisor_action_log_path(const grok_supervisor_t *s);

/**
 * @return Resolved state directory. Valid until close.
 */
GROK_POLICYD_API const char *grok_supervisor_state_dir(const grok_supervisor_t *s);

/**
 * @return Resolved runtime directory. Valid until close.
 */
GROK_POLICYD_API const char *grok_supervisor_runtime_dir(const grok_supervisor_t *s);

/** @} */

/**
 * @defgroup agents Agent control
 * @{
 */

/**
 * Start @a argv as a process-group leader under @a agent_id.
 *
 * @param s           Open supervisor.
 * @param agent_id    `[A-Za-z0-9_-]+`, length < @ref GROK_ID_MAX.
 * @param mode        Non-empty mode token (e.g. `"agent"`).
 * @param workspace   Absolute workspace root for policy path checks, or NULL.
 * @param argv        NULL-terminated argv; argv[0] is the executable.
 * @return @ref GROK_OK, @ref GROK_ERR_EXISTS, @ref GROK_ERR_SPAWN, etc.
 */
GROK_POLICYD_API int grok_supervisor_start(grok_supervisor_t *s,
					   const char *agent_id,
					   const char *mode,
					   const char *workspace,
					   char *const argv[]);

/**
 * Fill @a out with the current status of @a agent_id (reaps zombies).
 *
 * @return @ref GROK_OK or @ref GROK_ERR_NOTFOUND / @ref GROK_ERR_INVAL.
 */
GROK_POLICYD_API int grok_supervisor_status(grok_supervisor_t *s,
					    const char *agent_id,
					    grok_agent_status_t *out);

/**
 * Stop @a agent_id: SIGTERM→SIGKILL on the process group; best-effort
 * `cgroup.kill` when a writable cgroup v2 child was created at start.
 *
 * @return @ref GROK_OK or @ref GROK_ERR_NOTFOUND / @ref GROK_ERR_STATE.
 */
GROK_POLICYD_API int grok_supervisor_stop(grok_supervisor_t *s,
					  const char *agent_id);

/**
 * Append a structured line to the action log for @a agent_id.
 */
GROK_POLICYD_API int grok_supervisor_log(grok_supervisor_t *s,
					 const char *agent_id,
					 const char *kind,
					 const char *detail);

/**
 * Copy the last action-log line into @a buf (NUL-terminated, truncated).
 *
 * @return @ref GROK_OK or @ref GROK_ERR_IO / @ref GROK_ERR_INVAL.
 */
GROK_POLICYD_API int grok_supervisor_log_last(const grok_supervisor_t *s,
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
 *
 * @param s         Open supervisor (used for agent workspace lookup).
 * @param agent_id  Agent whose workspace roots the check.
 * @param tool      Tool name (e.g. `"fs"`, `"shell"`).
 * @param action    Action name (e.g. `"read"`, `"exec"`).
 * @param path      Optional absolute path for path-scoped tools; may be NULL.
 * @param out       Receives decision + reason; must not be NULL.
 * @return @ref GROK_OK on a completed evaluation (including deny/prompt).
 *         Negative codes only for invalid inputs / missing agent.
 */
GROK_POLICYD_API int grok_policy_check(grok_supervisor_t *s,
				       const char *agent_id,
				       const char *tool,
				       const char *action,
				       const char *path,
				       grok_policy_result_t *out);

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
#define GROK_POLICY_CAPNP_MAX_BODY (64 * 1024)

/** status() → out root PolicydStatus. */
GROK_POLICYD_API void grok_policyd_status(grok_supervisor_t *sup,
					  uint8_t **out,
					  size_t *out_len);

/** checkSeat → in SeatCheck, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_check_seat(grok_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** checkModel → in ModelCheck, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_check_model(grok_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** checkPath → in PathCheck, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_check_path(grok_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** checkShell → in ShellCheck, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_check_shell(grok_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** checkRisk → in RiskCheck, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_check_risk(grok_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** admitSeat → in AdmitSeat, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_admit_seat(grok_supervisor_t *sup,
					      const uint8_t *in,
					      size_t in_len,
					      uint8_t **out,
					      size_t *out_len);

/** admitModel → in AdmitModel, out PolicyDecision. */
GROK_POLICYD_API void grok_policyd_admit_model(grok_supervisor_t *sup,
					       const uint8_t *in,
					       size_t in_len,
					       uint8_t **out,
					       size_t *out_len);

/** agentStatus → in AgentQuery, out AgentStatus. */
GROK_POLICYD_API void grok_policyd_agent_status(grok_supervisor_t *sup,
						const uint8_t *in,
						size_t in_len,
						uint8_t **out,
						size_t *out_len);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GROK_POLICYD_SUPERVISOR_H */
