/* SPDX-License-Identifier: MIT */
/*
 * Minimal C consumer of the phronesis stable ABI.
 * Build: pixi run example  (links build/libphronesis.a)
 */
#include "phronesis/supervisor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void rm_tree(const char *path)
{
	char cmd[640];
	int st;

	if (!path || !path[0])
		return;
	/* Example-only cleanup; not a library API. */
	snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);
	st = system(cmd);
	if (st != 0)
		fprintf(stderr, "warning: cleanup %s failed (status %d)\n", path, st);
}

static int make_dirs(char *state, size_t sn, char *runtime, size_t rn)
{
	/* Prefer a non-/tmp scratch root (runtime under /tmp is rejected). */
	const char *base = getenv("PHRONESIS_EXAMPLE_ROOT");
	char tmpl_s[256];
	char tmpl_r[256];
	char *s;
	char *r;

	if (!base || !base[0])
		base = "/var/tmp";
	snprintf(tmpl_s, sizeof(tmpl_s), "%s/gp-ex-st-XXXXXX", base);
	snprintf(tmpl_r, sizeof(tmpl_r), "%s/gp-ex-rt-XXXXXX", base);
	s = mkdtemp(tmpl_s);
	r = mkdtemp(tmpl_r);
	if (!s || !r)
		return -1;
	if (snprintf(state, sn, "%s", s) >= (int)sn)
		return -1;
	if (snprintf(runtime, rn, "%s", r) >= (int)rn)
		return -1;
	return 0;
}

int main(void)
{
	phronesis_supervisor_t *sup = NULL;
	char *argv[] = { "true", NULL };
	char state_dir[512];
	char runtime_dir[512];
	int rc;
	int exit_code = 0;

	printf("phronesis %s (api %d)\n",
	       phronesis_version_string(),
	       phronesis_api_version());

	if (make_dirs(state_dir, sizeof(state_dir), runtime_dir, sizeof(runtime_dir)) != 0) {
		fprintf(stderr,
			"mkdtemp failed (need writable /var/tmp or PHRONESIS_EXAMPLE_ROOT)\n");
		return 1;
	}

	rc = phronesis_supervisor_open(&sup, state_dir, runtime_dir);
	if (rc != PHRONESIS_OK) {
		fprintf(stderr, "open failed: %d\n", rc);
		exit_code = 1;
		goto cleanup;
	}

	rc = phronesis_supervisor_start(sup, "ex-agent", "demo", state_dir, argv);
	if (rc != PHRONESIS_OK && rc != PHRONESIS_ERR_EXISTS) {
		fprintf(stderr, "start failed: %d\n", rc);
		exit_code = 1;
		goto cleanup;
	}

	{
		phronesis_agent_status_t st;

		memset(&st, 0, sizeof(st));
		if (phronesis_supervisor_status(sup, "ex-agent", &st) == PHRONESIS_OK)
			printf("agent %s state=%d pid=%d\n", st.id, (int)st.state,
			       (int)st.pid);
	}

	(void)phronesis_supervisor_stop(sup, "ex-agent");
	printf("example ok state=%s\n", state_dir);

cleanup:
	if (sup)
		phronesis_supervisor_close(sup);
	rm_tree(state_dir);
	rm_tree(runtime_dir);
	return exit_code;
}
