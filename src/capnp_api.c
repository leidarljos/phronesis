/* SPDX-License-Identifier: Apache-2.0 */
/*
 * interface Policyd via CallEnvelope (FFI without Cap'n RPC runtime).
 * Method returns are *Results unions (ok | err). Decision is never a C errno.
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

static int capn_write_body(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf = NULL;
	size_t cap = 8192U;
	const size_t cap_max = 1024U * 1024U;
	int64_t n;

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

static AgentId_ptr write_agent(struct capn_segment *seg, uint64_t hi,
			       uint64_t lo)
{
	struct AgentId id;
	AgentId_ptr p;

	id.hi = hi;
	id.lo = lo;
	p = new_AgentId(seg);
	write_AgentId(&id, p);
	return p;
}

static TraceId_ptr write_trace(struct capn_segment *seg, const struct TraceId *t)
{
	TraceId_ptr p = new_TraceId(seg);

	write_TraceId(t, p);
	return p;
}

static int read_agent_ptr(AgentId_ptr p, struct AgentId *out)
{
	if (p.p.type == CAPN_NULL) {
		out->hi = 0;
		out->lo = 0;
		return 0;
	}
	read_AgentId(out, p);
	return 0;
}

static int read_trace_ptr(TraceId_ptr p, struct TraceId *out)
{
	if (p.p.type == CAPN_NULL) {
		out->hi = 0;
		out->lo = 0;
		return 0;
	}
	read_TraceId(out, p);
	return 0;
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

static int reply_check_results(const struct TraceId *trace, int admit,
			       const grok_policy_result_t *pr,
			       struct AgentId agent, uint8_t **out,
			       size_t *out_len)
{
	struct capn c;
	struct CallEnvelope env;
	struct CheckResults cr;
	struct PolicyDecision d;
	CheckResults_ptr crp;
	PolicyDecision_ptr dp;
	CallEnvelope_ptr cep;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&d, 0, sizeof(d));
	d.decision = (enum Decision)pr->decision;
	d.reason = ctext(pr->reason);
	d.agentId = write_agent(capn_root(&c).seg, agent.hi, agent.lo);
	dp = new_PolicyDecision(capn_root(&c).seg);
	write_PolicyDecision(&d, dp);
	memset(&cr, 0, sizeof(cr));
	cr.which = CheckResults_ok;
	cr.ok = dp;
	crp = new_CheckResults(capn_root(&c).seg);
	write_CheckResults(&cr, crp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = write_trace(capn_root(&c).seg, trace);
	if (admit) {
		env.body_which = CallEnvelope_body_admitResults;
		env.body.admitResults = crp;
	} else {
		env.body_which = CallEnvelope_body_checkResults;
		env.body.checkResults = crp;
	}
	cep = new_CallEnvelope(capn_root(&c).seg);
	write_CallEnvelope(&env, cep);
	if (capn_setp(capn_root(&c), 0, cep.p) != 0) {
		capn_free(&c);
		return -1;
	}
	if (capn_write_body(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int reply_check_err(const struct TraceId *trace, int admit,
			   enum ErrCode code, const char *msg, uint8_t **out,
			   size_t *out_len)
{
	struct capn c;
	struct CallEnvelope env;
	struct CheckResults cr;
	struct Err e;
	CheckResults_ptr crp;
	Err_ptr ep;
	CallEnvelope_ptr cep;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&e, 0, sizeof(e));
	e.code = code;
	e.message = ctext(msg);
	ep = new_Err(capn_root(&c).seg);
	write_Err(&e, ep);
	memset(&cr, 0, sizeof(cr));
	cr.which = CheckResults_err;
	cr.err = ep;
	crp = new_CheckResults(capn_root(&c).seg);
	write_CheckResults(&cr, crp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = write_trace(capn_root(&c).seg, trace);
	if (admit) {
		env.body_which = CallEnvelope_body_admitResults;
		env.body.admitResults = crp;
	} else {
		env.body_which = CallEnvelope_body_checkResults;
		env.body.checkResults = crp;
	}
	cep = new_CallEnvelope(capn_root(&c).seg);
	write_CallEnvelope(&env, cep);
	if (capn_setp(capn_root(&c), 0, cep.p) != 0) {
		capn_free(&c);
		return -1;
	}
	if (capn_write_body(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

int grok_policyd_handle_capnp(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	CallEnvelope_ptr root;
	struct CallEnvelope env;
	struct TraceId trace;

	if (!out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	memset(&trace, 0, sizeof(trace));
	if (!sup || !in || in_len == 0 || in_len > GROK_POLICY_CAPNP_MAX_BODY)
		return reply_check_err(&trace, 0, ErrCode_inval,
				       "bad request body", out, out_len);

	memset(&c, 0, sizeof(c));
	if (capn_init_mem(&c, in, in_len, 0) != 0)
		return reply_check_err(&trace, 0, ErrCode_inval,
				       "capn init failed", out, out_len);

	root.p = capn_getp(capn_root(&c), 0, 1);
	read_CallEnvelope(&env, root);
	if (env.protocolVersion != 0 && env.protocolVersion != 1) {
		capn_free(&c);
		return reply_check_err(&trace, 0, ErrCode_inval,
				       "unsupported protocolVersion", out,
				       out_len);
	}
	read_trace_ptr(env.traceId, &trace);

	switch (env.body_which) {
	case CallEnvelope_body_status: {
		struct capn co;
		struct CallEnvelope re;
		struct StatusResults sr;
		struct PolicydStatus st;
		StatusResults_ptr srp;
		PolicydStatus_ptr stp;
		CallEnvelope_ptr cep;

		capn_free(&c);
		memset(&co, 0, sizeof(co));
		capn_init_malloc(&co);
		memset(&st, 0, sizeof(st));
		st.version = ctext(grok_policyd_version_string());
		st.apiVersion = grok_policyd_api_version();
		st.stateDir = ctext(grok_supervisor_state_dir(sup));
		st.runtimeDir = ctext(grok_supervisor_runtime_dir(sup));
		st.ready = 1;
		stp = new_PolicydStatus(capn_root(&co).seg);
		write_PolicydStatus(&st, stp);
		memset(&sr, 0, sizeof(sr));
		sr.which = StatusResults_ok;
		sr.ok = stp;
		srp = new_StatusResults(capn_root(&co).seg);
		write_StatusResults(&sr, srp);
		memset(&re, 0, sizeof(re));
		re.protocolVersion = 1;
		re.traceId = write_trace(capn_root(&co).seg, &trace);
		re.body_which = CallEnvelope_body_statusResults;
		re.body.statusResults = srp;
		cep = new_CallEnvelope(capn_root(&co).seg);
		write_CallEnvelope(&re, cep);
		if (capn_setp(capn_root(&co), 0, cep.p) != 0) {
			capn_free(&co);
			return -1;
		}
		if (capn_write_body(&co, out, out_len) != 0) {
			capn_free(&co);
			return -1;
		}
		capn_free(&co);
		return 0;
	}
	case CallEnvelope_body_check: {
		struct CheckParams params;
		struct AgentId agent;
		grok_policy_result_t pr;
		const char *ws;
		char hex[GROK_ID_MAX];
		char detail[GROK_DETAIL_MAX];

		read_CheckParams(&params, env.body.check);
		read_agent_ptr(params.agentId, &agent);
		ws = workspace_for(sup, agent);
		grok_policy_decide_params(ws, &params, &pr);
		grok_agent_id_to_hex(agent.hi, agent.lo, hex);
		snprintf(detail, sizeof(detail), "check decision=%d %s",
			 (int)pr.decision, pr.reason);
		(void)grok_supervisor_log(sup, hex[0] ? hex : "-", "policy",
					  detail);
		capn_free(&c);
		return reply_check_results(&trace, 0, &pr, agent, out, out_len);
	}
	case CallEnvelope_body_admit: {
		struct AdmitParams admit;
		struct CheckParams params;
		struct CheckBody body;
		struct AgentId agent;
		grok_policy_result_t pr;
		const char *ws;
		struct capn synth;
		CheckBody_ptr bp;
		AgentId_ptr ap;

		read_AdmitParams(&admit, env.body.admit);
		read_agent_ptr(admit.agentId, &agent);
		if (admit.kind == AdmitKind_unset) {
			capn_free(&c);
			return reply_check_err(&trace, 1, ErrCode_inval,
					       "unknown admit.kind", out,
					       out_len);
		}
		/* Build CheckParams in a live segment for body pointers. */
		memset(&synth, 0, sizeof(synth));
		capn_init_malloc(&synth);
		memset(&body, 0, sizeof(body));
		if (admit.kind == AdmitKind_seat) {
			body.which = CheckBody_seat;
			body.seat = SeatAction_publishRun;
		} else {
			/* model / agent */
			struct ModelOp mop;
			ModelOp_ptr mp;

			body.which = CheckBody_model;
			memset(&mop, 0, sizeof(mop));
			mop.model = ctext("");
			mp = new_ModelOp(capn_root(&synth).seg);
			write_ModelOp(&mop, mp);
			body.model = mp;
		}
		bp = new_CheckBody(capn_root(&synth).seg);
		write_CheckBody(&body, bp);
		ap = write_agent(capn_root(&synth).seg, agent.hi, agent.lo);
		memset(&params, 0, sizeof(params));
		params.agentId = ap;
		params.body = bp;
		ws = workspace_for(sup, agent);
		grok_policy_decide_params(ws, &params, &pr);
		capn_free(&synth);
		capn_free(&c);
		return reply_check_results(&trace, 1, &pr, agent, out, out_len);
	}
	case CallEnvelope_body_agentStatus: {
		struct AgentStatusParams aq;
		struct AgentId agent;
		struct AgentStatus as;
		struct capn co;
		struct CallEnvelope re;
		struct AgentStatusResults ar;
		grok_agent_status_t st;
		char hex[GROK_ID_MAX];
		int rc;
		AgentStatus_ptr asp;
		AgentStatusResults_ptr arp;
		CallEnvelope_ptr cep;

		read_AgentStatusParams(&aq, env.body.agentStatus);
		read_agent_ptr(aq.agentId, &agent);
		grok_agent_id_to_hex(agent.hi, agent.lo, hex);
		if (!hex[0]) {
			capn_free(&c);
			return reply_check_err(&trace, 0, ErrCode_inval,
					       "agentId required", out,
					       out_len);
		}
		rc = grok_supervisor_status(sup, hex, &st);
		capn_free(&c);
		if (rc != GROK_OK)
			return reply_check_err(
				&trace, 0,
				rc == GROK_ERR_NOTFOUND ? ErrCode_notFound
							: ErrCode_internal,
				"agent status failed", out, out_len);
		memset(&co, 0, sizeof(co));
		capn_init_malloc(&co);
		memset(&as, 0, sizeof(as));
		as.id = write_agent(capn_root(&co).seg, agent.hi, agent.lo);
		as.state = (enum AgentState)st.state;
		as.pid = st.pid;
		as.pgid = st.pgid;
		as.exitStatus = st.exit_status;
		as.mode = SeatMode_develop;
		as.workspace = ctext(st.workspace);
		as.hasCgroup = st.has_cgroup ? 1 : 0;
		asp = new_AgentStatus(capn_root(&co).seg);
		write_AgentStatus(&as, asp);
		memset(&ar, 0, sizeof(ar));
		ar.which = AgentStatusResults_ok;
		ar.ok = asp;
		arp = new_AgentStatusResults(capn_root(&co).seg);
		write_AgentStatusResults(&ar, arp);
		memset(&re, 0, sizeof(re));
		re.protocolVersion = 1;
		re.traceId = write_trace(capn_root(&co).seg, &trace);
		re.body_which = CallEnvelope_body_agentStatusResults;
		re.body.agentStatusResults = arp;
		cep = new_CallEnvelope(capn_root(&co).seg);
		write_CallEnvelope(&re, cep);
		if (capn_setp(capn_root(&co), 0, cep.p) != 0) {
			capn_free(&co);
			return -1;
		}
		if (capn_write_body(&co, out, out_len) != 0) {
			capn_free(&co);
			return -1;
		}
		capn_free(&co);
		return 0;
	}
	default:
		capn_free(&c);
		return reply_check_err(&trace, 0, ErrCode_inval,
				       "unknown call body", out, out_len);
	}
}
