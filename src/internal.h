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

/* Create agent cgroup under runtime if the host allows; path empty if not. */
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


/* Unix primitives (suckless: OS calls, no host-hostile chmod on shared parents). */
#include <sys/types.h>
int grok_unix_mkdir_leaf(const char *path, mode_t mode);
int grok_unix_ensure_socket_parent(const char *socket_path);

/* host_platform: libuv + libsystemd + libcap */
int grok_host_init(void);
void grok_host_fini(void);
int grok_host_should_stop(void);
void grok_host_notify_ready(void);
void grok_host_notify_stopping(void);
int grok_host_drop_bounding_caps(void);
int grok_unix_stream_listen(const char *socket_path, mode_t sock_mode, int *listen_fd);
int grok_unix_peer_uid(int fd, uid_t *uid_out);
int grok_unix_peer_is_self(int fd);

#endif
