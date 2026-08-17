/* SPDX-License-Identifier: MIT */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <string.h>
#include <unistd.h>

static void test_invalid_ids_and_args(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	char *argv_ok[] = { "true", NULL };
	char long_id[PHRONESIS_ID_MAX + 8];
	phronesis_agent_status_t stt;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "inval"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "", NULL, NULL, argv_ok), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "bad/id", NULL, NULL, argv_ok), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "bad id", NULL, NULL, argv_ok), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "bad.id", NULL, NULL, argv_ok), PHRONESIS_ERR_INVAL);
	memset(long_id, 'a', sizeof(long_id));
	long_id[PHRONESIS_ID_MAX] = '\0';
	assert_int_equal(phronesis_supervisor_start(s, long_id, NULL, NULL, argv_ok), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, NULL), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", "bad mode", NULL, argv_ok), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, "/tmp/ws with space", argv_ok),
			 PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", NULL), PHRONESIS_ERR_INVAL);
	assert_int_equal(phronesis_supervisor_status(s, "nope", &stt), PHRONESIS_ERR_NOTFOUND);
	assert_int_equal(phronesis_supervisor_stop(s, "nope"), PHRONESIS_ERR_NOTFOUND);
	assert_int_equal(phronesis_supervisor_stop(NULL, "agent-a"), PHRONESIS_ERR_INVAL);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_start_status_stop_happy(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "happy"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", "research", "/ws/proj", argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_RUNNING);
	assert_true(stt.pid > 1);
	assert_int_equal(stt.pid, stt.pgid);
	assert_string_equal(stt.mode, "research");
	assert_string_equal(stt.workspace, "/ws/proj");
	pid = stt.pid;
	assert_true(t_pid_alive(pid));
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_ERR_EXISTS);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	assert_false(t_pid_alive(pid));
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_default_mode_and_restart(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "sleep", "60", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "restart"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	assert_string_equal(stt.mode, "develop");
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", "focus", NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_RUNNING);
	assert_string_equal(stt.mode, "focus");
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_natural_exit_reaped(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "true", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "exit"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
		if (stt.state != PHRONESIS_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_exec_failure_child(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "/no/such/phronesis-agent-bin-xyz", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "execfail"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_OK);
		if (stt.state != PHRONESIS_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_two_agents_independent(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t sa, sb;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pa, pb;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "two"), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-a", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s, "agent-b", NULL, NULL, argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &sa), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, "agent-b", &sb), PHRONESIS_OK);
	pa = sa.pid;
	pb = sb.pid;
	assert_int_not_equal(pa, pb);
	assert_int_not_equal(sa.pgid, sb.pgid);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_OK);
	assert_false(t_pid_alive(pa));
	assert_true(t_pid_alive(pb));
	assert_int_equal(phronesis_supervisor_status(s, "agent-b", &sb), PHRONESIS_OK);
	assert_int_equal(sb.state, PHRONESIS_AGENT_RUNNING);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-b"), PHRONESIS_OK);
	assert_false(t_pid_alive(pb));
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_bind_slot_no_fork(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	const char *hex = "00000000000000010000000000000002";

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "bind"),
			 PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, hex, &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_RUNNING);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(phronesis_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 PHRONESIS_ERR_EXISTS);
	assert_int_equal(phronesis_supervisor_stop(s, hex), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s, hex, &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_STOPPED);
	phronesis_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_lifecycle_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_ids_and_args),
		cmocka_unit_test(test_start_status_stop_happy),
		cmocka_unit_test(test_default_mode_and_restart),
		cmocka_unit_test(test_natural_exit_reaped),
		cmocka_unit_test(test_exec_failure_child),
		cmocka_unit_test(test_two_agents_independent),
		cmocka_unit_test(test_bind_slot_no_fork),
	};
	return cmocka_run_group_tests_name("lifecycle", tests, NULL, NULL);
}
