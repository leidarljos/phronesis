/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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

static void test_start_stop_log_lines(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char line[1024];
	const char *logpath;
	char *argv[] = { "sleep", "30", NULL };

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "alog") == GROK_OK,
		 "open");
	logpath = grok_supervisor_action_log_path(s);
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "last start");
	t_expect(strstr(line, "\"kind\":\"start\"") != NULL, "kind start");
	t_expect(strstr(line, "\"agent\":\"agent-a\"") != NULL, "agent a");
	t_expect(strstr(line, "pid=") != NULL, "pid detail");

	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "last stop");
	t_expect(strstr(line, "\"kind\":\"stop\"") != NULL, "kind stop");
	t_expect(count_lines(logpath) >= 2, "at least 2 lines");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_log_json_escape_and_system(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char line[1024];

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "esc") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_log(s, NULL, "note", "say \"hi\" \\ ok"), GROK_OK, "log");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "last");
	t_expect(strstr(line, "\\\"hi\\\"") != NULL, "escaped quotes");
	t_expect(strstr(line, "\\\\") != NULL, "escaped backslash");
	t_expect(strstr(line, "\"agent\":\"\"") != NULL, "empty agent system");

	t_expect_eq(grok_supervisor_log(s, "bad id", "x", "y"), GROK_ERR_INVAL, "bad agent");
	t_expect_eq(grok_supervisor_log(s, "agent-a", "", "y"), GROK_ERR_INVAL, "empty kind");
	t_expect_eq(grok_supervisor_log(s, "agent-a", NULL, "y"), GROK_ERR_INVAL, "null kind");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_log_append_order(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char line[1024];
	const char *logpath;
	int n;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "ord") == GROK_OK,
		 "open");
	logpath = grok_supervisor_action_log_path(s);
	t_expect_eq(grok_supervisor_log(s, "agent-a", "k1", "d1"), GROK_OK, "l1");
	t_expect_eq(grok_supervisor_log(s, "agent-a", "k2", "d2"), GROK_OK, "l2");
	t_expect_eq(grok_supervisor_log(s, "agent-b", "k3", "d3"), GROK_OK, "l3");
	n = count_lines(logpath);
	t_expect_eq(n, 3, "three lines");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "last");
	t_expect(strstr(line, "\"kind\":\"k3\"") != NULL, "last is k3");
	t_expect(strstr(line, "agent-b") != NULL, "last agent b");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_idempotent_stop_logged(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char line[1024];
	char *argv[] = { "true", NULL };
	int tries;
	grok_agent_status_t st;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "idemp") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start");
	for (tries = 0; tries < 100; tries++) {
		grok_supervisor_status(s, "agent-a", &st);
		if (st.state != GROK_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop idle");
	t_expect_eq(grok_supervisor_log_last(s, line, sizeof(line)), GROK_OK, "last");
	t_expect(strstr(line, "idempotent") != NULL || strstr(line, "\"kind\":\"stop\"") != NULL,
		 "stop logged");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_action_log_suite(void)
{
	t_run("start_stop_log_lines", test_start_stop_log_lines);
	t_run("log_json_escape_and_system", test_log_json_escape_and_system);
	t_run("log_append_order", test_log_append_order);
	t_run("idempotent_stop_logged", test_idempotent_stop_logged);
}
