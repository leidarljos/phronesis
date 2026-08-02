/* SPDX-License-Identifier: Apache-2.0 */
/*
 * interface Policyd: one Cap'n method per entry point.
 * Params message root in, result message root out. Always PolicyDecision for
 * checks (fail-closed deny on bad input). No unions, no Decision-in-int.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "internal.h"
#include "policy.capnp.h"
#include "util.capnp.h"

#include <capnp_c.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static capn_text ctext(const char *s)
{
	capn_text t;
	size_t len;

	if (!s)
		s = "";
	len = strlen(s);
	if (len > (size_t)INT_MAX)
		len = (size_t)INT_MAX;
	t.len = (int)len;
	t.str = s;
	t.seg = NULL;
	return t;
}

static int write_msg(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf = NULL;
	size_t cap = 8192U;
	const size_t cap_max = 1024U * 1024U;
	int64_t n;

	if (!out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	for (;;) {
		if (!cap || cap > cap_max)
			return -1;
		buf = malloc(cap);
		if (!buf)
			return -1;
		n = capn_write_mem(c, buf, cap, 0);
		if (n >= 0)
			break;
		free(buf);
		if (cap > cap_max / 2U)
			return -1;
		cap *= 2U;
	}
	if ((uint64_t)n > (uint64_t)GROK_POLICY_CAPNP_MAX_BODY) {
		free(buf);
		return -1;
	}
	*out = buf;
	*out_len = (size_t)n;
	return 0;
}

static AgentId_ptr put_agent(struct capn_segment *seg, uint64_t hi, uint64_t lo)
{
	struct AgentId id = { .hi = hi, .lo = lo };
	AgentId_ptr p = new_AgentId(seg);

	write_AgentId(&id, p);
	return p;
}

static void read_agent(AgentId_ptr p, struct AgentId *out)
{
	if (p.p.type == CAPN_NULL) {
		out->hi = 0;
		out->lo = 0;
		return;
	}
	read_AgentId(out, p);
}

static const char *workspace_for(grok_supervisor_t *sup, struct AgentId id)
{
	char hex[GROK_ID_MAX];
	grok_agent_status_t st;

	grok_agent_id_to_hex(id.hi, id.lo, hex);
	if (!hex[0])
		return NULL;
	if (grok_supervisor_status(sup, hex, &st) != GROK_OK)
		return NULL;
	return st.workspace[0] ? st.workspace : NULL;
}

static void emit_decision(const grok_policy_result_t *pr, struct AgentId agent,
			  uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr dp;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;
	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&d, 0, sizeof(d));
	d.decision = (enum Decision)pr->decision;
	d.reason = ctext(pr->reason);
	d.agentId = put_agent(capn_root(&c).seg, agent.hi, agent.lo);
	dp = new_PolicyDecision(capn_root(&c).seg);
	write_PolicyDecision(&d, dp);
	if (capn_setp(capn_root(&c), 0, dp.p) != 0) {
		capn_free(&c);
		return;
	}
	(void)write_msg(&c, out, out_len);
	capn_free(&c);
}

static void deny_msg(struct AgentId agent, const char *reason, uint8_t **out,
		     size_t *out_len)
{
	grok_policy_result_t pr;

	memset(&pr, 0, sizeof(pr));
	pr.decision = GROK_DECISION_DENY;
	snprintf(pr.reason, sizeof(pr.reason), "%s", reason ? reason : "deny");
	emit_decision(&pr, agent, out, out_len);
}

static int open_in(const uint8_t *in, size_t in_len, struct capn *c)
{
	if (!in || in_len == 0 || in_len > GROK_POLICY_CAPNP_MAX_BODY)
		return -1;
	memset(c, 0, sizeof(*c));
	return capn_init_mem(c, in, in_len, 0);
}

void grok_policyd_status(grok_supervisor_t *sup, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicydStatus st;
	PolicydStatus_ptr sp;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;
	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&st, 0, sizeof(st));
	st.version = ctext(grok_policyd_version_string());
	st.apiVersion = grok_policyd_api_version();
	st.stateDir = ctext(sup ? grok_supervisor_state_dir(sup) : "");
	st.runtimeDir = ctext(sup ? grok_supervisor_runtime_dir(sup) : "");
	st.ready = sup ? 1 : 0;
	sp = new_PolicydStatus(capn_root(&c).seg);
	write_PolicydStatus(&st, sp);
	if (capn_setp(capn_root(&c), 0, sp.p) != 0) {
		capn_free(&c);
		return;
	}
	(void)write_msg(&c, out, out_len);
	capn_free(&c);
}

void grok_policyd_check_seat(grok_supervisor_t *sup, const uint8_t *in,
			     size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct SeatCheck sc;
	struct AgentId agent;
	grok_policy_result_t pr;
	SeatCheck_ptr root;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid SeatCheck message", out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_SeatCheck(&sc, root);
	read_agent(sc.agentId, &agent);
	memset(&pr, 0, sizeof(pr));
	switch (sc.action) {
	case SeatAction_publishRun:
	case SeatAction_readRun:
	case SeatAction_listRuns:
	case SeatAction_listEvents:
		pr.decision = GROK_DECISION_ALLOW;
		snprintf(pr.reason, sizeof(pr.reason),
			 "seat board op allow (session plane ACL)");
		break;
	default:
		pr.decision = GROK_DECISION_DENY;
		snprintf(pr.reason, sizeof(pr.reason), "unknown seat action");
		break;
	}
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
}

void grok_policyd_check_model(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct ModelCheck mc;
	struct AgentId agent;
	grok_policy_result_t pr;
	ModelCheck_ptr root;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid ModelCheck message", out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_ModelCheck(&mc, root);
	read_agent(mc.agentId, &agent);
	memset(&pr, 0, sizeof(pr));
	pr.decision = GROK_DECISION_ALLOW;
	snprintf(pr.reason, sizeof(pr.reason), "model start admit plane");
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
}

void grok_policyd_check_path(grok_supervisor_t *sup, const uint8_t *in,
			     size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PathCheck pc;
	struct AgentId agent;
	char path[GROK_PATH_MAX];
	const char *ws;
	grok_policy_result_t pr;
	PathCheck_ptr root;
	size_t pl;

	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid PathCheck message", out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PathCheck(&pc, root);
	read_agent(pc.agentId, &agent);
	ws = workspace_for(sup, agent);
	pl = pc.path.len > 0 ? (size_t)pc.path.len : 0;
	if (pl >= sizeof(path) || (pl > 0 && !pc.path.str)) {
		capn_free(&c);
		deny_msg(agent, "path too long", out, out_len);
		return;
	}
	if (pl)
		memcpy(path, pc.path.str, pl);
	path[pl] = '\0';
	/* Reuse string bridge eval for path lexical rules */
	{
		const char *action = "read";

		if (pc.action == PathAction_write)
			action = "write";
		else if (pc.action == PathAction_delete)
			action = "delete";
		(void)grok_policy_eval(ws, "fs", action, path[0] ? path : NULL,
				       &pr);
	}
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
}

