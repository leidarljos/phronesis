/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Domain eval helpers for Policyd methods.
 * Outcomes are Decision values (deny/allow/prompt), not C errno.
 */
#include "internal.h"
#include "policy_trace.h"

#include <capnp_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void grok_agent_id_to_hex(uint64_t hi, uint64_t lo, char out[GROK_ID_MAX])
{
	if (!out)
		return;
	if (hi == 0 && lo == 0) {
		out[0] = '\0';
		return;
	}
	snprintf(out, GROK_ID_MAX, "%016llx%016llx",
		 (unsigned long long)hi, (unsigned long long)lo);
}

int grok_policyd_map_admit_kind(const char *kind, const char **tool,
				const char **action)
{
	if (!kind || !tool || !action)
		return -1;
	if (strcmp(kind, "seat") == 0) {
		*tool = "seat";
		*action = "publish_run";
		return 0;
	}
	if (kind[0] == '\0' || strcmp(kind, "model") == 0 ||
	    strcmp(kind, "agent") == 0) {
		*tool = "model";
		*action = "start";
		return 0;
	}
	return -1;
}

static int is_clean_abs_path(const char *path)
{
	const char *p, *comp;

	if (!path || path[0] != '/')
		return 0;
	for (p = path; *p; p++) {
		if (p[0] == '/' && p[1] == '/')
			return 0;
		if (p[0] == '/') {
			comp = p + 1;
			if (comp[0] == '\0')
				break;
			if (comp[0] == '.' &&
			    (comp[1] == '/' || comp[1] == '\0'))
				return 0;
			if (comp[0] == '.' && comp[1] == '.' &&
			    (comp[2] == '/' || comp[2] == '\0'))
				return 0;
		}
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
	return path[wl] == '\0' || path[wl] == '/';
}

/*
 * Paths that must not be Read/viewed into model traces (credential material).
 * Lexical only — basename and path component match.
 */
static int path_is_sensitive(const char *path)
{
	const char *base;
	const char *p;

	if (!path || !path[0])
		return 0;
	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (strcmp(base, ".env") == 0 ||
	    strncmp(base, ".env.", 5) == 0 ||
	    strcmp(base, ".netrc") == 0 ||
	    strcmp(base, ".npmrc") == 0 ||
	    strcmp(base, ".pypirc") == 0 ||
	    strcmp(base, "id_rsa") == 0 ||
	    strcmp(base, "id_ed25519") == 0 ||
	    strcmp(base, "id_ecdsa") == 0 ||
	    strcmp(base, "id_dsa") == 0 ||
	    strcmp(base, "credentials") == 0 ||
	    strcmp(base, "credentials.json") == 0 ||
	    strcmp(base, "service-account.json") == 0 ||
	    strcmp(base, "token") == 0 ||
	    strcmp(base, "secrets.yaml") == 0 ||
	    strcmp(base, "secrets.yml") == 0 ||
	    strcmp(base, "secrets.json") == 0)
		return 1;
	/* Path components */
	if (strstr(path, "/.ssh/") || strstr(path, "/.gnupg/") ||
	    strstr(path, "/.aws/") || strstr(path, "/.kube/") ||
	    strstr(path, "/.docker/") || strstr(path, "/.password-store/") ||
	    strstr(path, "/.config/gcloud/") ||
	    strstr(path, "/private_dot_") || strstr(path, "/.vault/"))
		return 1;
	/* ends with .pem / .key */
	p = strrchr(path, '.');
	if (p && (strcmp(p, ".pem") == 0 || strcmp(p, ".key") == 0 ||
		  strcmp(p, ".p12") == 0 || strcmp(p, ".pfx") == 0))
		return 1;
	(void)base;
	return 0;
}

int grok_policy_resolve_script(const char *cwd, const char *script, char *out,
			       size_t out_n)
{
	size_t cl, sl;

	if (!script || !script[0] || !out || !out_n)
		return -1;
	if (script[0] == '/') {
		if (strlen(script) + 1 > out_n)
			return -1;
		memcpy(out, script, strlen(script) + 1);
		return 0;
	}
	if (!cwd || cwd[0] != '/')
		return -1;
	cl = strlen(cwd);
	sl = strlen(script);
	while (cl > 1 && cwd[cl - 1] == '/')
		cl--;
	if (cl + 1 + sl + 1 > out_n)
		return -1;
	memcpy(out, cwd, cl);
	out[cl] = '/';
	memcpy(out + cl + 1, script, sl + 1);
	return 0;
}

/** Truthy env for TCB gates: 1 / true / yes (any case of true/yes). */
static int env_truthy(const char *name)
{
	const char *v = getenv(name);

	if (!v || !v[0])
		return 0;
	return strcmp(v, "1") == 0 || strcmp(v, "true") == 0 ||
	       strcmp(v, "yes") == 0 || strcmp(v, "TRUE") == 0 ||
	       strcmp(v, "YES") == 0;
}

int grok_policy_eval(const char *workspace, const char *tool,
		     const char *action, const char *path,
		     grok_policy_result_t *out)
{
	if (!out)
		return GROK_ERR_INVAL;
	grok_policy_result_set(out, GROK_DECISION_DENY,
			       GROK_REASON_TOOLS_DEFAULT_DENY);

	PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_ENTER,
		       "policy-eval", tool && tool[0] ? tool : "", -1, NULL, 0);

	if (env_truthy("GROKOS_POLICYD_DENY_ALL")) {
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_DENY_ALL);
				PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_DECIDE,
			       "policy-eval/deny-all", "GROKOS_POLICYD_DENY_ALL",
			       (int)GROK_REASON_DENY_ALL, "deny", 1);
		return GROK_OK;
	}

	if (!tool || !tool[0] || !action || !action[0]) {
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_MISSING_TOOL_ACTION);
		return GROK_OK;
	}

	if (strcmp(tool, "seat") == 0) {
		if (strcmp(action, "publish_run") == 0 ||
		    strcmp(action, "read_run") == 0 ||
		    strcmp(action, "list_runs") == 0 ||
		    strcmp(action, "list_events") == 0) {
			grok_policy_result_set(out, GROK_DECISION_ALLOW,
					       GROK_REASON_SEAT_BOARD_ALLOW);
			return GROK_OK;
		}
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_UNKNOWN_SEAT_ACTION);
		return GROK_OK;
	}
	if (strcmp(tool, "model") == 0 && strcmp(action, "start") == 0) {
		grok_policy_result_set(out, GROK_DECISION_ALLOW,
				       GROK_REASON_MODEL_START_ALLOW);
		return GROK_OK;
	}
	/* Secrets must never leave the seat — deny, not prompt. */
	if (strcmp(action, "secret_export") == 0) {
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_SECRET_EXPORT_DENIED);
		return GROK_OK;
	}
	if (strcmp(action, "delete") == 0 || strcmp(action, "network") == 0 ||
	    strcmp(action, "sudo") == 0) {
		grok_policy_result_set(out, GROK_DECISION_PROMPT,
				       GROK_REASON_HIGH_RISK_PROMPT);
		return GROK_OK;
	}
	if ((strcmp(action, "read") == 0 || strcmp(action, "write") == 0) &&
	    path && path[0] && path_is_sensitive(path)) {
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_PATH_SENSITIVE_DENY);
		return GROK_OK;
	}
	if ((strcmp(action, "read") == 0 || strcmp(action, "write") == 0) &&
	    path && path[0] && path_under_workspace(workspace, path)) {
		grok_policy_result_set(out, GROK_DECISION_ALLOW,
				       GROK_REASON_PATH_UNDER_WORKSPACE_ALLOW);
		return GROK_OK;
	}
	if (strcmp(tool, "shell") == 0 && strcmp(action, "exec") == 0 &&
	    path && path[0] && path_under_workspace(workspace, path)) {
		grok_policy_result_set(out, GROK_DECISION_ALLOW,
				       GROK_REASON_SHELL_EXEC_ALLOW);
		return GROK_OK;
	}
	if (path && path[0] && !path_under_workspace(workspace, path))
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_PATH_OUTSIDE_WORKSPACE);
	else
		grok_policy_result_set(out, GROK_DECISION_DENY,
				       GROK_REASON_TOOLS_DEFAULT_DENY);
	return GROK_OK;
}
