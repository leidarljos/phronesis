/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_TEST_HARNESS_H
#define GROK_POLICYD_TEST_HARNESS_H

#include "grok-policyd/supervisor.h"

#include <stddef.h>
#include <sys/types.h>

extern int g_failures;
extern int g_tests;

void t_expect(int cond, const char *msg);
void t_expect_eq(long a, long b, const char *msg);
void t_expect_streq(const char *a, const char *b, const char *msg);

int t_tmpdir(char *buf, size_t n, const char *prefix);
int t_write_file(const char *path, const char *body);
int t_wait_file(const char *path, int timeout_ms);
int t_pid_alive(pid_t pid);
int t_read_pidfile(const char *path, pid_t *out);
void t_rm_rf(const char *path);

/* Open supervisor on fresh state+runtime dirs under caller-owned roots. */
int t_open_pair(grok_supervisor_t **out, char *state, size_t sn,
		char *runtime, size_t rn, const char *tag);

typedef void (*t_fn)(void);
int t_run(const char *name, t_fn fn);

#endif
