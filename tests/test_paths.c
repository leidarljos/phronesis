/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void test_open_null(void)
{
	t_expect_eq(grok_supervisor_open(NULL, "/x", "/y"), GROK_ERR_INVAL, "null out");
}

static void test_refuse_tmp_runtime(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX];

	t_expect(t_tmpdir(state, sizeof(state), "gp-tmp-st") == 0, "state dir");
	t_expect_eq(grok_supervisor_open(&s, state, "/tmp"), GROK_ERR_STATE, "/tmp");
	t_expect_eq(grok_supervisor_open(&s, state, "/tmp/"), GROK_ERR_STATE, "/tmp/");
	t_expect(s == NULL, "no handle");
	t_rm_rf(state);
}

static void test_creates_state_tree_and_modes(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char path[GROK_PATH_MAX];
	struct stat st;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "modes") == GROK_OK,
		 "open");
	t_expect(s != NULL, "handle");

	snprintf(path, sizeof(path), "%s/log", state);
	t_expect(stat(path, &st) == 0 && S_ISDIR(st.st_mode), "log dir");
	t_expect((st.st_mode & 0777) == 0700, "log mode 0700");

	snprintf(path, sizeof(path), "%s/policyd", state);
	t_expect(stat(path, &st) == 0 && S_ISDIR(st.st_mode), "policyd dir");

	snprintf(path, sizeof(path), "%s/agents", runtime);
	t_expect(stat(path, &st) == 0 && S_ISDIR(st.st_mode), "agents dir");
	t_expect((st.st_mode & 0777) == 0700, "agents mode 0700");

	t_expect(strstr(grok_supervisor_action_log_path(s), state) != NULL, "log under state");
	t_expect(strstr(grok_supervisor_action_log_path(s), "actions.jsonl") != NULL, "log name");
	t_expect_streq(grok_supervisor_state_dir(s), state, "state dir");
	t_expect_streq(grok_supervisor_runtime_dir(s), runtime, "runtime dir");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_env_action_log_override(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX], logpath[GROK_PATH_MAX];
	char *old;

	t_expect(t_tmpdir(state, sizeof(state), "gp-env-st") == 0, "state");
	t_expect(t_tmpdir(runtime, sizeof(runtime), "gp-env-rt") == 0, "runtime");
	snprintf(logpath, sizeof(logpath), "%s/custom.jsonl", state);

	old = getenv("GROKOS_ACTION_LOG");
	setenv("GROKOS_ACTION_LOG", logpath, 1);
	t_expect_eq(grok_supervisor_open(&s, state, runtime), GROK_OK, "open");
	t_expect_streq(grok_supervisor_action_log_path(s), logpath, "custom log");
	grok_supervisor_close(s);
	if (old)
		setenv("GROKOS_ACTION_LOG", old, 1);
	else
		unsetenv("GROKOS_ACTION_LOG");
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_env_state_runtime_override(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char *os, *orun;

	t_expect(t_tmpdir(state, sizeof(state), "gp-env2-st") == 0, "state");
	t_expect(t_tmpdir(runtime, sizeof(runtime), "gp-env2-rt") == 0, "runtime");

	os = getenv("GROKOS_STATE_DIR");
	orun = getenv("GROKOS_RUNTIME_DIR");
	setenv("GROKOS_STATE_DIR", state, 1);
	setenv("GROKOS_RUNTIME_DIR", runtime, 1);

	t_expect_eq(grok_supervisor_open(&s, NULL, NULL), GROK_OK, "open via env");
	t_expect_streq(grok_supervisor_state_dir(s), state, "env state");
	t_expect_streq(grok_supervisor_runtime_dir(s), runtime, "env runtime");
	grok_supervisor_close(s);

	if (os)
		setenv("GROKOS_STATE_DIR", os, 1);
	else
		unsetenv("GROKOS_STATE_DIR");
	if (orun)
		setenv("GROKOS_RUNTIME_DIR", orun, 1);
	else
		unsetenv("GROKOS_RUNTIME_DIR");
	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_paths_suite(void)
{
	t_run("open_null", test_open_null);
	t_run("refuse_tmp_runtime", test_refuse_tmp_runtime);
	t_run("creates_state_tree_and_modes", test_creates_state_tree_and_modes);
	t_run("env_action_log_override", test_env_action_log_override);
	t_run("env_state_runtime_override", test_env_state_runtime_override);
}
