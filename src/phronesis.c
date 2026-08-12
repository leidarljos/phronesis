/* SPDX-License-Identifier: Apache-2.0 */
#include "phronesis/supervisor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
	fprintf(stderr,
		"usage:\n"
		"  %s [--state-dir DIR] [--runtime-dir DIR] start <agent-id> -- <cmd> [args...]\n"
		"  %s [--state-dir DIR] [--runtime-dir DIR] status <agent-id>\n"
		"  %s [--state-dir DIR] [--runtime-dir DIR] stop <agent-id>\n"
		"  %s [--state-dir DIR] [--runtime-dir DIR] log <agent-id> <kind> <detail>\n"
		"  %s [--state-dir DIR] [--runtime-dir DIR] check <agent-id> <tool> <action> [path]\n",
		argv0, argv0, argv0, argv0, argv0);
}

static const char *state_name(policyd_agent_state_t st)
{
	switch (st) {
	case POLICYD_AGENT_RUNNING:
		return "running";
	case POLICYD_AGENT_FAILED:
		return "failed";
	case POLICYD_AGENT_STOPPED:
	default:
		return "stopped";
	}
}

int main(int argc, char **argv)
{
	const char *state_dir = NULL;
	const char *runtime_dir = NULL;
	const char *cmd;
	policyd_supervisor_t *sup = NULL;
	int i = 1;
	int rc;
	int exit_code = 0;

	while (i < argc && argv[i][0] == '-') {
		if (strcmp(argv[i], "--state-dir") == 0 && i + 1 < argc) {
			state_dir = argv[++i];
			i++;
		} else if (strcmp(argv[i], "--runtime-dir") == 0 && i + 1 < argc) {
			runtime_dir = argv[++i];
			i++;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}
	if (i >= argc) {
		usage(argv[0]);
		return 2;
	}
	cmd = argv[i++];

	rc = policyd_supervisor_open(&sup, state_dir, runtime_dir);
	if (rc != POLICYD_OK) {
		fprintf(stderr, "open failed: %d\n", rc);
		return 1;
	}

	if (strcmp(cmd, "start") == 0) {
		const char *id;
		int dash = -1;
		int j;

		if (i >= argc) {
			usage(argv[0]);
			exit_code = 2;
			goto out;
		}
		id = argv[i++];
		for (j = i; j < argc; j++) {
			if (strcmp(argv[j], "--") == 0) {
				dash = j;
				break;
			}
		}
		if (dash < 0 || dash + 1 >= argc) {
			fprintf(stderr, "start requires: start <id> -- <cmd>...\n");
			exit_code = 2;
			goto out;
		}
		rc = policyd_supervisor_start(sup, id, "develop", NULL, &argv[dash + 1]);
		if (rc != POLICYD_OK) {
			fprintf(stderr, "start failed: %d\n", rc);
			exit_code = 1;
			goto out;
		}
		printf("started %s\n", id);
	} else if (strcmp(cmd, "status") == 0) {
		policyd_agent_status_t st;

		if (i >= argc) {
			usage(argv[0]);
			exit_code = 2;
			goto out;
		}
		rc = policyd_supervisor_status(sup, argv[i], &st);
		if (rc != POLICYD_OK) {
			fprintf(stderr, "status failed: %d\n", rc);
			exit_code = 1;
			goto out;
		}
		printf("id=%s state=%s pid=%d pgid=%d mode=%s\n",
		       st.id, state_name(st.state), (int)st.pid, (int)st.pgid, st.mode);
	} else if (strcmp(cmd, "stop") == 0) {
		if (i >= argc) {
			usage(argv[0]);
			exit_code = 2;
			goto out;
		}
		rc = policyd_supervisor_stop(sup, argv[i]);
		if (rc != POLICYD_OK) {
			fprintf(stderr, "stop failed: %d\n", rc);
			exit_code = 1;
			goto out;
		}
		printf("stopped %s\n", argv[i]);
	} else if (strcmp(cmd, "log") == 0) {
		if (i + 2 >= argc) {
			usage(argv[0]);
			exit_code = 2;
			goto out;
		}
		rc = policyd_supervisor_log(sup, argv[i], argv[i + 1], argv[i + 2]);
		if (rc != POLICYD_OK) {
			fprintf(stderr, "log failed: %d\n", rc);
			exit_code = 1;
			goto out;
		}
	} else if (strcmp(cmd, "check") == 0) {
		policyd_policy_result_t pr;
		const char *path = NULL;
		const char *dec;

		if (i + 2 >= argc) {
			usage(argv[0]);
			exit_code = 2;
			goto out;
		}
		if (i + 3 < argc)
			path = argv[i + 3];
		rc = policyd_policy_check(sup, argv[i], argv[i + 1], argv[i + 2], path, &pr);
		if (rc != POLICYD_OK) {
			fprintf(stderr, "check failed: %d\n", rc);
			exit_code = 1;
			goto out;
		}
		switch (pr.decision) {
		case POLICYD_DECISION_ALLOW:
			dec = "allow";
			break;
		case POLICYD_DECISION_PROMPT:
			dec = "prompt";
			break;
		default:
			dec = "deny";
			break;
		}
		printf("decision=%s reason=%s\n", dec, pr.reason);
		exit_code = (pr.decision == POLICYD_DECISION_DENY) ? 2 : 0;
	} else {
		usage(argv[0]);
		exit_code = 2;
	}

out:
	policyd_supervisor_close(sup);
	return exit_code;
}
