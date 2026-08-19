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
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char *argv_ok[] = { "true", NULL };
	char long_id[POLICYD_ID_MAX + 8];
	policyd_agent_status_t stt;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "inval"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "", NULL, NULL, argv_ok), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "bad/id", NULL, NULL, argv_ok), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "bad id", NULL, NULL, argv_ok), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "bad.id", NULL, NULL, argv_ok), POLICYD_ERR_INVAL);
	memset(long_id, 'a', sizeof(long_id));
	long_id[POLICYD_ID_MAX] = '\0';
	assert_int_equal(policyd_supervisor_start(s, long_id, NULL, NULL, argv_ok), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, NULL), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", "bad mode", NULL, argv_ok), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/tmp/ws with space", argv_ok),
			 POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", NULL), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_status(s, "nope", &stt), POLICYD_ERR_NOTFOUND);
	assert_int_equal(policyd_supervisor_stop(s, "nope"), POLICYD_ERR_NOTFOUND);
	assert_int_equal(policyd_supervisor_stop(NULL, "agent-a"), POLICYD_ERR_INVAL);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_start_status_stop_happy(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "happy"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", "research", "/ws/proj", argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_RUNNING);
	assert_true(stt.pid > 1);
	assert_int_equal(stt.pid, stt.pgid);
	assert_string_equal(stt.mode, "research");
	assert_string_equal(stt.workspace, "/ws/proj");
	pid = stt.pid;
	assert_true(t_pid_alive(pid));
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_ERR_EXISTS);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_false(t_pid_alive(pid));
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_STOPPED);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_default_mode_and_restart(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	char *argv[] = { "sleep", "60", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "restart"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
	assert_string_equal(stt.mode, "develop");
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", "focus", NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_RUNNING);
	assert_string_equal(stt.mode, "focus");
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_natural_exit_reaped(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	char *argv[] = { "true", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "exit"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
		if (stt.state != POLICYD_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, POLICYD_AGENT_STOPPED);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_exec_failure_child(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	char *argv[] = { "/no/such/grok-policyd-agent-bin-xyz", NULL };
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "execfail"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	for (tries = 0; tries < 100; tries++) {
		assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_OK);
		if (stt.state != POLICYD_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(stt.state, POLICYD_AGENT_STOPPED);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_two_agents_independent(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t sa, sb;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pa, pb;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "two"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-b", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &sa), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, "agent-b", &sb), POLICYD_OK);
	pa = sa.pid;
	pb = sb.pid;
	assert_int_not_equal(pa, pb);
	assert_int_not_equal(sa.pgid, sb.pgid);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_false(t_pid_alive(pa));
	assert_true(t_pid_alive(pb));
	assert_int_equal(policyd_supervisor_status(s, "agent-b", &sb), POLICYD_OK);
	assert_int_equal(sb.state, POLICYD_AGENT_RUNNING);
	assert_int_equal(policyd_supervisor_stop(s, "agent-b"), POLICYD_OK);
	assert_false(t_pid_alive(pb));
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_bind_slot_no_fork(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	const char *hex = "00000000000000010000000000000002";

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "bind"),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, hex, &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_RUNNING);
	assert_int_equal(stt.pid, 0);
	assert_int_equal(policyd_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 POLICYD_ERR_EXISTS);
	assert_int_equal(policyd_supervisor_stop(s, hex), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, hex, &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_STOPPED);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_bind_fills_empty_workspace(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	const char *hex = "00000000000000010000000000000003";

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt),
				     "bindfill"),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_bind(s, hex, "agent", NULL, 0),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, hex, &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_RUNNING);
	assert_string_equal(stt.workspace, "");
	assert_int_equal(policyd_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s, hex, &stt), POLICYD_OK);
	assert_string_equal(stt.workspace, "/ws/proj");
	assert_int_equal(policyd_supervisor_bind(s, hex, "agent", "/ws/other", 0),
			 POLICYD_ERR_EXISTS);
	assert_int_equal(policyd_supervisor_status(s, hex, &stt), POLICYD_OK);
	assert_string_equal(stt.workspace, "/ws/proj");
	assert_int_equal(policyd_supervisor_stop(s, hex), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_bind_empty_does_not_wipe_workspace(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_agent_status_t stt;
	const char *hex = "00000000000000010000000000000004";

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt),
				     "bindkeep"),
			 GROK_OK);
	assert_int_equal(grok_supervisor_bind(s, hex, "agent", "/ws/proj", 0),
			 GROK_OK);
	assert_int_equal(grok_supervisor_stop(s, hex), GROK_OK);
	assert_int_equal(grok_supervisor_bind(s, hex, "agent", NULL, 0),
			 GROK_OK);
	assert_int_equal(grok_supervisor_status(s, hex, &stt), GROK_OK);
	assert_string_equal(stt.workspace, "/ws/proj");
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
		cmocka_unit_test(test_bind_fills_empty_workspace),
		cmocka_unit_test(test_bind_empty_does_not_wipe_workspace),
	};
	return cmocka_run_group_tests_name("lifecycle", tests, NULL, NULL);
}
