/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Minimal C consumer of the grok-policyd stable ABI.
 * Build: pixi run example  (links build/libgrok_policyd.a)
 */
#include "grok-policyd/supervisor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void rm_tree(const char *path)
{
	char cmd[640];

	if (!path || !path[0])
		return;
	/* Example-only cleanup; not a library API. */
	snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);
	(void)system(cmd);
}

static int make_dirs(char *state, size_t sn, char *runtime, size_t rn)
{
	/* Prefer a non-/tmp scratch root (runtime under /tmp is rejected). */
	const char *base = getenv("GROK_POLICYD_EXAMPLE_ROOT");
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
	grok_supervisor_t *sup = NULL;
	char *argv[] = { "true", NULL };
	char state_dir[512];
	char runtime_dir[512];
	int rc;
	int exit_code = 0;

	printf("grok-policyd %s (api %d)\n",
	       grok_policyd_version_string(),
	       grok_policyd_api_version());

	if (make_dirs(state_dir, sizeof(state_dir), runtime_dir, sizeof(runtime_dir)) != 0) {
		fprintf(stderr,
			"mkdtemp failed (need writable /var/tmp or GROK_POLICYD_EXAMPLE_ROOT)\n");
		return 1;
	}

	rc = grok_supervisor_open(&sup, state_dir, runtime_dir);
	if (rc != GROK_OK) {
		fprintf(stderr, "open failed: %d\n", rc);
		exit_code = 1;
		goto cleanup;
	}

	rc = grok_supervisor_start(sup, "ex-agent", "demo", state_dir, argv);
	if (rc != GROK_OK && rc != GROK_ERR_EXISTS) {
		fprintf(stderr, "start failed: %d\n", rc);
		exit_code = 1;
		goto cleanup;
	}

	{
		grok_agent_status_t st;

		memset(&st, 0, sizeof(st));
		if (grok_supervisor_status(sup, "ex-agent", &st) == GROK_OK)
			printf("agent %s state=%d pid=%d\n", st.id, (int)st.state,
			       (int)st.pid);
	}

	(void)grok_supervisor_stop(sup, "ex-agent");
	printf("example ok state=%s\n", state_dir);

cleanup:
	if (sup)
		grok_supervisor_close(sup);
	rm_tree(state_dir);
	rm_tree(runtime_dir);
	return exit_code;
}
