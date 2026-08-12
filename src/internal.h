/* SPDX-License-Identifier: MIT */
#ifndef GROK_POLICYD_INTERNAL_H
#define GROK_POLICYD_INTERNAL_H

#include "phronesis/supervisor.h"

#include <stdint.h>

int grok_paths_resolve(char *state_dir, size_t state_len,
		       char *runtime_dir, size_t runtime_len,
		       char *action_log, size_t log_len,
		       const char *state_override,
		       const char *runtime_override);
int grok_paths_ensure_dir(const char *path, int mode);

int grok_action_log_append(const char *path,
			   const char *agent_id,
			   const char *kind,
			   const char *detail);
int grok_action_log_last(const char *path, char *buf, size_t buflen);

int grok_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len);
int grok_cgroup_attach(const char *cgroup_path, pid_t pid);
int grok_cgroup_kill(const char *cgroup_path);
void grok_cgroup_remove(const char *cgroup_path);

/** Off-wire slot key: 32 hex or empty for zero id. */
void grok_agent_id_to_hex(uint64_t hi, uint64_t lo, char out[GROK_ID_MAX]);

/** CLI: map admit kind string → tool/action (legacy bridge). */
int grok_policyd_map_admit_kind(const char *kind, const char **tool,
				const char **action);

/**
 * CLI/string bridge only. Product path is Cap'n CallEnvelope → CheckResults.
 * Maps legacy tool/action/path strings into domain checks.
 */
int grok_policy_eval(const char *workspace,
		     const char *tool,
		     const char *action,
		     const char *path,
		     grok_policy_result_t *out);

#include <capnp_c.h>

/** Resolve script path against cwd (workspace-bound callers only). */
int grok_policy_resolve_script(const char *cwd, const char *script, char *out,
			       size_t out_n);

#endif
