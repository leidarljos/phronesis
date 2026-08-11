/* SPDX-License-Identifier: MIT */
/*
 * interface Policyd: one Cap'n method per entry point.
 * Params message root in, result message root out. Always PolicyDecision for
 * checks (fail-closed deny on bad input). No unions, no Decision-in-int.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "internal.h"
#include "policy_janet.h"
#include "policy_trace.h"
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

/* Copy agent workspace into caller buffer; never return a pointer into a local. */
static const char *workspace_for(grok_supervisor_t *sup, struct AgentId id,
				 char *ws_buf, size_t ws_len)
{
	char hex[GROK_ID_MAX];
	grok_agent_status_t st;

	if (!ws_buf || ws_len == 0)
		return NULL;
	ws_buf[0] = '\0';
	grok_agent_id_to_hex(id.hi, id.lo, hex);
	if (!hex[0])
		return NULL;
	if (grok_supervisor_status(sup, hex, &st) != GROK_OK)
		return NULL;
	if (!st.workspace[0])
		return NULL;
	snprintf(ws_buf, ws_len, "%s", st.workspace);
	return ws_buf;
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
	/* TCB leaves reason empty; product packs set reason on Cap'n passthrough. */
	d.reason = ctext("");
	d.agentId = put_agent(capn_root(&c).seg, agent.hi, agent.lo);
	d.code = (enum PolicyReason)pr->code;
	dp = new_PolicyDecision(capn_root(&c).seg);
	write_PolicyDecision(&d, dp);
	if (capn_setp(capn_root(&c), 0, dp.p) != 0) {
		capn_free(&c);
		return;
	}
	(void)write_msg(&c, out, out_len);
	capn_free(&c);
}

static void emit_code(grok_decision_t decision, grok_policy_reason_t code,
		      struct AgentId agent, uint8_t **out, size_t *out_len)
{
	grok_policy_result_t pr;

	grok_policy_result_set(&pr, decision, code);
	emit_decision(&pr, agent, out, out_len);
}

