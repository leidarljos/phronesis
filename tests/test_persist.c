/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_slot_survives_reopen(void)
{
	grok_supervisor_t *s1 = NULL, *s2 = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	grok_agent_status_t st;
	char *argv[] = { "sleep", "60", NULL };
	char slot[GROK_PATH_MAX];
	pid_t pid;

	t_expect(t_tmpdir(state, sizeof(state), "gp-p-st") == 0, "state");
	t_expect(t_tmpdir(runtime, sizeof(runtime), "gp-p-rt") == 0, "runtime");
	t_expect_eq(grok_supervisor_open(&s1, state, runtime), GROK_OK, "open1");
	t_expect_eq(grok_supervisor_start(s1, "agent-a", "develop", "/ws", argv), GROK_OK, "start");
	t_expect_eq(grok_supervisor_status(s1, "agent-a", &st), GROK_OK, "st1");
	pid = st.pid;
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", runtime);
	t_expect(access(slot, F_OK) == 0, "slot file exists");
	grok_supervisor_close(s1);

	t_expect_eq(grok_supervisor_open(&s2, state, runtime), GROK_OK, "open2");
	t_expect_eq(grok_supervisor_status(s2, "agent-a", &st), GROK_OK, "st2");
	t_expect_eq((long)st.state, (long)GROK_AGENT_RUNNING, "still running");
	t_expect_eq((long)st.pid, (long)pid, "same pid");
	t_expect_streq(st.mode, "develop", "mode persisted");
	t_expect_streq(st.workspace, "/ws", "workspace persisted");
	t_expect(t_pid_alive(pid), "process still up");
	t_expect_eq(grok_supervisor_stop(s2, "agent-a"), GROK_OK, "stop via second handle");
	t_expect(!t_pid_alive(pid), "killed via reopened supervisor");
	grok_supervisor_close(s2);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_corrupt_slot(void)
{
	grok_supervisor_t *s = NULL;
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char slot[GROK_PATH_MAX];
	grok_agent_status_t st;

	t_expect(t_open_pair(&s, state, sizeof(state), runtime, sizeof(runtime), "corrupt") == GROK_OK,
		 "open");
	snprintf(slot, sizeof(slot), "%s/agents/agent-a.slot", runtime);
	t_expect(t_write_file(slot, "not-a-valid-slot\n") == 0, "write junk");
	/* load from disk fails parse → IO */
	t_expect_eq(grok_supervisor_status(s, "agent-a", &st), GROK_ERR_IO, "corrupt status");
	t_expect_eq(grok_supervisor_stop(s, "agent-a"), GROK_ERR_IO, "corrupt stop");
	grok_supervisor_close(s);
	t_rm_rf(state);
	t_rm_rf(runtime);
}

static void test_cli_subprocess_roundtrip(void)
{
	char state[GROK_PATH_MAX], runtime[GROK_PATH_MAX];
	char cmd[1024];
	int rc;
	const char *bin = "build/grok-policyd";

	if (access(bin, X_OK) != 0) {
		/* suite may run before cli linked; skip soft */
		fprintf(stderr, "  skip: %s not built\n", bin);
		return;
	}
	t_expect(t_tmpdir(state, sizeof(state), "gp-cli-st") == 0, "state");
	t_expect(t_tmpdir(runtime, sizeof(runtime), "gp-cli-rt") == 0, "runtime");

	snprintf(cmd, sizeof(cmd),
		 "%s --state-dir '%s' --runtime-dir '%s' start agent-cli -- sleep 60",
		 bin, state, runtime);
	rc = system(cmd);
	t_expect_eq(rc, 0, "cli start");

	snprintf(cmd, sizeof(cmd),
		 "%s --state-dir '%s' --runtime-dir '%s' status agent-cli | grep -q running",
		 bin, state, runtime);
	rc = system(cmd);
	t_expect_eq(rc, 0, "cli status running");

	snprintf(cmd, sizeof(cmd),
		 "%s --state-dir '%s' --runtime-dir '%s' stop agent-cli",
		 bin, state, runtime);
	rc = system(cmd);
	t_expect_eq(rc, 0, "cli stop");

	snprintf(cmd, sizeof(cmd),
		 "%s --state-dir '%s' --runtime-dir '%s' status agent-cli | grep -q stopped",
		 bin, state, runtime);
	rc = system(cmd);
	t_expect_eq(rc, 0, "cli status stopped");

	t_rm_rf(state);
	t_rm_rf(runtime);
}

void test_persist_suite(void)
{
	t_run("slot_survives_reopen", test_slot_survives_reopen);
	t_run("corrupt_slot", test_corrupt_slot);
	t_run("cli_subprocess_roundtrip", test_cli_subprocess_roundtrip);
}
