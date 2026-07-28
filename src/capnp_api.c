/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Cap'n policy API for in-process FFI.
 * Callers compose PolicyEnvelope (schema/policy.capnp) and call
 * grok_policyd_handle_capnp — no nng socket peer.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "internal.h"
#include "policy.capnp.h"

#include <capnp_c.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/** Fail closed: refuse Cap'n text that does not fit the TCB C buffer. */
static int ctext_copy(char *dst, size_t n, capn_text t)
{
	size_t l;

	if (!dst || n == 0 || t.len < 0)
		return -1;
	l = (size_t)t.len;
	if (l > 0 && !t.str)
		return -1;
	if (l >= n)
		return -1;
	if (l > 0)
		memcpy(dst, t.str, l);
	dst[l] = '\0';
	return 0;
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
		if (cap == 0 || cap > cap_max)
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

static int encode_ok(const char *trace, enum PolicyResponse_ok_which which,
		     void *payload, uint8_t **out, size_t *out_len)
{
	struct capn c;
	capn_ptr cr;
	struct capn_segment *cs;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	PolicyEnvelope_ptr ep;
	PolicyResponse_ptr rp;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	cr = capn_root(&c);
	cs = cr.seg;

	memset(&pr, 0, sizeof(pr));
	pr.ok_which = which;
	switch (which) {
	case PolicyResponse_ok_status: {
		struct PolicydStatus *st = payload;
		PolicydStatus_ptr sp = new_PolicydStatus(cs);

		write_PolicydStatus(st, sp);
		pr.ok.status = sp;
		break;
	}
	case PolicyResponse_ok_check:
	case PolicyResponse_ok_admit: {
		struct PolicyDecision *d = payload;
		PolicyDecision_ptr dp = new_PolicyDecision(cs);

		write_PolicyDecision(d, dp);
		if (which == PolicyResponse_ok_check)
			pr.ok.check = dp;
		else
			pr.ok.admit = dp;
		break;
	}
	case PolicyResponse_ok_agentStatus: {
		struct AgentStatusWire *a = payload;
		AgentStatusWire_ptr ap = new_AgentStatusWire(cs);

		write_AgentStatusWire(a, ap);
		pr.ok.agentStatus = ap;
		break;
	}
	case PolicyResponse_ok_error:
	default: {
		struct PolicyError *e = payload;
		PolicyError_ptr ep2 = new_PolicyError(cs);

		write_PolicyError(e, ep2);
		pr.ok_which = PolicyResponse_ok_error;
		pr.ok.error = ep2;
		break;
	}
	}

	rp = new_PolicyResponse(cs);
	write_PolicyResponse(&pr, rp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = ctext(trace ? trace : "");
	env.body_which = PolicyEnvelope_body_response;
	env.body.response = rp;
	ep = new_PolicyEnvelope(cs);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0) {
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

static int reply_error(const char *trace, int32_t code, const char *msg,
		       uint8_t **out, size_t *out_len)
{
	struct PolicyError e;

	memset(&e, 0, sizeof(e));
	e.code = code;
	e.message = ctext(msg ? msg : "error");
	return encode_ok(trace, PolicyResponse_ok_error, &e, out, out_len);
}

int grok_policyd_handle_capnp(grok_supervisor_t *sup, const uint8_t *in,
			      size_t in_len, uint8_t **out, size_t *out_len)
{
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyRequest req;
	char trace[128];
	char agent[64], tool[64], action[64], path[512];
	char kind[32], detail[512];

	if (!out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	if (!sup || !in || in_len == 0 || in_len > GROK_POLICY_CAPNP_MAX_BODY)
		return reply_error("", GROK_ERR_INVAL, "bad request body", out,
				   out_len);

	memset(&c, 0, sizeof(c));
	if (capn_init_mem(&c, in, in_len, 0) != 0)
		return reply_error("", GROK_ERR_INVAL, "capn init failed", out,
				   out_len);

	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	/* protocolVersion 0 = unset (legacy); only 1 is current. */
	if (env.protocolVersion != 0 && env.protocolVersion != 1) {
		capn_free(&c);
		return reply_error("", GROK_ERR_INVAL,
				   "unsupported protocolVersion", out, out_len);
	}
	if (ctext_copy(trace, sizeof(trace), env.traceId) != 0) {
		capn_free(&c);
		return reply_error("", GROK_ERR_INVAL, "traceId too long", out,
				   out_len);
	}
	if (env.body_which != PolicyEnvelope_body_request) {
		capn_free(&c);
		return reply_error(trace, GROK_ERR_INVAL,
				   "expected request envelope", out, out_len);
	}
	read_PolicyRequest(&req, env.body.request);

	switch (req.op_which) {
	case PolicyRequest_op_status: {
		struct PolicydStatus st;

		capn_free(&c);
		memset(&st, 0, sizeof(st));
		st.version = ctext(grok_policyd_version_string());
		st.apiVersion = grok_policyd_api_version();
		st.stateDir = ctext(grok_supervisor_state_dir(sup));
		st.runtimeDir = ctext(grok_supervisor_runtime_dir(sup));
		st.socket = ctext("ffi"); /* in-process; no peer socket */
		st.ready = 1;
		return encode_ok(trace, PolicyResponse_ok_status, &st, out,
				 out_len);
	}
	case PolicyRequest_op_check: {
		struct PolicyCheck ch;
		struct PolicyDecision d;
		grok_policy_result_t pr;
		int rc;

		read_PolicyCheck(&ch, req.op.check);
		if (ctext_copy(agent, sizeof(agent), ch.agentId) != 0 ||
		    ctext_copy(tool, sizeof(tool), ch.tool) != 0 ||
		    ctext_copy(action, sizeof(action), ch.action) != 0 ||
		    ctext_copy(path, sizeof(path), ch.path) != 0) {
			capn_free(&c);
			return reply_error(trace, GROK_ERR_INVAL,
					   "check field too long", out,
					   out_len);
		}
		capn_free(&c);
		rc = grok_policy_check(sup, agent, tool, action,
				       path[0] ? path : NULL, &pr);
		if (rc != GROK_OK)
			return reply_error(trace, rc, "policy_check failed", out,
					   out_len);
		memset(&d, 0, sizeof(d));
		d.decision = (enum Decision)pr.decision;
		d.reason = ctext(pr.reason);
		d.agentId = ctext(agent);
		d.tool = ctext(tool);
		d.action = ctext(action);
		return encode_ok(trace, PolicyResponse_ok_check, &d, out,
				 out_len);
	}
	case PolicyRequest_op_admit: {
		struct PolicyAdmit ad;
		struct PolicyDecision d;
		const char *t = NULL;
		const char *a = NULL;
		const char *p = NULL;
		grok_policy_result_t pr;
		int rc;

		read_PolicyAdmit(&ad, req.op.admit);
		if (ctext_copy(agent, sizeof(agent), ad.agentId) != 0 ||
		    ctext_copy(kind, sizeof(kind), ad.kind) != 0 ||
		    ctext_copy(detail, sizeof(detail), ad.detail) != 0) {
			capn_free(&c);
			return reply_error(trace, GROK_ERR_INVAL,
					   "admit field too long", out,
					   out_len);
		}
		capn_free(&c);
		if (grok_policyd_map_admit_kind(kind, &t, &a) != 0)
			return reply_error(trace, GROK_ERR_INVAL,
					   "unknown admit.kind (fail-closed)",
					   out, out_len);
		if (strcmp(t, "seat") == 0)
			p = detail;
		rc = grok_policy_check(sup, agent, t, a, p, &pr);
		if (rc != GROK_OK)
			return reply_error(trace, rc, "admit check failed", out,
					   out_len);
		memset(&d, 0, sizeof(d));
		d.decision = (enum Decision)pr.decision;
		d.reason = ctext(pr.reason);
		d.agentId = ctext(agent);
		d.tool = ctext(t);
		d.action = ctext(a);
		return encode_ok(trace, PolicyResponse_ok_admit, &d, out,
				 out_len);
	}
	case PolicyRequest_op_agentStatus: {
		struct AgentQuery aq;
		struct AgentStatusWire a;
		grok_agent_status_t st;
		int rc;

		read_AgentQuery(&aq, req.op.agentStatus);
		if (ctext_copy(agent, sizeof(agent), aq.agentId) != 0) {
			capn_free(&c);
			return reply_error(trace, GROK_ERR_INVAL,
					   "agentId too long", out, out_len);
		}
		capn_free(&c);
		rc = grok_supervisor_status(sup, agent, &st);
		if (rc != GROK_OK)
			return reply_error(trace, rc, "agent status failed", out,
					   out_len);
		memset(&a, 0, sizeof(a));
		a.id = ctext(st.id);
		a.state = (enum AgentStateWire)st.state;
		a.pid = (st.pid > INT32_MAX || st.pid < 0) ? 0
							     : (int32_t)st.pid;
		a.pgid = (st.pgid > INT32_MAX || st.pgid < 0)
				 ? 0
				 : (int32_t)st.pgid;
		a.exitStatus = st.exit_status;
		a.mode = ctext(st.mode);
		a.workspace = ctext(st.workspace);
		a.hasCgroup = st.has_cgroup ? 1 : 0;
		return encode_ok(trace, PolicyResponse_ok_agentStatus, &a, out,
				 out_len);
	}
	default:
		capn_free(&c);
		return reply_error(trace, GROK_ERR_INVAL, "unknown op", out,
				   out_len);
	}
}

