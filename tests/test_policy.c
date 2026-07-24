/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <string.h>
#include <unistd.h>

static void wait_stopped(grok_supervisor_t *s, const char *id)
{
	grok_agent_status_t stt;
	int i;

	for (i = 0; i < 50; i++) {
		grok_supervisor_status(s, id, &stt);
		if (stt.state != GROK_AGENT_RUNNING)
			return;
		usleep(10 * 1000);
	}
}

static void test_tools_default_deny(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "sleep", "30", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "pol"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), GROK_OK);
	assert_int_equal(grok_policy_check(s, "agent-a", "shell", "exec", NULL, &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "shell", "exec", "/etc/passwd", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "", "read", "/ws/proj/a", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_supervisor_stop(s, "agent-a"), GROK_OK);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_workspace_allowlist(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "ws"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), GROK_OK);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/file", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_ALLOW);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "write", "/ws/proj/out", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_ALLOW);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/other/x", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/projevil", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/../etc/passwd", &pr),
			 GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	wait_stopped(s, "agent-a");
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_high_risk_prompt(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "risk"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, "/ws", argv), GROK_OK);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "delete", "/ws/x", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_PROMPT);
	assert_int_equal(grok_policy_check(s, "agent-a", "net", "network", NULL, &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_PROMPT);
	assert_int_equal(grok_policy_check(s, "agent-a", "vault", "secret_export", NULL, &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_PROMPT);
	wait_stopped(s, "agent-a");
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_policy_logged(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char line[1024];

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "plog"), GROK_OK);
	assert_int_equal(grok_policy_check(s, "agent-x", "shell", "exec", NULL, &pr), GROK_OK);
	assert_int_equal(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK);
	assert_non_null(strstr(line, "\"kind\":\"policy\""));
	assert_true(strstr(line, "decision=0") != NULL || strstr(line, "deny") != NULL);
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_lexical_rejects_dot_and_slashslash(void **state)
{
	grok_supervisor_t *s = NULL;
	char st[GROK_PATH_MAX], rt[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "lex"), GROK_OK);
	assert_int_equal(grok_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), GROK_OK);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj//file", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/./file", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	assert_int_equal(grok_policy_check(s, "agent-a", "fs", "read", "ws/proj/file", &pr), GROK_OK);
	assert_int_equal(pr.decision, GROK_DECISION_DENY);
	wait_stopped(s, "agent-a");
	grok_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_policy_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_tools_default_deny),
		cmocka_unit_test(test_workspace_allowlist),
		cmocka_unit_test(test_high_risk_prompt),
		cmocka_unit_test(test_policy_logged),
		cmocka_unit_test(test_lexical_rejects_dot_and_slashslash),
	};
	return cmocka_run_group_tests_name("policy", tests, NULL, NULL);
}
