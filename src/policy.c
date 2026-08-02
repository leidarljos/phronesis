/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Domain checks for interface Policyd.check / admit.
 * Outcomes are Decision values (deny/allow/prompt), not C errno.
 */
#include "internal.h"
#include "policy.capnp.h"
#include "util.capnp.h"

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

/** CLI/tests: map admit.kind string → tool/action tokens (legacy). */
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

static int copy_text(char *dst, size_t n, capn_text t)
{
	size_t l;

	if (!dst || n == 0 || t.len < 0)
		return -1;
	l = (size_t)t.len;
	if (l > 0 && !t.str)
		return -1;
	if (l >= n)
		return -1;
	if (l)
		memcpy(dst, t.str, l);
	dst[l] = '\0';
	return 0;
}

static const char *token_base(const char *tok)
{
	const char *s;

	if (!tok || !tok[0])
		return tok;
	s = strrchr(tok, '/');
	return (s && s[1]) ? s + 1 : tok;
}

static int is_python_interpreter(const char *tok)
{
	const char *b;

	if (!tok || !tok[0])
		return 0;
	b = token_base(tok);
	if (strcmp(b, "python") == 0 || strcmp(b, "pypy") == 0 ||
	    strcmp(b, "pypy3") == 0)
		return 1;
	if (strncmp(b, "python", 6) != 0)
		return 0;
	return b[6] == '\0' || (b[6] >= '0' && b[6] <= '9');
}

static int ends_py(const char *tok)
{
	size_t n;

	if (!tok || tok[0] == '-')
		return 0;
	n = strlen(tok);
	return n >= 4 && strcmp(tok + n - 3, ".py") == 0;
}

static int tmp_from_text(capn_text t, char *tmp, size_t n)
{
	if (t.len < 0 || !t.str || (size_t)t.len >= n)
		return -1;
	memcpy(tmp, t.str, (size_t)t.len);
	tmp[(size_t)t.len] = '\0';
	return 0;
}

static int argv_has_uv_run(capn_ptr argv, int n)
{
	int i, j;
	capn_text empty = { 0, "", NULL };
	char tmp[GROK_PATH_MAX];

	for (i = 0; i < n; i++) {
		if (tmp_from_text(capn_get_text(argv, i, empty), tmp,
				  sizeof(tmp)) != 0)
			continue;
		if (strcmp(token_base(tmp), "uv") != 0)
			continue;
		for (j = i + 1; j < n; j++) {
			capn_text u = capn_get_text(argv, j, empty);

			if (u.str && u.len == 3 && memcmp(u.str, "run", 3) == 0)
				return 1;
		}
	}
	return 0;
}

static int argv_touches_python(capn_ptr argv, int n)
{
	int i;
	capn_text empty = { 0, "", NULL };
	char tmp[GROK_PATH_MAX];

	for (i = 0; i < n; i++) {
		if (tmp_from_text(capn_get_text(argv, i, empty), tmp,
				  sizeof(tmp)) != 0)
			continue;
		if (is_python_interpreter(tmp) || ends_py(tmp))
			return 1;
	}
	return 0;
}

static int argv_python_dash_c(capn_ptr argv, int n)
{
	int i;
	capn_text empty = { 0, "", NULL };
	char tmp[GROK_PATH_MAX];

	for (i = 0; i + 1 < n; i++) {
		capn_text n1;

		if (tmp_from_text(capn_get_text(argv, i, empty), tmp,
				  sizeof(tmp)) != 0)
			continue;
		if (!is_python_interpreter(tmp))
			continue;
		n1 = capn_get_text(argv, i + 1, empty);
		if (n1.str && n1.len == 2 && memcmp(n1.str, "-c", 2) == 0)
			return 1;
	}
	return 0;
}

static int argv_find_py(capn_ptr argv, int n, char *out, size_t out_n)
{
	int i;
	capn_text empty = { 0, "", NULL };
	char tmp[GROK_PATH_MAX];

	for (i = 0; i < n; i++) {
		if (tmp_from_text(capn_get_text(argv, i, empty), tmp,
				  sizeof(tmp)) != 0)
			continue;
		if (!ends_py(tmp))
			continue;
		if (strlen(tmp) + 1 > out_n)
			return -1;
		memcpy(out, tmp, strlen(tmp) + 1);
		return 0;
	}
	return 1;
}