void grok_policyd_check_shell(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct ShellCheck sc;
	struct AgentId agent;
	const char *ws;
	char cwd[GROK_PATH_MAX];
	grok_policy_result_t pr;
	ShellCheck_ptr root;
	size_t cl;

	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid ShellCheck message", out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_ShellCheck(&sc, root);
	read_agent(sc.agentId, &agent);
	ws = workspace_for(sup, agent);
	cl = sc.cwd.len > 0 ? (size_t)sc.cwd.len : 0;
	if (cl >= sizeof(cwd) || (cl > 0 && !sc.cwd.str)) {
		capn_free(&c);
		deny_msg(agent, "cwd too long", out, out_len);
		return;
	}
	if (cl)
		memcpy(cwd, sc.cwd.str, cl);
	cwd[cl] = '\0';

	/* path-only shell allow, then argv content gate via decide_shell */
	(void)grok_policy_eval(ws, "shell", "exec", cwd[0] ? cwd : NULL, &pr);
	if (pr.decision == GROK_DECISION_ALLOW && sc.argv.type != CAPN_NULL &&
	    sc.argv.len > 0) {
		/* content gate: build via grok_policy_decide_shell if present */
		grok_policy_shell_gate(ws, cwd, sc.argv, &pr);
	}
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
}

void grok_policyd_check_risk(grok_supervisor_t *sup, const uint8_t *in,
			     size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct RiskCheck rc;
	struct AgentId agent;
	grok_policy_result_t pr;
	RiskCheck_ptr root;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid RiskCheck message", out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_RiskCheck(&rc, root);
	read_agent(rc.agentId, &agent);
	memset(&pr, 0, sizeof(pr));
	pr.decision = GROK_DECISION_PROMPT;
	snprintf(pr.reason, sizeof(pr.reason),
		 "high-risk action requires confirm");
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
}

