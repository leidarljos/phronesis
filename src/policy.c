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
 * Lexical workspace allowlist (no realpath — non-existent paths must still
 * evaluate for write-to-new-file). Absolute paths only.
 *
 * Rejects: relative paths, empty path components (//), "." components,
 * ".." components. Symlink escape past workspace remains open without
 * canonicalization (README stub limit).
 */
static int is_clean_abs_path(const char *path)
{
	const char *p;
	const char *comp;

	if (!path || path[0] != '/')
		return 0;
	/* no interior NUL issues beyond C string; reject empty components and . / .. */
	p = path;
	while (*p) {
		if (p[0] == '/' && p[1] == '/')
			return 0; /* empty component */
		if (p[0] == '/') {
			comp = p + 1;
			if (comp[0] == '\0')
				break; /* trailing slash OK */
			if (comp[0] == '.' && (comp[1] == '/' || comp[1] == '\0'))
				return 0; /* "." */
			if (comp[0] == '.' && comp[1] == '.' &&
			    (comp[2] == '/' || comp[2] == '\0'))
				return 0; /* ".." */
		}
		p++;
	}
	return 1;
}

static int path_under_workspace(const char *workspace, const char *path)
{
	size_t wl;

	if (!workspace || !workspace[0] || !path || !path[0])
		return 0;
	if (!is_clean_abs_path(workspace) || !is_clean_abs_path(path))
		return 0;

	wl = strlen(workspace);
	while (wl > 1 && workspace[wl - 1] == '/')
		wl--;
	if (strncmp(path, workspace, wl) != 0)
		return 0;
	/* next char must end path or be a separator (prevents /ws/proj vs /ws/projevil) */
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
