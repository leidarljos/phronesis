/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void wait_stopped(policyd_supervisor_t *s, const char *id)
{
	policyd_agent_status_t stt;
	int i;

	for (i = 0; i < 50; i++) {
		policyd_supervisor_status(s, id, &stt);
		if (stt.state != POLICYD_AGENT_RUNNING)
			return;
		usleep(10 * 1000);
	}
}


static void test_deny_all_env(void **state)
{
	policyd_supervisor_t *s = NULL;
	policyd_policy_result_t pr;
	char tmpl_s[] = "/tmp/gpd-denys-XXXXXX";
	char tmpl_r[] = "/tmp/gpd-denyr-XXXXXX";
	char *state_dir;
	char *run_dir;
	(void)state;

	char *argv[] = { "sleep", "30", NULL };

	state_dir = mkdtemp(tmpl_s);
	run_dir = mkdtemp(tmpl_r);
	assert_non_null(state_dir);
	assert_non_null(run_dir);
	assert_int_equal(policyd_supervisor_open(&s, state_dir, run_dir), POLICYD_OK);
	assert_non_null(s);
	assert_int_equal(policyd_supervisor_start(s,
					       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
					       NULL, "/ws", argv),
			 POLICYD_OK);

	setenv("GROKOS_POLICYD_DENY_ALL", "1", 1);
	assert_int_equal(
		policyd_policy_check(s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "model", "start",
				  "/bin/true", &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_DENY_ALL);

	assert_int_equal(
		policyd_policy_check(s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "seat", "publish_run",
				  "x", &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);

	unsetenv("GROKOS_POLICYD_DENY_ALL");
	assert_int_equal(
		policyd_policy_check(s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "model", "start",
				  "/bin/true", &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);

	assert_int_equal(policyd_supervisor_stop(s,
					      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
			 POLICYD_OK);
	policyd_supervisor_close(s);
}

static void test_deny_all_truthy_spellings(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	const char *deny_vals[] = { "true", "yes", "TRUE", "YES" };
	const char *allow_vals[] = { "0", "false" };
	size_t i;

	char *argv[] = { "sleep", "30", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "denyt"),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s,
					       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
					       NULL, "/ws", argv),
			 POLICYD_OK);
	for (i = 0; i < sizeof(deny_vals) / sizeof(deny_vals[0]); i++) {
		setenv("GROKOS_POLICYD_DENY_ALL", deny_vals[i], 1);
		assert_int_equal(
			policyd_policy_check(s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
					  "model", "start", "/bin/true", &pr),
			POLICYD_OK);
		assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
		assert_int_equal(pr.code, POLICYD_REASON_DENY_ALL);
		unsetenv("GROKOS_POLICYD_DENY_ALL");
	}
	for (i = 0; i < sizeof(allow_vals) / sizeof(allow_vals[0]); i++) {
		setenv("GROKOS_POLICYD_DENY_ALL", allow_vals[i], 1);
		assert_int_equal(
			policyd_policy_check(s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
					  "model", "start", "/bin/true", &pr),
			POLICYD_OK);
		assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
		unsetenv("GROKOS_POLICYD_DENY_ALL");
	}
	assert_int_equal(policyd_supervisor_stop(s,
					      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
			 POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_seat_board_actions(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	const char *actions[] = {
		"publish_run",
		"read_run",
		"list_runs",
		"list_events",
	};
	char *argv[] = { "sleep", "30", NULL };
	size_t i;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "seat"),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws", argv),
			 POLICYD_OK);
	for (i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
		assert_int_equal(
			policyd_policy_check(s, "agent-a", "seat", actions[i], NULL,
					  &pr),
			POLICYD_OK);
		assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
		assert_int_equal(pr.code, POLICYD_REASON_SEAT_BOARD_ALLOW);
	}
	assert_int_equal(
		policyd_policy_check(s, "agent-a", "seat", "not_a_board_op", NULL,
				  &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_UNKNOWN_SEAT_ACTION);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_seat_model_identity_deny(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "sleep", "30", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "idny"),
			 POLICYD_OK);

	assert_int_equal(policyd_policy_check(s, NULL, "seat", "publish_run", NULL,
					   &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_INVALID_MESSAGE);

	assert_int_equal(policyd_policy_check(s, "", "model", "start", NULL, &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_INVALID_MESSAGE);

	assert_int_equal(policyd_policy_check(s, "missing-slot", "seat",
					   "publish_run", NULL, &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_TOOLS_DEFAULT_DENY);

	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws", argv),
			 POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-a", "model", "start", NULL,
					   &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
	assert_int_equal(pr.code, POLICYD_REASON_MODEL_START_ALLOW);

	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-a", "seat", "list_runs",
					   NULL, &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_TOOLS_DEFAULT_DENY);

	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_sensitive_path_deny(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "true", NULL };
	const char *paths[] = {
		"/ws/proj/.env",
		"/ws/proj/.env.local",
		"/ws/proj/.netrc",
		"/ws/proj/.npmrc",
		"/ws/proj/.pypirc",
		"/home/u/.ssh/id_rsa",
		"/home/u/.ssh/id_ed25519",
		"/home/u/.ssh/id_ecdsa",
		"/home/u/.ssh/id_dsa",
		"/ws/proj/credentials",
		"/ws/proj/credentials.json",
		"/ws/proj/service-account.json",
		"/ws/proj/token",
		"/ws/proj/secrets.yaml",
		"/ws/proj/secrets.yml",
		"/ws/proj/secrets.json",
		"/ws/proj/cert.pem",
		"/ws/proj/id.key",
		"/ws/proj/store.p12",
		"/ws/proj/store.pfx",
	};
	const char *fs_acts[] = { "read", "write", "delete" };
	size_t i, j;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "sens"),
			 POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv),
			 POLICYD_OK);
	for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		for (j = 0; j < sizeof(fs_acts) / sizeof(fs_acts[0]); j++) {
			assert_int_equal(
				policyd_policy_check(s, "agent-a", "fs", fs_acts[j],
						  paths[i], &pr),
				POLICYD_OK);
			assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
			assert_int_equal(pr.code, POLICYD_REASON_PATH_SENSITIVE_DENY);
		}
		assert_int_equal(
			policyd_policy_check(s, "agent-a", "shell", "exec",
					  paths[i], &pr),
			POLICYD_OK);
		assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
		assert_int_equal(pr.code, POLICYD_REASON_PATH_SENSITIVE_DENY);
	}
	wait_stopped(s, "agent-a");
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_tools_default_deny(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "sleep", "30", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "pol"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), POLICYD_OK);
	/* shell/exec with empty path or outside workspace stays deny */
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", NULL, &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", "/etc/passwd", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "", "read", "/ws/proj/a", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_shell_exec_workspace_allow(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "shx"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), POLICYD_OK);

	/* path = absolute cwd / target root under workspace → allow */
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", "/ws/proj", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
	assert_int_equal(pr.code, POLICYD_REASON_SHELL_EXEC_ALLOW);
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", "/ws/proj/sub", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);

	/* empty path, outside workspace, unclean → deny */
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", NULL, &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec", "/ws/other", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "exec",
					   "/ws/proj/../etc", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);

	/* high-risk action still prompts (precedence over shell allow) */
	assert_int_equal(policyd_policy_check(s, "agent-a", "shell", "sudo", "/ws/proj", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_PROMPT);

	/* no workspace on agent → deny even under a path that looks absolute */
	assert_int_equal(policyd_supervisor_start(s, "agent-b", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-b", "shell", "exec", "/ws/proj", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);

	/* unknown agent → empty workspace → deny */
	assert_int_equal(policyd_policy_check(s, "agent-missing", "shell", "exec", "/ws/proj",
					   &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);

	wait_stopped(s, "agent-a");
	wait_stopped(s, "agent-b");
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_workspace_allowlist(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "ws"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj/file", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "write", "/ws/proj/out", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_ALLOW);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/other/x", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/projevil", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj/../etc/passwd", &pr),
			 POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	wait_stopped(s, "agent-a");
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_high_risk_prompt(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "risk"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws", argv), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "delete", "/ws/x", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_PROMPT);
	assert_int_equal(policyd_policy_check(s, "agent-a", "net", "network", NULL, &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_PROMPT);
	/* secret_export: fail closed (never leave seat / enter traces). */
	assert_int_equal(policyd_policy_check(s, "agent-a", "vault", "secret_export", NULL, &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(pr.code, POLICYD_REASON_SECRET_EXPORT_DENIED);
	wait_stopped(s, "agent-a");
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_policy_logged(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char line[1024];

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "plog"), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-x", "shell", "exec", NULL, &pr), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_non_null(strstr(line, "\"kind\":\"policy\""));
	assert_true(strstr(line, "decision=0") != NULL || strstr(line, "deny") != NULL);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_lexical_rejects_dot_and_slashslash(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	policyd_policy_result_t pr;
	char *argv[] = { "true", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "lex"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, "/ws/proj", argv), POLICYD_OK);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj//file", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj/./file", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(policyd_policy_check(s, "agent-a", "fs", "read", "ws/proj/file", &pr), POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	/* Final `.` / `..` components (not `/./file` / `/../etc`). */
	assert_int_equal(
		policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj/.", &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	assert_int_equal(
		policyd_policy_check(s, "agent-a", "fs", "read", "/ws/proj/..", &pr),
		POLICYD_OK);
	assert_int_equal(pr.decision, POLICYD_DECISION_DENY);
	wait_stopped(s, "agent-a");
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_policy_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_deny_all_env),
		cmocka_unit_test(test_deny_all_truthy_spellings),
		cmocka_unit_test(test_seat_board_actions),
		cmocka_unit_test(test_seat_model_identity_deny),
		cmocka_unit_test(test_sensitive_path_deny),
		cmocka_unit_test(test_tools_default_deny),
		cmocka_unit_test(test_shell_exec_workspace_allow),
		cmocka_unit_test(test_workspace_allowlist),
		cmocka_unit_test(test_high_risk_prompt),
		cmocka_unit_test(test_policy_logged),
		cmocka_unit_test(test_lexical_rejects_dot_and_slashslash),
	};
	return cmocka_run_group_tests_name("policy", tests, NULL, NULL);
}