void grok_policyd_admit_seat(grok_supervisor_t *sup, const uint8_t *in,
			     size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct AdmitSeat as;
	struct AgentId agent;
	struct capn synth;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *inner = NULL;
	size_t inner_len = 0;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid AdmitSeat message", out, out_len);
		return;
	}
	{
		AdmitSeat_ptr root;

		root.p = capn_getp(capn_root(&c), 0, 1);
		read_AdmitSeat(&as, root);
		read_agent(as.agentId, &agent);
	}
	capn_free(&c);
	/* synthesize SeatCheck(publishRun) */
	memset(&synth, 0, sizeof(synth));
	capn_init_malloc(&synth);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = put_agent(capn_root(&synth).seg, agent.hi, agent.lo);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&synth).seg);
	write_SeatCheck(&sc, sp);
	if (capn_setp(capn_root(&synth), 0, sp.p) != 0 ||
	    write_msg(&synth, &inner, &inner_len) != 0) {
		capn_free(&synth);
		deny_msg(agent, "admitSeat encode failed", out, out_len);
		return;
	}
	capn_free(&synth);
	grok_policyd_check_seat(sup, inner, inner_len, out, out_len);
	free(inner);
}

void grok_policyd_admit_model(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct AdmitModel am;
	struct AgentId agent;
	struct capn synth;
	struct ModelCheck mc;
	ModelCheck_ptr mp;
	uint8_t *inner = NULL;
	size_t inner_len = 0;

	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, "invalid AdmitModel message", out, out_len);
		return;
	}
	{
		AdmitModel_ptr root;

		root.p = capn_getp(capn_root(&c), 0, 1);
		read_AdmitModel(&am, root);
		read_agent(am.agentId, &agent);
	}
	capn_free(&c);
	memset(&synth, 0, sizeof(synth));
	capn_init_malloc(&synth);
	memset(&mc, 0, sizeof(mc));
	mc.agentId = put_agent(capn_root(&synth).seg, agent.hi, agent.lo);
	mc.model = ctext("");
	mp = new_ModelCheck(capn_root(&synth).seg);
	write_ModelCheck(&mc, mp);
	if (capn_setp(capn_root(&synth), 0, mp.p) != 0 ||
	    write_msg(&synth, &inner, &inner_len) != 0) {
		capn_free(&synth);
		deny_msg(agent, "admitModel encode failed", out, out_len);
		return;
	}
	capn_free(&synth);
	grok_policyd_check_model(sup, inner, inner_len, out, out_len);
	free(inner);
}

void grok_policyd_agent_status(grok_supervisor_t *sup, const uint8_t *in,
			       size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c, co;
	struct AgentQuery aq;
	struct AgentId agent;
	struct AgentStatus as;
	grok_agent_status_t st;
	char hex[GROK_ID_MAX];
	AgentQuery_ptr root;
	AgentStatus_ptr asp;
	int rc;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0)
		goto missing;
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_AgentQuery(&aq, root);
	read_agent(aq.agentId, &agent);
	capn_free(&c);
	grok_agent_id_to_hex(agent.hi, agent.lo, hex);
	if (!hex[0] || !sup)
		goto missing;
	rc = grok_supervisor_status(sup, hex, &st);
	if (rc != GROK_OK)
		goto missing;

	memset(&co, 0, sizeof(co));
	capn_init_malloc(&co);
	memset(&as, 0, sizeof(as));
	as.id = put_agent(capn_root(&co).seg, agent.hi, agent.lo);
	as.state = (enum AgentState)st.state;
	as.pid = st.pid;
	as.pgid = st.pgid;
	as.exitStatus = st.exit_status;
	as.mode = SeatMode_develop;
	as.workspace = ctext(st.workspace);
	as.hasCgroup = st.has_cgroup ? 1 : 0;
	as.detail = ctext("");
	asp = new_AgentStatus(capn_root(&co).seg);
	write_AgentStatus(&as, asp);
	if (capn_setp(capn_root(&co), 0, asp.p) != 0) {
		capn_free(&co);
		return;
	}
	(void)write_msg(&co, out, out_len);
	capn_free(&co);
	return;

missing:
	memset(&co, 0, sizeof(co));
	capn_init_malloc(&co);
	memset(&as, 0, sizeof(as));
	as.id = put_agent(capn_root(&co).seg, agent.hi, agent.lo);
	as.state = AgentState_missing;
	as.detail = ctext("agent not found");
	asp = new_AgentStatus(capn_root(&co).seg);
	write_AgentStatus(&as, asp);
	if (capn_setp(capn_root(&co), 0, asp.p) == 0)
		(void)write_msg(&co, out, out_len);
	capn_free(&co);
}
