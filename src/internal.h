/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_INTERNAL_H
#define GROK_POLICYD_INTERNAL_H

#include "grok-policyd/supervisor.h"

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

int grok_policy_eval(const char *workspace,
		     const char *tool,
		     const char *action,
		     const char *path,
		     grok_policy_result_t *out);

/* Ensure parent dirs of a socket path exist (Cap'n serve bind). */
int grok_unix_ensure_socket_parent(const char *socket_path);

/*
 * CLI host backend (not linked into libgrok_policyd.so).
 * Portable surface: stop flag + optional service-manager notify.
 * Serve is nng-native; does not require sd_event.
 */
int grok_host_init(void);
void grok_host_fini(void);
int grok_host_should_stop(void);
void grok_host_request_stop(void);
void grok_host_notify_ready(void);
void grok_host_notify_stopping(void);
void grok_host_watchdog_ping(void);
int grok_host_drop_bounding_caps(void);
const char *grok_host_backend_name(void);

#endif
