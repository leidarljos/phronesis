/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <stdio.h>
#include <string.h>

static int is_high_risk(const char *action)
{
	static const char *risk[] = {
		"delete", "unlink", "rm", "network", "connect", "bind",
		"pay", "auth", "login", "os_change", "secret_export",
		"privilege", "sudo", NULL
	};
	int i;

	if (!action || !action[0])
		return 0;
	for (i = 0; risk[i]; i++) {
		if (strcmp(action, risk[i]) == 0)
			return 1;
	}
	return 0;
}

/*
 * Lexical workspace allowlist (no realpath). Absolute paths only.
 * Rejects ".." components and "//" style noise so /ws/proj/../etc/passwd
 * cannot prefix-match /ws/proj. Symlink escape is still possible without
 * canonicalization — documented as stub limit in README.
 */
static int has_dotdot_component(const char *path)
{
	const char *p = path;

	if (!p)
		return 1;
	while (*p) {
		if (p[0] == '.' && p[1] == '.' &&
		    (p[2] == '/' || p[2] == '\0') &&
		    (p == path || p[-1] == '/'))
			return 1;
		p++;
	}
	return 0;
}

static int path_under_workspace(const char *workspace, const char *path)
{
	size_t wl;

	if (!workspace || !workspace[0] || !path || !path[0])
		return 0;
	if (path[0] != '/' || workspace[0] != '/')
		return 0;
	if (has_dotdot_component(path) || has_dotdot_component(workspace))
		return 0;
	wl = strlen(workspace);
	/* strip trailing slash on workspace for join rules */
	while (wl > 1 && workspace[wl - 1] == '/')
		wl--;
	if (strncmp(path, workspace, wl) != 0)
		return 0;
	if (path[wl] != '\0' && path[wl] != '/')
		return 0;
	return 1;
}

int grok_policy_eval(const char *workspace,
		     const char *tool,
		     const char *action,
		     const char *path,
		     grok_policy_result_t *out)
{
	if (!out)
		return GROK_ERR_INVAL;
	memset(out, 0, sizeof(*out));
	out->decision = GROK_DECISION_DENY;

	if (!tool || !tool[0]) {
		snprintf(out->reason, sizeof(out->reason), "missing tool");
		return GROK_OK;
	}
	if (!action || !action[0]) {
		snprintf(out->reason, sizeof(out->reason), "missing action");
		return GROK_OK;
	}

	if (is_high_risk(action)) {
		out->decision = GROK_DECISION_PROMPT;
		snprintf(out->reason, sizeof(out->reason), "high-risk action requires confirm");
		return GROK_OK;
	}

	/* Limited allow: read/write only under declared workspace root. */
	if (path && path[0] &&
	    (strcmp(action, "read") == 0 || strcmp(action, "write") == 0) &&
	    path_under_workspace(workspace, path)) {
		out->decision = GROK_DECISION_ALLOW;
		snprintf(out->reason, sizeof(out->reason), "path under workspace allowlist");
		return GROK_OK;
	}

	out->decision = GROK_DECISION_DENY;
	if (path && path[0] && !path_under_workspace(workspace, path))
		snprintf(out->reason, sizeof(out->reason), "path outside workspace (deny)");
	else
		snprintf(out->reason, sizeof(out->reason), "tools default deny");
	return GROK_OK;
}
