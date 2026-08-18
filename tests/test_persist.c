/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_slot_survives_reopen(void **state)
{
	policyd_supervisor_t *s1 = NULL, *s2 = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX], slot[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;
	char *argv[] = { "sleep", "60", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-p-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-p-rt"), 0);
	assert_int_equal(policyd_supervisor_open(&s1, st, rt), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s1, "agent-a", "develop", "/ws", argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s1, "agent-a", &stt), POLICYD_OK);
	pid = stt.pid;
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", rt);
	assert_int_equal(access(slot, F_OK), 0);
	policyd_supervisor_close(s1);
	assert_int_equal(policyd_supervisor_open(&s2, st, rt), POLICYD_OK);
	assert_int_equal(policyd_supervisor_status(s2, "agent-a", &stt), POLICYD_OK);
	assert_int_equal(stt.state, POLICYD_AGENT_RUNNING);
	assert_int_equal(stt.pid, pid);
	assert_string_equal(stt.mode, "develop");
	assert_string_equal(stt.workspace, "/ws");
	assert_true(t_pid_alive(pid));
	assert_int_equal(policyd_supervisor_stop(s2, "agent-a"), POLICYD_OK);
	assert_false(t_pid_alive(pid));
	policyd_supervisor_close(s2);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_corrupt_slot(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX], slot[POLICYD_PATH_MAX];
	policyd_agent_status_t stt;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "corrupt"), POLICYD_OK);
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", rt);
	assert_int_equal(t_write_file(slot, "not-a-valid-slot\n"), 0);
	assert_int_equal(policyd_supervisor_status(s, "agent-a", &stt), POLICYD_ERR_IO);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_ERR_IO);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_persist_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_slot_survives_reopen),
		cmocka_unit_test(test_corrupt_slot),
	};
	return cmocka_run_group_tests_name("persist", tests, NULL, NULL);
}
