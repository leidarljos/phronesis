/* SPDX-License-Identifier: Apache-2.0 */
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
	assert_int_equal(policyd_supervisor_open(NULL, "/x", "/y"), POLICYD_ERR_INVAL);
}

static void test_refuse_tmp_runtime(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX];

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-tmp-st"), 0);
	assert_int_equal(policyd_supervisor_open(&s, st, "/tmp"), POLICYD_ERR_STATE);
	assert_int_equal(policyd_supervisor_open(&s, st, "/tmp/"), POLICYD_ERR_STATE);
	assert_null(s);
	t_rm_rf(st);
}

static void test_creates_state_tree_and_modes(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX], path[POLICYD_PATH_MAX];
	struct stat sb;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "modes"), POLICYD_OK);
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
	assert_non_null(strstr(policyd_supervisor_action_log_path(s), st));
	assert_non_null(strstr(policyd_supervisor_action_log_path(s), "actions.jsonl"));
	assert_string_equal(policyd_supervisor_state_dir(s), st);
	assert_string_equal(policyd_supervisor_runtime_dir(s), rt);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_env_action_log_override(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX], logpath[POLICYD_PATH_MAX];
	char *old;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-env-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-env-rt"), 0);
	snprintf(logpath, sizeof(logpath), "%s/custom.jsonl", st);
	old = getenv("GROKOS_ACTION_LOG");
	setenv("GROKOS_ACTION_LOG", logpath, 1);
	assert_int_equal(policyd_supervisor_open(&s, st, rt), POLICYD_OK);
	assert_string_equal(policyd_supervisor_action_log_path(s), logpath);
	policyd_supervisor_close(s);
	if (old)
		setenv("GROKOS_ACTION_LOG", old, 1);
	else
		unsetenv("GROKOS_ACTION_LOG");
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_open_creates_three_deep_missing_state(void **state)
{
	policyd_supervisor_t *s = NULL;
	char root[POLICYD_PATH_MAX], st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	struct stat sb;

	(void)state;
	assert_int_equal(t_tmpdir(root, sizeof(root), "gp-deep"), 0);
	t_rm_rf(root);
	snprintf(st, sizeof(st), "%s/a/b/c", root);
	snprintf(rt, sizeof(rt), "%s/run", root);
	assert_int_equal(policyd_supervisor_open(&s, st, rt), POLICYD_OK);
	assert_non_null(s);
	assert_int_equal(stat(st, &sb), 0);
	assert_true(S_ISDIR(sb.st_mode));
	assert_int_equal(sb.st_mode & 0777, 0700);
	assert_int_equal(stat(rt, &sb), 0);
	assert_true(S_ISDIR(sb.st_mode));
	assert_int_equal(sb.st_mode & 0777, 0700);
	policyd_supervisor_close(s);
	t_rm_rf(root);
}

static void test_ensure_dir_leaves_existing_parent_mode(void **state)
{
	char root[POLICYD_PATH_MAX], child[POLICYD_PATH_MAX];
	struct stat sb;

	(void)state;
	assert_int_equal(t_tmpdir(root, sizeof(root), "gp-parent"), 0);
	assert_int_equal(chmod(root, 0755), 0);
	snprintf(child, sizeof(child), "%s/leaf", root);
	assert_int_equal(policyd_paths_ensure_dir(child, 0700), POLICYD_OK);
	assert_int_equal(stat(root, &sb), 0);
	assert_int_equal(sb.st_mode & 0777, 0755);
	assert_int_equal(stat(child, &sb), 0);
	assert_true(S_ISDIR(sb.st_mode));
	assert_int_equal(sb.st_mode & 0777, 0700);
	t_rm_rf(root);
}

static void test_env_state_runtime_override(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char *os, *orun;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-env2-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-env2-rt"), 0);
	os = getenv("GROKOS_STATE_DIR");
	orun = getenv("GROKOS_RUNTIME_DIR");
	setenv("GROKOS_STATE_DIR", st, 1);
	setenv("GROKOS_RUNTIME_DIR", rt, 1);
	assert_int_equal(policyd_supervisor_open(&s, NULL, NULL), POLICYD_OK);
	assert_string_equal(policyd_supervisor_state_dir(s), st);
	assert_string_equal(policyd_supervisor_runtime_dir(s), rt);
	policyd_supervisor_close(s);
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
	assert_int_equal(policyd_map_admit_kind("seat", &tool, &action), 0);
	assert_string_equal(tool, "seat");
	assert_string_equal(action, "publish_run");

	assert_int_equal(policyd_map_admit_kind("model", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(policyd_map_admit_kind("", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(policyd_map_admit_kind("agent", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");
}

static void test_map_admit_kind_unknown_fail_closed(void **state)
{
	const char *tool = "model";
	const char *action = "start";

	(void)state;
	assert_int_equal(policyd_map_admit_kind("weird", &tool, &action), -1);
	assert_int_equal(policyd_map_admit_kind("shell", &tool, &action), -1);
	assert_int_equal(policyd_map_admit_kind(NULL, &tool, &action), -1);
}

int run_paths_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_open_null),
		cmocka_unit_test(test_refuse_tmp_runtime),
		cmocka_unit_test(test_creates_state_tree_and_modes),
		cmocka_unit_test(test_open_creates_three_deep_missing_state),
		cmocka_unit_test(test_ensure_dir_leaves_existing_parent_mode),
		cmocka_unit_test(test_env_action_log_override),
		cmocka_unit_test(test_env_state_runtime_override),
		cmocka_unit_test(test_map_admit_kind_known),
		cmocka_unit_test(test_map_admit_kind_unknown_fail_closed),
	};
	return cmocka_run_group_tests_name("paths", tests, NULL, NULL);
}
