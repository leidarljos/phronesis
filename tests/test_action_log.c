/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int count_lines(const char *path)
{
	FILE *f = fopen(path, "r");
	char buf[2048];
	int n = 0;

	if (!f)
		return -1;
	while (fgets(buf, sizeof(buf), f))
		n++;
	fclose(f);
	return n;
}

static void test_start_stop_log_lines(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char line[1024];
	const char *logpath;
	char *argv[] = { "sleep", "30", NULL };

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "alog"), POLICYD_OK);
	logpath = policyd_supervisor_action_log_path(s);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_non_null(strstr(line, "\"kind\":\"start\""));
	assert_non_null(strstr(line, "\"agent\":\"agent-a\""));
	assert_non_null(strstr(line, "pid="));
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_non_null(strstr(line, "\"kind\":\"stop\""));
	assert_true(count_lines(logpath) >= 2);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_log_json_escape_and_system(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char line[1024];

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "esc"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log(s, NULL, "note", "say \"hi\" \\ ok"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_non_null(strstr(line, "\\\"hi\\\""));
	assert_non_null(strstr(line, "\\\\"));
	assert_non_null(strstr(line, "\"agent\":\"\""));
	assert_int_equal(policyd_supervisor_log(s, "bad id", "x", "y"), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_log(s, "agent-a", "", "y"), POLICYD_ERR_INVAL);
	assert_int_equal(policyd_supervisor_log(s, "agent-a", NULL, "y"), POLICYD_ERR_INVAL);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_log_append_order(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char line[1024];
	const char *logpath;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "ord"), POLICYD_OK);
	logpath = policyd_supervisor_action_log_path(s);
	assert_int_equal(policyd_supervisor_log(s, "agent-a", "k1", "d1"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log(s, "agent-a", "k2", "d2"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log(s, "agent-b", "k3", "d3"), POLICYD_OK);
	assert_int_equal(count_lines(logpath), 3);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_non_null(strstr(line, "\"kind\":\"k3\""));
	assert_non_null(strstr(line, "agent-b"));
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

static void test_idempotent_stop_logged(void **state)
{
	policyd_supervisor_t *s = NULL;
	char st[POLICYD_PATH_MAX], rt[POLICYD_PATH_MAX];
	char line[1024];
	char *argv[] = { "true", NULL };
	policyd_agent_status_t stt;
	int tries;

	(void)state;
	assert_int_equal(t_open_pair(&s, st, sizeof(st), rt, sizeof(rt), "idemp"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_start(s, "agent-a", NULL, NULL, argv), POLICYD_OK);
	for (tries = 0; tries < 100; tries++) {
		policyd_supervisor_status(s, "agent-a", &stt);
		if (stt.state != POLICYD_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	assert_int_equal(policyd_supervisor_stop(s, "agent-a"), POLICYD_OK);
	assert_int_equal(policyd_supervisor_log_last(s, line, sizeof(line)), POLICYD_OK);
	assert_true(strstr(line, "idempotent") != NULL || strstr(line, "\"kind\":\"stop\"") != NULL);
	policyd_supervisor_close(s);
	t_rm_rf(st);
	t_rm_rf(rt);
}

int run_action_log_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_start_stop_log_lines),
		cmocka_unit_test(test_log_json_escape_and_system),
		cmocka_unit_test(test_log_append_order),
		cmocka_unit_test(test_idempotent_stop_logged),
	};
	return cmocka_run_group_tests_name("action_log", tests, NULL, NULL);
}
