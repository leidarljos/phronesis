/* SPDX-License-Identifier: MIT */
#include "harness.h"
#include "internal.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void test_open_null(void **state)
{
	(void)state;
	assert_int_equal(grok_supervisor_open(NULL, "/x", "/y"), GROK_ERR_INVAL);
}

static void test_refuse_tmp_runtime(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX];

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-tmp-st"), 0);
	assert_int_equal(grok_supervisor_open(&s, st, "/tmp"), GROK_ERR_STATE);
	assert_int_equal(grok_supervisor_open(&s, st, "/tmp/"), GROK_ERR_STATE);
	assert_null(s);
	t_rm_rf(st);
}

static void test_creates_state_tree_and_modes(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX], path[GROK_PATH_MAX];
	struct stat sb;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "modes"), GROK_OK);
	assert_non_null(s);
	snprintf(path, sizeof(path), "%s/log", st);
	assert_int_equal(stat(path, &sb), 0);
	assert_true(S_ISDIR(sb.st_mode));
	assert_int_equal(sb.st_mode & 0777, 0700);
	snprintf(path, sizeof(path), "%s/policyd", st);
	assert_int_equal(stat(path, &sb), 0);
	snprintf(path, sizeof(path), "%s/agents", rt);
	assert_int_equal(stat(path, &sb), 0);
	assert_int_equal(sb.st_mode & 0777, 0700);
	assert_non_null(strstr(grok_supervisor_action_log_path(s), st));
	assert_non_null(strstr(grok_supervisor_action_log_path(s), "actions.jsonl"));
	assert_string_equal(grok_supervisor_state_dir(s), st);
	assert_string_equal(grok_supervisor_runtime_dir(s), rt);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_env_action_log_override(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX], logpath[GROK_PATH_MAX];
	char *old;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-env-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-env-rt"), 0);
	snprintf(logpath, sizeof(logpath), "%s/custom.jsonl", st);
	old = getenv("GROKOS_ACTION_LOG");
	setenv("GROKOS_ACTION_LOG", logpath, 1);
	assert_int_equal(grok_supervisor_open(&s, st, rt), GROK_OK);
	assert_string_equal(grok_supervisor_action_log_path(s), logpath);
	grok_supervisor_close(s);
	if (old)
		setenv("GROKOS_ACTION_LOG", old, 1);
	else
		unsetenv("GROKOS_ACTION_LOG");
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_env_state_runtime_override(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	char *os, *orun;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-env2-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-env2-rt"), 0);
	os = getenv("GROKOS_STATE_DIR");
	orun = getenv("GROKOS_RUNTIME_DIR");
	setenv("GROKOS_STATE_DIR", st, 1);
	setenv("GROKOS_RUNTIME_DIR", rt, 1);
	assert_int_equal(grok_supervisor_open(&s, NULL, NULL), GROK_OK);
	assert_string_equal(grok_supervisor_state_dir(s), st);
	assert_string_equal(grok_supervisor_runtime_dir(s), rt);
	grok_supervisor_close(s);
	if (os)
		setenv("GROKOS_STATE_DIR", os, 1);
	else
		unsetenv("GROKOS_STATE_DIR");
	if (orun)
		setenv("GROKOS_RUNTIME_DIR", orun, 1);
	else
		unsetenv("GROKOS_RUNTIME_DIR");
	t_rm_rf(st);
	t_rm_rf(rt);
}

/* admit.kind → tool/action map (Cap'n FFI helper; not sock serve). */
static void test_map_admit_kind_known(void **state)
{
	const char *tool = NULL;
	const char *action = NULL;

	(void)state;
	assert_int_equal(grok_policyd_map_admit_kind("seat", &tool, &action), 0);
	assert_string_equal(tool, "seat");
	assert_string_equal(action, "publish_run");

	assert_int_equal(grok_policyd_map_admit_kind("model", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(grok_policyd_map_admit_kind("", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(grok_policyd_map_admit_kind("agent", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");
}

static void test_map_admit_kind_unknown_fail_closed(void **state)
{
	const char *tool = "model";
	const char *action = "start";

	(void)state;
	assert_int_equal(grok_policyd_map_admit_kind("weird", &tool, &action), -1);
	assert_int_equal(grok_policyd_map_admit_kind("shell", &tool, &action), -1);
	assert_int_equal(grok_policyd_map_admit_kind(NULL, &tool, &action), -1);
}

int run_paths_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_open_null),
		cmocka_unit_test(test_refuse_tmp_runtime),
		cmocka_unit_test(test_creates_state_tree_and_modes),
		cmocka_unit_test(test_env_action_log_override),
		cmocka_unit_test(test_env_state_runtime_override),
		cmocka_unit_test(test_map_admit_kind_known),
		cmocka_unit_test(test_map_admit_kind_unknown_fail_closed),
	};
	return cmocka_run_group_tests_name("paths", tests, NULL, NULL);
}
