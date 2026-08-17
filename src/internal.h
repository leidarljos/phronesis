/* SPDX-License-Identifier: Apache-2.0 */
#ifndef POLICYD_INTERNAL_H
#define POLICYD_INTERNAL_H

#include "grok-policyd/supervisor.h"

#include <stdint.h>

int policyd_paths_resolve(char *state_dir, size_t state_len,
		       char *runtime_dir, size_t runtime_len,
		       char *action_log, size_t log_len,
		       const char *state_override,
		       const char *runtime_override);
int policyd_paths_ensure_dir(const char *path, int mode);

/** Drop loaded Janet packs so the next check re-reads default_pack_spec(). */
void policyd_policy_pack_reset(void);

int policyd_action_log_append(const char *path,
			   const char *agent_id,
			   const char *kind,
			   const char *detail);
int policyd_action_log_last(const char *path, char *buf, size_t buflen);

int policyd_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len);
int policyd_cgroup_attach(const char *cgroup_path, pid_t pid);
int policyd_cgroup_kill(const char *cgroup_path);
void policyd_cgroup_remove(const char *cgroup_path);

/** Off-wire slot key: 32 hex or empty for zero id. */
void policyd_agent_id_to_hex(uint64_t hi, uint64_t lo, char out[POLICYD_ID_MAX]);

/** CLI: map admit kind string → tool/action (legacy bridge). */
int policyd_map_admit_kind(const char *kind, const char **tool,
				const char **action);

/**
 * CLI/string bridge only. Product path is Cap'n CallEnvelope → CheckResults.
 * Maps legacy tool/action/path strings into domain checks.
 */
int policyd_policy_eval(const char *workspace,
		     const char *tool,
		     const char *action,
		     const char *path,
		     policyd_policy_result_t *out);

/** Truthy env for TCB gates: 1 / true / yes (any case of true/yes). */
int policyd_env_truthy(const char *name);

/** Truthy GROKOS_POLICYD_DENY_ALL — hard deny for Cap'n PolicyDecision entries. */
int policyd_policy_deny_all(void);

/**
 * Admit identity: @a agent_id must name a running supervisor slot.
 * Unset / empty id → INVALID_MESSAGE. Missing supervisor, unknown slot, or
 * non-running slot → TOOLS_DEFAULT_DENY. Writes the deny code into @a code.
 * @return 0 if the slot is running, -1 if the caller must deny.
 */
int policyd_policy_require_running_agent(policyd_supervisor_t *sup,
				      const char *agent_id,
				      policyd_policy_reason_t *code);

/** 1 if @a s has at least one POLICYD_AGENT_RUNNING slot (reaps first). */
int policyd_supervisor_has_running(policyd_supervisor_t *s);

#include <capnp_c.h>

/** Resolve script path against cwd (workspace-bound callers only). */
int policyd_policy_resolve_script(const char *cwd, const char *script, char *out,
			       size_t out_n);

/**
 * Build Cap'n ShellView bytes (malloc *flat_out).
 * Returns 0 on success, -2 if argc > 256 or an arg is >= POLICYD_PATH_MAX,
 * -1 on other failure. Callers free *flat_out.
 */
int policyd_policy_build_shell_view(const char *workspace, const char *cwd,
				 capn_ptr argv, uint8_t **flat_out,
				 size_t *flat_len);

#endif
