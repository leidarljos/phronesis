/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <string.h>
#include <unistd.h>

static void test_tools_default_deny(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "sleep", "30", NULL };

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "pol") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), GROK_OK, "start");

	t_expect_eq(grok_policy_check(s, "agent-a", "shell", "exec", NULL, &pr), GROK_OK, "check");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "default deny exec");

	t_expect_eq(grok_policy_check(s, "agent-a", "shell", "exec", "/etc/passwd", &pr), GROK_OK,
		    "check outside");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "outside deny");

	t_expect_eq(grok_policy_check(s, "agent-a", "", "read", "/ws/proj/a", &pr), GROK_OK,
		    "empty tool");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "empty tool deny");

	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_workspace_allowlist(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "true", NULL };

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "ws") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), GROK_OK, "start");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/file", &pr), GROK_OK,
		    "read under");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_ALLOW, "allow under workspace");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "write", "/ws/proj/out", &pr), GROK_OK,
		    "write under");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_ALLOW, "allow write under");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "read", "/ws/other/x", &pr), GROK_OK,
		    "sibling");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "deny sibling prefix");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "read", "/ws/projevil", &pr), GROK_OK,
		    "prefix attack");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "deny prefix attack");

	/* Keel/Rohit: reject lexical .. traversal even if absolute and prefix-matching */
	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/../etc/passwd", &pr),
		    GROK_OK, "dotdot");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "deny .. under workspace prefix");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "read", "/ws/proj/foo/../../etc", &pr),
		    GROK_OK, "nested dotdot");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_DENY, "deny nested ..");

	/* wait natural exit */
	{
		grok_agent_status_t st;
		int i;
		for (i = 0; i < 50; i++) {
			grok_supervisor_status(s, "agent-a", &st);
			if (st.state != GROK_AGENT_RUNNING)
				break;
			usleep(10 * 1000);
		}
	}
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_high_risk_prompt(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char *argv[] = { "true", NULL };

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "risk") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, "/ws", argv), GROK_OK, "start");

	t_expect_eq(grok_policy_check(s, "agent-a", "fs", "delete", "/ws/x", &pr), GROK_OK, "del");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_PROMPT, "delete prompts");

	t_expect_eq(grok_policy_check(s, "agent-a", "net", "network", NULL, &pr), GROK_OK, "net");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_PROMPT, "network prompts");

	t_expect_eq(grok_policy_check(s, "agent-a", "vault", "secret_export", NULL, &pr), GROK_OK,
		    "secret");
	t_expect_eq((long)pr.decision, (long)GROK_DECISION_PROMPT, "secret prompts");

	{
		grok_agent_status_t st;
		int i;
		for (i = 0; i < 50; i++) {
			grok_supervisor_status(s, "agent-a", &st);
			if (st.state != GROK_AGENT_RUNNING)
				break;
			usleep(10 * 1000);
		}
	}
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_policy_logged(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_policy_result_t pr;
	char line[1024];

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "plog") == GROK_OK,
		 "open");
	t_expect_eq(grok_policy_check(s, "agent-x", "shell", "exec", NULL, &pr), GROK_OK, "check");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "log");
	t_expect(strstr(line, "\"kind\":\"policy\"") != NULL, "policy kind");
	t_expect(strstr(line, "decision=0") != NULL || strstr(line, "deny") != NULL, "deny logged");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_policy_suite(void)
{
	t_run("tools_default_deny", test_tools_default_deny);
	t_run("workspace_allowlist", test_workspace_allowlist);
	t_run("high_risk_prompt", test_high_risk_prompt);
	t_run("policy_logged", test_policy_logged);
}