static int pep723(const char *path)
{
	FILE *f;
	char line[512];
	int open_blk = 0;

	if (!path || !path[0])
		return 0;
	f = fopen(path, "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		size_t len;

		while (*p == ' ' || *p == '\t')
			p++;
		len = strlen(p);
		while (len && (p[len - 1] == '\n' || p[len - 1] == '\r'))
			p[--len] = '\0';
		if (!open_blk) {
			if (strcmp(p, "# /// script") == 0)
				open_blk = 1;
			continue;
		}
		if (strcmp(p, "# ///") == 0) {
			fclose(f);
			return 1;
		}
	}
	fclose(f);
	return 0;
}

static int resolve_script(const char *cwd, const char *script, char *out,
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

/** 1 = decision final; 0 = keep allow path. */
static int shell_python_gate(const char *cwd, capn_ptr argv,
			     grok_policy_result_t *out)
{
	int n, fr;
	char script[GROK_PATH_MAX], abs[GROK_PATH_MAX];

	if (argv.type == CAPN_NULL || argv.len <= 0)
		return 0;
	n = argv.len;
	if (!argv_touches_python(argv, n))
		return 0;
	if (!argv_has_uv_run(argv, n)) {
		out->decision = GROK_DECISION_DENY;
		snprintf(out->reason, sizeof(out->reason),
			 "python requires uv run + PEP 723");
		return 1;
	}
	if (argv_python_dash_c(argv, n)) {
		out->decision = GROK_DECISION_DENY;
		snprintf(out->reason, sizeof(out->reason),
			 "python -c denied; use uv run --script");
		return 1;
	}
	fr = argv_find_py(argv, n, script, sizeof(script));
	if (fr > 0)
		return 0;
	if (fr < 0 || resolve_script(cwd, script, abs, sizeof(abs)) != 0) {
		out->decision = GROK_DECISION_DENY;
		snprintf(out->reason, sizeof(out->reason),
			 "python script path unresolvable");
		return 1;
	}
	if (!pep723(abs)) {
		out->decision = GROK_DECISION_DENY;
		snprintf(out->reason, sizeof(out->reason),
			 "python missing PEP 723 metadata");
		return 1;
	}
	return 0;
}

static void decide_body(const char *workspace, const struct CheckBody *body,
			grok_policy_result_t *out)
{
	char path[GROK_PATH_MAX];
	char cwd[GROK_PATH_MAX];
	struct PathOp pop;
	struct ShellOp sop;
	struct RiskOp rop;

	out->decision = GROK_DECISION_DENY;
	snprintf(out->reason, sizeof(out->reason), "tools default deny");

	{
		const char *da = getenv("GROKOS_POLICYD_DENY_ALL");

		if (da && da[0] &&
		    (strcmp(da, "1") == 0 || strcmp(da, "true") == 0 ||
		     strcmp(da, "yes") == 0 || strcmp(da, "TRUE") == 0 ||
		     strcmp(da, "YES") == 0)) {
			snprintf(out->reason, sizeof(out->reason),
				 "deny-all (GROKOS_POLICYD_DENY_ALL)");
			return;
		}
	}

	switch (body->which) {
	case CheckBody_seat:
		switch (body->seat) {
		case SeatAction_publishRun:
		case SeatAction_readRun:
		case SeatAction_listRuns:
		case SeatAction_listEvents:
			out->decision = GROK_DECISION_ALLOW;
			snprintf(out->reason, sizeof(out->reason),
				 "seat board op allow (session plane ACL)");
			return;
		default:
			snprintf(out->reason, sizeof(out->reason),
				 "unknown seat action");
			return;
		}

	case CheckBody_model:
		out->decision = GROK_DECISION_ALLOW;
		snprintf(out->reason, sizeof(out->reason),
			 "model start admit plane");
		return;

	case CheckBody_path:
		read_PathOp(&pop, body->path);
		if (copy_text(path, sizeof(path), pop.path) != 0) {
			snprintf(out->reason, sizeof(out->reason),
				 "path too long");
			return;
		}
		if (pop.action == PathAction_delete) {
			out->decision = GROK_DECISION_PROMPT;
			snprintf(out->reason, sizeof(out->reason),
				 "high-risk action requires confirm");
			return;
		}
		if ((pop.action == PathAction_read ||
		     pop.action == PathAction_write) &&
		    path[0] && path_under_workspace(workspace, path)) {
			out->decision = GROK_DECISION_ALLOW;
			snprintf(out->reason, sizeof(out->reason),
				 "path under workspace allowlist");
			return;
		}
		if (path[0] && !path_under_workspace(workspace, path))
			snprintf(out->reason, sizeof(out->reason),
				 "path outside workspace (deny)");
		return;

	case CheckBody_shell:
		read_ShellOp(&sop, body->shell);
		if (copy_text(cwd, sizeof(cwd), sop.cwd) != 0) {
			snprintf(out->reason, sizeof(out->reason),
				 "cwd too long");
			return;
		}
		if (!cwd[0] || !path_under_workspace(workspace, cwd)) {
			if (cwd[0])
				snprintf(out->reason, sizeof(out->reason),
					 "path outside workspace (deny)");
			return;
		}
		if (shell_python_gate(cwd, sop.argv, out))
			return;
		out->decision = GROK_DECISION_ALLOW;
		snprintf(out->reason, sizeof(out->reason),
			 "shell exec under workspace allowlist");
		return;

	case CheckBody_risk:
		read_RiskOp(&rop, body->risk);
		if (rop.action == RiskAction_unset) {
			snprintf(out->reason, sizeof(out->reason),
				 "missing risk action");
			return;
		}
		out->decision = GROK_DECISION_PROMPT;
		snprintf(out->reason, sizeof(out->reason),
			 "high-risk action requires confirm");
		return;

	default:
		snprintf(out->reason, sizeof(out->reason),
			 "unknown check body");
		return;
	}
}

void grok_policy_decide_params(const char *workspace,
			       const struct CheckParams *params,
			       grok_policy_result_t *out)
{
	struct CheckBody body;

	memset(out, 0, sizeof(*out));
	out->decision = GROK_DECISION_DENY;
	if (!params) {
		snprintf(out->reason, sizeof(out->reason), "missing params");
		return;
	}
	if (params->body.p.type == CAPN_NULL) {
		snprintf(out->reason, sizeof(out->reason), "missing check body");
		return;
	}
	read_CheckBody(&body, params->body);
	decide_body(workspace, &body, out);
}

/* ---- CLI string bridge (maps into same decide_body) -------------------- */

int grok_policy_eval(const char *workspace, const char *tool,
		     const char *action, const char *path,
		     grok_policy_result_t *out)
{
	struct CheckBody body;

	if (!out)
		return GROK_ERR_INVAL;
	memset(out, 0, sizeof(*out));
	out->decision = GROK_DECISION_DENY;

	if (!tool || !tool[0] || !action || !action[0]) {
		snprintf(out->reason, sizeof(out->reason), "missing tool/action");
		return GROK_OK;
	}

	memset(&body, 0, sizeof(body));

	/*
	 * Build a synthetic CheckBody without Cap'n pointers for simple
	 * seat/model/risk. path/shell need Cap'n segments — CLI uses
	 * path-only string form without argv content gate.
	 */
	if (strcmp(tool, "seat") == 0) {
		body.which = CheckBody_seat;
		if (strcmp(action, "publish_run") == 0)
			body.seat = SeatAction_publishRun;
		else if (strcmp(action, "read_run") == 0)
			body.seat = SeatAction_readRun;
		else if (strcmp(action, "list_runs") == 0)
			body.seat = SeatAction_listRuns;
		else if (strcmp(action, "list_events") == 0)
			body.seat = SeatAction_listEvents;
		else {
			snprintf(out->reason, sizeof(out->reason),
				 "unknown seat action");
			return GROK_OK;
		}
		decide_body(workspace, &body, out);
		return GROK_OK;
	}
	if (strcmp(tool, "model") == 0 && strcmp(action, "start") == 0) {
		body.which = CheckBody_model;
		decide_body(workspace, &body, out);
		return GROK_OK;
	}
	/* path-like without Cap'n PathOp: inline lexical checks */
	if ((strcmp(action, "read") == 0 || strcmp(action, "write") == 0) &&
	    path && path[0] && path_under_workspace(workspace, path)) {
		out->decision = GROK_DECISION_ALLOW;
		snprintf(out->reason, sizeof(out->reason),
			 "path under workspace allowlist");
		return GROK_OK;
	}
	if (strcmp(tool, "shell") == 0 && strcmp(action, "exec") == 0 &&
	    path && path[0] && path_under_workspace(workspace, path)) {
		/* CLI path-only: no argv content gate */
		out->decision = GROK_DECISION_ALLOW;
		snprintf(out->reason, sizeof(out->reason),
			 "shell exec under workspace allowlist");
		return GROK_OK;
	}
	if (strcmp(action, "delete") == 0 || strcmp(action, "network") == 0 ||
	    strcmp(action, "sudo") == 0 ||
	    strcmp(action, "secret_export") == 0) {
		out->decision = GROK_DECISION_PROMPT;
		snprintf(out->reason, sizeof(out->reason),
			 "high-risk action requires confirm");
		return GROK_OK;
	}
	if (path && path[0] && !path_under_workspace(workspace, path))
		snprintf(out->reason, sizeof(out->reason),
			 "path outside workspace (deny)");
	else
		snprintf(out->reason, sizeof(out->reason),
			 "tools default deny");
	return GROK_OK;
}
