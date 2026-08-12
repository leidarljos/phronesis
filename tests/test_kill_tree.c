/* SPDX-License-Identifier: MIT */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_tree_script(const char *path, const char *marker, int depth, int ignore_term)
{
	char body[2048];
	int n;

	if (depth <= 1) {
		n = snprintf(body, sizeof(body),
			     "#!/bin/sh\n"
			     "%s"
			     "sleep 120 &\n"
			     "echo $! > '%s'\n"
			     "wait\n",
			     ignore_term ? "trap '' TERM\n" : "",
			     marker);
	} else {
		n = snprintf(body, sizeof(body),
			     "#!/bin/sh\n"
			     "%s"
			     "(\n"
			     "  sleep 120 &\n"
			     "  echo $! > '%s'\n"
			     "  wait\n"
			     ") &\n"
			     "wait\n",
			     ignore_term ? "trap '' TERM\n" : "",
			     marker);
	}
	if (n < 0 || (size_t)n >= sizeof(body))
		return -1;
	if (t_write_file(path, body) != 0)
		return -1;
	return chmod(path, 0700);
}

static void run_tree_case(const char *tag, int depth, int ignore_term)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	char script[PHRONESIS_PATH_MAX], marker[PHRONESIS_PATH_MAX];
	char *argv[3];
	pid_t leader = 0, leaf = 0;
	phronesis_agent_status_t stt;
	int tries;

	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), tag), PHRONESIS_OK);
	snprintf(script, sizeof(script), "%s/tree.sh", rt);
	snprintf(marker, sizeof(marker), "%s/leaf.pid", rt);
	assert_int_equal(write_tree_script(script, marker, depth, ignore_term), 0);
	argv[0] = "/bin/sh";
	argv[1] = script;
	argv[2] = NULL;
	assert_int_equal(phronesis_supervisor_start(s, "agent-tree", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-tree", &stt), PHRONESIS_OK);
	leader = stt.pid;
	assert_true(t_pid_alive(leader));
	assert_int_equal(t_wait_file(marker, 2000), 0);
	assert_int_equal(t_read_pidfile(marker, &leaf), 0);
	assert_true(leaf > 1 && leaf != leader);
	assert_true(t_pid_alive(leaf));
	assert_int_equal(phronesis_supervisor_stop(s, "agent-tree"), PHRONESIS_OK);
	for (tries = 0; tries < 100; tries++) {
		if (!t_pid_alive(leader) && !t_pid_alive(leaf))
			break;
		usleep(10 * 1000);
	}
	assert_false(t_pid_alive(leader));
	assert_false(t_pid_alive(leaf));
	assert_int_equal(phronesis_supervisor_status(s, "agent-tree", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_kill_one_level_grandchild(void **state)
{
	(void)state;
	run_tree_case("kt1", 1, 0);
}

static void test_kill_nested_shell_tree(void **state)
{
	(void)state;
	run_tree_case("kt2", 2, 0);
}

static void test_kill_sigterm_ignored_leader(void **state)
{
	(void)state;
	run_tree_case("kt3", 1, 1);
}

static void test_stop_after_external_kill(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "extkill"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	pid = stt.pid;
	assert_int_equal(kill(pid, SIGKILL), 0);
	usleep(50 * 1000);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_thrash_start_stop(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	char *argv[] = { "sleep", "30", NULL };
	int i;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "thrash"), PHRONESIS_OK);
	for (i = 0; i < 8; i++) {
		assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
		assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	}
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_kill_tree_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_kill_one_level_grandchild),
		cmocka_unit_test(test_kill_nested_shell_tree),
		cmocka_unit_test(test_kill_sigterm_ignored_leader),
		cmocka_unit_test(test_stop_after_external_kill),
		cmocka_unit_test(test_thrash_start_stop),
	};
	return cmocka_run_group_tests_name("kill_tree", tests, NULL, NULL);
}
