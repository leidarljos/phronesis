/* SPDX-License-Identifier: Apache-2.0 */
#ifndef POLICYD_TEST_HARNESS_H
#define POLICYD_TEST_HARNESS_H

#include "phronesis/supervisor.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int t_tmpdir(char *buf, size_t n, const char *prefix);
int t_write_file(const char *path, const char *body);
int t_wait_file(const char *path, int timeout_ms);
int t_pid_alive(pid_t pid);
int t_read_pidfile(const char *path, pid_t *out);
void t_rm_rf(const char *path);

int t_open_pair(policyd_supervisor_t **out, char *state, size_t sn,
		char *runtime, size_t rn, const char *tag);

int run_paths_tests(void);
int run_lifecycle_tests(void);
int run_kill_tree_tests(void);
int run_action_log_tests(void);
int run_persist_tests(void);
int run_policy_tests(void);
int run_shell_pack_tests(void);
int run_pack_lib_tests(void);
int run_version_tests(void);
int run_capnp_ffi_tests(void);

#endif
