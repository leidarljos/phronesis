/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <string.h>
#include <unistd.h>

static void test_invalid_ids_and_args(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	char *argv_ok[] = { "true", NULL };
	char long_id[GROK_ID_MAX + 8];
	grok_agent_status_t stt;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "inval"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "", NULL, NULL, argv_ok), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "bad/id", NULL, NULL, argv_ok), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "bad id", NULL, NULL, argv_ok), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "bad.id", NULL, NULL, argv_ok), GROK_ERR_INVAL);
	memset(long_id, 'a', sizeof(long_id));
	long_id[GROK_ID_MAX] = '\0';
	assert_int_equal(grok_supervisor_start(s, long_id, NULL, NULL, argv_ok), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, NULL), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "agent-a", "bad mode", NULL, argv_ok), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, "/tmp/ws with space", argv_ok),
			 GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_status(s, "agent-a", NULL), GROK_ERR_INVAL);
	assert_int_equal(grok_supervisor_status(s, "nope", &stt), GROK_ERR_NOTFOUND);
	assert_int_equal(grok_supervisor_stop(s, "nope"), GROK_ERR_NOTFOUND);
	assert_int_equal(grok_supervisor_stop(NULL, "agent-a"), GROK_ERR_INVAL);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_start_status_stop_happy(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "happy"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", "research", "/ws/proj", argv), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
	assert_int_equal(stt.state, GROK_AGENT_RUNNING);
	assert_true(stt.pid > 1);
	assert_int_equal(stt.pid, stt.pgid);
	assert_string_equal(stt.mode, "research");
	assert_string_equal(stt.workspace, "/ws/proj");
	pid = stt.pid;
	assert_true(t_pid_alive(pid));
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_ERR_EXISTS);
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	assert_false(t_pid_alive(pid));
	assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
	assert_int_equal(stt.state, GROK_AGENT_STOPPED);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_default_mode_and_restart(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	char *argv[] = { "sleep", "60", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "restart"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
	assert_string_equal(stt.mode, "develop");
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", "focus", NULL, argv), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
	assert_int_equal(stt.state, GROK_AGENT_RUNNING);
	assert_string_equal(stt.mode, "focus");
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_natural_exit_reaped(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	char *argv[] = { "true", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "exit"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
		if (stt.state != GROK_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, GROK_AGENT_STOPPED);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK);
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_exec_failure_child(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	char *argv[] = { "/no/such/grok-policyd-agent-bin-xyz", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "execfail"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(grok_supervisor_status(s, "agent-a", &stt), GROK_OK);
		if (stt.state != GROK_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, GROK_AGENT_STOPPED);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_two_agents_independent(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t sa, sb;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pa, pb;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "two"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-b", NULL, NULL, argv), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, "agent-a", &sa), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, "agent-b", &sb), GROK_OK);
	pa = sa.pid;
	pb = sb.pid;
	assert_int_not_equal(pa, pb);
	assert_int_not_equal(sa.pgid, sb.pgid);
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	assert_false(t_pid_alive(pa));
	assert_true(t_pid_alive(pb));
	assert_int_equal(grok_supervisor_status(s, "agent-b", &sb), GROK_OK);
	assert_int_equal(sb.state, GROK_AGENT_RUNNING);
	assert_int_equal(grok_supervisor_stop(s, "agent-b"), GROK_OK);
	assert_false(t_pid_alive(pb));
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_bind_slot_no_fork(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	const char *hex = "00000000000000010000000000000002";

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "bind"),
			 GROK_OK);
	assert_int_equal(grok_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 GROK_OK);
	assert_int_equal(grok_supervisor_status(s, hex, &stt), GROK_OK);
	assert_int_equal(stt.state, GROK_AGENT_RUNNING);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(grok_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 GROK_ERR_EXISTS);
	assert_int_equal(grok_supervisor_stop(s, hex), GROK_OK);
	assert_int_equal(grok_supervisor_status(s, hex, &stt), GROK_OK);
	assert_int_equal(stt.state, GROK_AGENT_STOPPED);
	grok_supervisor_close(s);
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