static void deny_msg(struct AgentId agent, grok_policy_reason_t code,
		     uint8_t **out, size_t *out_len)
{
	emit_code(GROK_DECISION_DENY, code, agent, out, out_len);
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
	PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_ENTER, "checkShell",
		       "grok_policyd_check_shell", -1, NULL, 0);
	if (open_in(in, in_len, &c) != 0) {
		PD_TRACE_EVENT(PD_TRACE_LAYER_CAPNP, PD_TRACE_PHASE_ERROR,
			       "checkShell/open", "invalid Cap'n message",
			       (int)GROK_REASON_INVALID_MESSAGE, "deny", 1);
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_SeatCheck(&sc, root);
	read_agent(sc.agentId, &agent);
	switch (sc.action) {
	case SeatAction_publishRun:
	case SeatAction_readRun:
	case SeatAction_listRuns:
	case SeatAction_listEvents:
		grok_policy_result_set(&pr, GROK_DECISION_ALLOW,
				       GROK_REASON_SEAT_BOARD_ALLOW);
		break;
	default:
		grok_policy_result_set(&pr, GROK_DECISION_DENY,
				       GROK_REASON_UNKNOWN_SEAT_ACTION);
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_ModelCheck(&mc, root);
	read_agent(mc.agentId, &agent);
	grok_policy_result_set(&pr, GROK_DECISION_ALLOW,
			       GROK_REASON_MODEL_START_ALLOW);
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
	char ws_buf[GROK_PATH_MAX];
	const char *ws;
	grok_policy_result_t pr;
	PathCheck_ptr root;
	size_t pl;

	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PathCheck(&pc, root);
	read_agent(pc.agentId, &agent);
	ws = workspace_for(sup, agent, ws_buf, sizeof(ws_buf));
	pl = pc.path.len > 0 ? (size_t)pc.path.len : 0;
	if (pl >= sizeof(path) || (pl > 0 && !pc.path.str)) {
		capn_free(&c);
		deny_msg(agent, GROK_REASON_FIELD_TOO_LONG, out, out_len);
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

#ifdef GROKOS_POLICYD_TRACE
static const char *decision_str(grok_decision_t d)
{
	switch (d) {
	case GROK_DECISION_ALLOW:
		return "allow";
	case GROK_DECISION_PROMPT:
		return "prompt";
	case GROK_DECISION_DENY:
	default:
		return "deny";
	}
}
#endif

void grok_policyd_check_shell(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct ShellCheck sc;
	struct AgentId agent;
	char ws_buf[GROK_PATH_MAX];
	const char *ws;
	char cwd[GROK_PATH_MAX];
	grok_policy_result_t pr;
	ShellCheck_ptr root;
	size_t cl;

	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_ShellCheck(&sc, root);
	read_agent(sc.agentId, &agent);
	ws = workspace_for(sup, agent, ws_buf, sizeof(ws_buf));
	cl = sc.cwd.len > 0 ? (size_t)sc.cwd.len : 0;
	if (cl >= sizeof(cwd) || (cl > 0 && !sc.cwd.str)) {
		capn_free(&c);
		deny_msg(agent, GROK_REASON_FIELD_TOO_LONG, out, out_len);
		return;
	}
	if (cl)
		memcpy(cwd, sc.cwd.str, cl);
	cwd[cl] = '\0';

	/* Path plane first; content pack returns Cap'n PolicyDecision for passthrough. */
	PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_GATE,
		       "checkShell/path-plane", cwd[0] ? cwd : "", -1, NULL, 0);
	(void)grok_policy_eval(ws, "shell", "exec", cwd[0] ? cwd : NULL, &pr);
	if (pr.decision != GROK_DECISION_ALLOW) {
		capn_free(&c);
#ifdef GROKOS_POLICYD_TRACE
		PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_DECIDE,
			       "checkShell/path-plane", "path plane short-circuit",
			       (int)pr.code, decision_str(pr.decision), 1);
#endif
		emit_decision(&pr, agent, out, out_len);
		return;
	}
	/*
	 * Generated read_ShellCheck uses capn_getp(..., 0): argv may be an
	 * unresolved far pointer with len==0. Resolve before testing length
	 * (same idea as c-capnproto's capn_len macro).
	 */
	capn_resolve(&sc.argv);
	if (sc.argv.type != CAPN_NULL && sc.argv.len > 0) {
		uint8_t *pack_out = NULL;
		size_t pack_len = 0;

		PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_ENTER,
			       "checkShell/pack", "multi-pack shell-check compose", -1,
			       NULL, 0);
		grok_policy_shell_pack(ws, cwd, sc.argv, &pack_out, &pack_len);
		capn_free(&c);
		if (pack_out && pack_len) {
			/* Passthrough pack Cap'n PolicyDecision (reason from pack). */
			*out = pack_out;
			*out_len = pack_len;
			return;
		}
		deny_msg(agent, GROK_REASON_PACK_BAD_RESULT, out, out_len);
		return;
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_RiskCheck(&rc, root);
	read_agent(rc.agentId, &agent);
	memset(&pr, 0, sizeof(pr));
	/* secretExport: never allow (no secrets leave seat / traces). */
	if (rc.action == RiskAction_secretExport) {
		grok_policy_result_set(&pr, GROK_DECISION_DENY,
				       GROK_REASON_SECRET_EXPORT_DENIED);
	} else {
		/* network/sudo/pay/auth/…: prompt; agent fail-closes until UX. */
		grok_policy_result_set(&pr, GROK_DECISION_PROMPT,
				       GROK_REASON_HIGH_RISK_PROMPT);
	}
	capn_free(&c);
	emit_decision(&pr, agent, out, out_len);
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

/** Read Cap'n PolicyDecision decision+code into @a out (reason ignored). */
static int policy_result_from_capnp(const uint8_t *msg, size_t len,
				    grok_policy_result_t *out)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr root;

	if (!msg || !len || !out)
		return -1;
	memset(&c, 0, sizeof(c));
	if (capn_init_mem(&c, msg, len, 0) != 0)
		return -1;
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	grok_policy_result_set(out, (grok_decision_t)d.decision,
			       (grok_policy_reason_t)d.code);
	capn_free(&c);
	return 0;
}

void grok_policyd_check_audio(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct AudioCheck ac;
	struct AgentId agent;
	grok_policy_result_t pr;
	AudioCheck_ptr root;
	uint8_t *pack_out = NULL;
	size_t pack_len = 0;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_AudioCheck(&ac, root);
	read_agent(ac.agentId, &agent);
	capn_free(&c);

	/* Hard TCB env gates (same story as shell workspace gate before pack). */
	if (env_truthy("GROKOS_POLICYD_DENY_ALL")) {
		emit_code(GROK_DECISION_DENY, GROK_REASON_DENY_ALL, agent, out,
			  out_len);
		return;
	}
	if (env_truthy("GROKOS_POLICYD_AUDIO_ALLOW")) {
		emit_code(GROK_DECISION_ALLOW, GROK_REASON_AUDIO_FIXTURE_ALLOW,
			  agent, out, out_len);
		return;
	}

	/*
	 * Product table: Cap'n AudioCheck → Janet audio-check → PolicyDecision.
	 * Re-stamp agentId in TCB (pack does not need to echo it).
	 */
	grok_policy_audio_pack(in, in_len, &pack_out, &pack_len);
	if (!pack_out || !pack_len ||
	    policy_result_from_capnp(pack_out, pack_len, &pr) != 0) {
		free(pack_out);
		deny_msg(agent, GROK_REASON_PACK_BAD_RESULT, out, out_len);
		return;
	}
	free(pack_out);
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
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
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
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

void grok_policyd_reload_shell_pack(grok_supervisor_t *sup, const uint8_t *in,
				    size_t in_len, uint8_t **out,
				    size_t *out_len)
{
	struct capn c;
	struct ReloadShellPack rp;
	struct AgentId agent;
	grok_policy_result_t pr;
	/* Colon-joined multi-pack spec (files and/or dirs); same cap as host. */
	char path[4096 * 16];
	size_t pl;
	int rc;

	(void)sup;
	memset(&agent, 0, sizeof(agent));
	if (open_in(in, in_len, &c) != 0) {
		deny_msg(agent, GROK_REASON_INVALID_MESSAGE, out, out_len);
		return;
	}
	{
		ReloadShellPack_ptr root;

		root.p = capn_getp(capn_root(&c), 0, 1);
		read_ReloadShellPack(&rp, root);
	}
	pl = rp.path.len > 0 ? (size_t)rp.path.len : 0;
	if (pl == 0 || pl >= sizeof(path) || !rp.path.str) {
		capn_free(&c);
		deny_msg(agent, GROK_REASON_PACK_PATH_INVALID, out, out_len);
		return;
	}
	memcpy(path, rp.path.str, pl);
	path[pl] = '\0';
	capn_free(&c);

	rc = grok_policy_shell_pack_reload_internal(path);
	memset(&pr, 0, sizeof(pr));
	if (rc == 0) {
		grok_policy_result_set(&pr, GROK_DECISION_ALLOW,
				       GROK_REASON_PACK_RELOADED);
	} else if (rc == -1) {
		grok_policy_result_set(&pr, GROK_DECISION_DENY,
				       GROK_REASON_PACK_PATH_INVALID);
	} else {
		grok_policy_result_set(&pr, GROK_DECISION_DENY,
				       GROK_REASON_PACK_LOAD_FAILED);
	}
	emit_decision(&pr, agent, out, out_len);
}
