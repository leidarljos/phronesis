/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_invalid_ids_and_args(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char *argv_ok[] = { "true", NULL };
	char long_id[GROK_ID_MAX + 8];
	grok_agent_status_t st;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "inval") == GROK_OK,
		 "open");

	t_expect_eq(grok_supervisor_start(s, "", NULL, NULL, argv_ok), GROK_ERR_INVAL, "empty id");
	t_expect_eq(grok_supervisor_start(s, "bad/id", NULL, NULL, argv_ok), GROK_ERR_INVAL, "slash id");
	t_expect_eq(grok_supervisor_start(s, "bad id", NULL, NULL, argv_ok), GROK_ERR_INVAL, "space id");
	t_expect_eq(grok_supervisor_start(s, "bad.id", NULL, NULL, argv_ok), GROK_ERR_INVAL, "dot id");
	memset(long_id, 'a', sizeof(long_id));
	long_id[GROK_ID_MAX] = '\0';
	t_expect_eq(grok_supervisor_start(s, long_id, NULL, NULL, argv_ok), GROK_ERR_INVAL, "long id");

	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, NULL), GROK_ERR_INVAL, "null argv");
	t_expect_eq(grok_supervisor_start(s, "agent-a", "bad mode", NULL, argv_ok), GROK_ERR_INVAL,
		    "mode space");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, "/tmp/ws with space", argv_ok),
		    GROK_ERR_INVAL, "workspace space");

	t_expect_eq(grok_supervisor_status(s, "agent-a", NULL), GROK_ERR_INVAL, "null status out");
	t_expect_eq(grok_supervisor_status(s, "nope", &st), GROK_ERR_NOTFOUND, "status missing");
	t_expect_eq(grok_supervisor_stop(s, "nope"), GROK_ERR_NOTFOUND, "stop missing");
	t_expect_eq(grok_supervisor_stop(NULL, "agent-a"), GROK_ERR_INVAL, "stop null sup");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_start_status_stop_happy(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "happy") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", "research", "/ws/proj", argv), GROK_OK,
		    "start");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "status");
	t_expect_eq((long)st.state, (long)GROK_AGENT_RUNNING, "running");
	t_expect(st.pid > 1, "pid");
	t_expect_eq((long)st.pid, (long)st.pgid, "leader pgid==pid");
	t_expect_streq(st.mode, "research", "mode");
	t_expect_streq(st.workspace, "/ws/proj", "workspace");
	pid = st.pid;
	t_expect(t_pid_alive(pid), "alive");

	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_ERR_EXISTS,
		    "duplicate");

	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop");
	t_expect(!t_pid_alive(pid), "dead after stop");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "status stopped");
	t_expect_eq((long)st.state, (long)GROK_AGENT_STOPPED, "stopped");
	t_expect_eq((long)st.pid, 0, "pid cleared");

	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "idempotent stop");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_default_mode_and_restart(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "sleep", "60", NULL };

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "restart") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start1");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "st1");
	t_expect_streq(st.mode, "develop", "default mode");
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop1");
	t_expect_eq(grok_supervisor_start(s, "agent-a", "focus", NULL, argv), GROK_OK, "start2");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "st2");
	t_expect_eq((long)st.state, (long)GROK_AGENT_RUNNING, "running again");
	t_expect_streq(st.mode, "focus", "new mode");
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop2");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_natural_exit_reaped(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "true", NULL };
	int tries;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "exit") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start true");
	for (tries = 0; tries < 100; tries++) {
		t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "poll");
		if (st.state != GROK_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	t_expect_eq((long)st.state, (long)GROK_AGENT_STOPPED, "reaped stopped");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK,
		    "restart after natural exit");
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_exec_failure_child(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "/no/such/grok-policyd-agent-bin-xyz", NULL };
	int tries;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "execfail") == GROK_OK,
		 "open");
	/* fork succeeds; child exits 127 on exec failure */
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start");
	for (tries = 0; tries < 100; tries++) {
		t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "poll");
		if (st.state != GROK_AGENT_RUNNING)
			break;
		usleep(10 * 1000);
	}
	t_expect_eq((long)st.state, (long)GROK_AGENT_STOPPED, "failed child reaped");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_two_agents_independent(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t sa, sb;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pa, pb;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "two") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start a");
	t_expect_eq(grok_supervisor_start(s, "agent-b", NULL, NULL, argv), GROK_OK, "start b");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &sa), GROK_OK, "st a");
	t_expect_eq(grok_supervisor_status(s, "agent-b", &sb), GROK_OK, "st b");
	pa = sa.pid;
	pb = sb.pid;
	t_expect(pa != pb, "distinct pids");
	t_expect(sa.pgid != sb.pgid, "distinct pgids");

	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop a");
	t_expect(!t_pid_alive(pa), "a dead");
	t_expect(t_pid_alive(pb), "b still alive");
	t_expect_eq(grok_supervisor_status(s, "agent-b", &sb), GROK_OK, "st b2");
	t_expect_eq((long)sb.state, (long)GROK_AGENT_RUNNING, "b running");

	t_expect_eq(grok_supervisor_stop(s, "agent-b"), GROK_OK, "stop b");
	t_expect(!t_pid_alive(pb), "b dead");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_lifecycle_suite(void)
{
	t_run("invalid_ids_and_args", test_invalid_ids_and_args);
	t_run("start_status_stop_happy", test_start_status_stop_happy);
	t_run("default_mode_and_restart", test_default_mode_and_restart);
	t_run("natural_exit_reaped", test_natural_exit_reaped);
	t_run("exec_failure_child", test_exec_failure_child);
	t_run("two_agents_independent", test_two_agents_independent);
}
