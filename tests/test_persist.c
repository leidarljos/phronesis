/* SPDX-License-Identifier: MIT */
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
	phronesis_supervisor_t *s1 = NULL, *s2 = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX], slot[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;
	char *argv[] = { "sleep", "60", NULL };
	pid_t pid;

	(void)state;
	assert_int_equal(t_tmpdir(st, sizeof(st), "gp-p-st"), 0);
	assert_int_equal(t_tmpdir(rt, sizeof(rt), "gp-p-rt"), 0);
	assert_int_equal(phronesis_supervisor_open(&s1, st, rt), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_start(s1, "agent-a", "develop", "/ws", argv), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s1, "agent-a", &stt), PHRONESIS_OK);
	pid = stt.pid;
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", rt);
	assert_int_equal(access(slot, F_OK), 0);
	phronesis_supervisor_close(s1);
	assert_int_equal(phronesis_supervisor_open(&s2, st, rt), PHRONESIS_OK);
	assert_int_equal(phronesis_supervisor_status(s2, "agent-a", &stt), PHRONESIS_OK);
	assert_int_equal(stt.state, PHRONESIS_AGENT_RUNNING);
	assert_int_equal(stt.pid, pid);
	assert_string_equal(stt.mode, "develop");
	assert_string_equal(stt.workspace, "/ws");
	assert_true(t_pid_alive(pid));
	assert_int_equal(phronesis_supervisor_stop(s2, "agent-a"), PHRONESIS_OK);
	assert_false(t_pid_alive(pid));
	phronesis_supervisor_close(s2);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_corrupt_slot(void **state)
{
	phronesis_supervisor_t *s = NULL;
	char st[PHRONESIS_PATH_MAX], rt[PHRONESIS_PATH_MAX], slot[PHRONESIS_PATH_MAX];
	phronesis_agent_status_t stt;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "corrupt"), PHRONESIS_OK);
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", rt);
	assert_int_equal(t_write_file(slot, "not-a-valid-slot\n"), 0);
	assert_int_equal(phronesis_supervisor_status(s, "agent-a", &stt), PHRONESIS_ERR_IO);
	assert_int_equal(phronesis_supervisor_stop(s, "agent-a"), PHRONESIS_ERR_IO);
	phronesis_supervisor_close(s);
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
