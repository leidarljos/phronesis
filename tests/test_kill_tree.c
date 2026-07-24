/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_tree_script(const char *path, const char *marker, int depth, int ignore_term)
{
	char body[2048];
	int n;

	if (depth <= 1) {
		n = snprintf(body, sizeof(body),
			     "#!/bin/sh\n"
			     "%s"
			     "sleep 120 &\n"
			     "echo $! > '%s'\n"
			     "wait\n",
			     ignore_term ? "trap '' TERM\n" : "",
			     marker);
	} else {
		/* Nested shell stays in same process group (no job control). */
		n = snprintf(body, sizeof(body),
			     "#!/bin/sh\n"
			     "%s"
			     "(\n"
			     "  sleep 120 &\n"
			     "  echo $! > '%s'\n"
			     "  wait\n"
			     ") &\n"
			     "wait\n",
			     ignore_term ? "trap '' TERM\n" : "",
			     marker);
	}
	if (n < 0 || (size_t)n >= sizeof(body))
		return -1;
	if (t_write_file(path, body) != 0)
		return -1;
	return chmod(path, 0700);
}

static void run_tree_case(const char *tag, int depth, int ignore_term)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char script[GROK_PATH_MAX], marker[GROK_PATH_MAX];
	char *argv[3];
	pid_t leader = 0, leaf = 0;
	grok_agent_status_t st;
	int tries;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), tag) == GROK_OK,
		 "open");
	snprintf(script, sizeof(script), "%s/tree.sh", runtime);
	snprintf(marker, sizeof(marker), "%s/leaf.pid", runtime);
	t_expect(write_tree_script(script, marker, depth, ignore_term) == 0, "script");

	argv[0] = "/bin/sh";
	argv[1] = script;
	argv[2] = NULL;
	t_expect_eq(grok_supervisor_start(s, "agent-tree", NULL, NULL, argv), GROK_OK, "start");
	t_expect_eq(grok_supervisor_status(s, "agent-tree", &st), GROK_OK, "status");
	leader = st.pid;
	t_expect(t_pid_alive(leader), "leader alive");

	t_expect(t_wait_file(marker, 2000) == 0, "leaf pid file");
	t_expect(t_read_pidfile(marker, &leaf) == 0, "read leaf");
	t_expect(leaf > 1 && leaf != leader, "leaf distinct");
	t_expect(t_pid_alive(leaf), "leaf alive before stop");

	t_expect_eq(grok_supervisor_stop(s, "agent-tree"), GROK_OK, "stop");
	for (tries = 0; tries < 100; tries++) {
		if (!t_pid_alive(leader) && !t_pid_alive(leaf))
			break;
		usleep(10 * 1000);
	}
	t_expect(!t_pid_alive(leader), "leader dead");
	t_expect(!t_pid_alive(leaf), "leaf dead (tree kill)");
	t_expect_eq(grok_supervisor_status(s, "agent-tree", &st), GROK_OK, "status after");
	t_expect_eq((long)st.state, (long)GROK_AGENT_STOPPED, "stopped");

	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_kill_one_level_grandchild(void)
{
	run_tree_case("kt1", 1, 0);
}

static void test_kill_nested_shell_tree(void)
{
	run_tree_case("kt2", 2, 0);
}

static void test_kill_sigterm_ignored_leader(void)
{
	/* Leader ignores SIGTERM; stop must escalate to SIGKILL. */
	run_tree_case("kt3", 1, 1);
}

static void test_stop_after_external_kill(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "sleep", "120", NULL };
	pid_t pid;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "extkill") == GROK_OK,
		 "open");
	t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "st");
	pid = st.pid;
	t_expect(kill(pid, SIGKILL) == 0, "external kill");
	usleep(50 * 1000);
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop after external");
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_OK, "st2");
	t_expect_eq((long)st.state, (long)GROK_AGENT_STOPPED, "stopped");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_thrash_start_stop(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char *argv[] = { "sleep", "30", NULL };
	int i;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "thrash") == GROK_OK,
		 "open");
	for (i = 0; i < 8; i++) {
		t_expect_eq(grok_supervisor_start(s, "agent-a", NULL, NULL, argv), GROK_OK, "start");
		t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_OK, "stop");
	}
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_kill_tree_suite(void)
{
	t_run("kill_one_level_grandchild", test_kill_one_level_grandchild);
	t_run("kill_nested_shell_tree", test_kill_nested_shell_tree);
	t_run("kill_sigterm_ignored_leader", test_kill_sigterm_ignored_leader);
	t_run("stop_after_external_kill", test_stop_after_external_kill);
	t_run("thrash_start_stop", test_thrash_start_stop);
}
