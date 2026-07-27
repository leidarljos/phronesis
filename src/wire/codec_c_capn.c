/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Cap'n wire codec via c-capnproto (generated schema/policy.capnp.c).
 * Seacord/Effective C: bounds on sizes, checked growth, free on all error paths.
 */
#include "wire/capnp_min.h"

#include "policy.capnp.h"

#include <capnp_c.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static capn_text ctext(const char *s)
{
	capn_text t;
	size_t len;

	if (!s)
		s = "";
	len = strlen(s);
	/* capn_text.len is int; clamp (Effective C: no silent wrap). */
	if (len > (size_t)INT_MAX)
		len = (size_t)INT_MAX;
	t.len = (int)len;
	t.str = s;
	t.seg = NULL;
	return t;
}

static void copy_text(char *dst, size_t n, capn_text t)
{
	size_t l;

	if (!dst || n == 0)
		return;
	/* t.len is signed; reject negative before cast to size_t. */
	if (t.len <= 0 || !t.str)
		l = 0;
	else
		l = (size_t)t.len;
	if (l >= n)
		l = n - 1U;
	if (l > 0)
		memcpy(dst, t.str, l);
	dst[l] = '\0';
}

int wire_decode_request(const uint8_t *body, size_t body_len, struct wire_request *out)
{
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyRequest req;

	if (!body || body_len == 0 || !out)
		return -1;
	if (body_len > GROK_NNG_MAX_BODY)
		return -1;
	memset(out, 0, sizeof(*out));
	memset(&c, 0, sizeof(c));
	if (capn_init_mem(&c, body, body_len, 0) != 0)
		return -1;
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	copy_text(out->trace_id, sizeof(out->trace_id), env.traceId);
	if (env.body_which != PolicyEnvelope_body_request) {
		capn_free(&c);
		return -1;
	}
	read_PolicyRequest(&req, env.body.request);
	switch (req.op_which) {
	case PolicyRequest_op_status:
		out->op = WIRE_OP_STATUS;
		break;
	case PolicyRequest_op_check: {
		struct PolicyCheck ch;

		out->op = WIRE_OP_CHECK;
		read_PolicyCheck(&ch, req.op.check);
		copy_text(out->u.check.agent_id, sizeof(out->u.check.agent_id),
			  ch.agentId);
		copy_text(out->u.check.tool, sizeof(out->u.check.tool), ch.tool);
		copy_text(out->u.check.action, sizeof(out->u.check.action),
			  ch.action);
		copy_text(out->u.check.path, sizeof(out->u.check.path), ch.path);
		break;
	}
	case PolicyRequest_op_admit: {
		struct PolicyAdmit ad;

		out->op = WIRE_OP_ADMIT;
		read_PolicyAdmit(&ad, req.op.admit);
		copy_text(out->u.admit.agent_id, sizeof(out->u.admit.agent_id),
			  ad.agentId);
		copy_text(out->u.admit.kind, sizeof(out->u.admit.kind), ad.kind);
		copy_text(out->u.admit.detail, sizeof(out->u.admit.detail),
			  ad.detail);
		break;
	}
	case PolicyRequest_op_agentStatus: {
		struct AgentQuery aq;

		out->op = WIRE_OP_AGENT_STATUS;
		read_AgentQuery(&aq, req.op.agentStatus);
		copy_text(out->u.agent.agent_id, sizeof(out->u.agent.agent_id),
			  aq.agentId);
		break;
	}
	default:
		out->op = WIRE_OP_UNKNOWN;
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

int wire_encode_response(const struct wire_response *resp, uint8_t **out,
			 size_t *out_len)
{
	struct capn c;
	capn_ptr cr;
	struct capn_segment *cs;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	PolicyEnvelope_ptr ep;
	PolicyResponse_ptr rp;
	uint8_t *buf = NULL;
	size_t cap = 8192U;
	int64_t n;
	const size_t cap_max = 1024U * 1024U;

	if (!resp || !out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	cr = capn_root(&c);
	cs = cr.seg;

	memset(&env, 0, sizeof(env));
	memset(&pr, 0, sizeof(pr));
	env.protocolVersion = 1;
	env.traceId = ctext(resp->trace_id);
	env.body_which = PolicyEnvelope_body_response;

	switch (resp->kind) {
	case WIRE_RESP_STATUS: {
		struct PolicydStatus st;
		PolicydStatus_ptr sp;

		memset(&st, 0, sizeof(st));
		st.version = ctext(resp->u.status.version);
		st.apiVersion = resp->u.status.api_version;
		st.stateDir = ctext(resp->u.status.state_dir);
		st.runtimeDir = ctext(resp->u.status.runtime_dir);
		st.socket = ctext(resp->u.status.socket);
		st.ready = resp->u.status.ready ? 1 : 0;
		sp = new_PolicydStatus(cs);
		write_PolicydStatus(&st, sp);
		pr.ok_which = PolicyResponse_ok_status;
		pr.ok.status = sp;
		break;
	}
	case WIRE_RESP_CHECK:
	case WIRE_RESP_ADMIT: {
		struct PolicyDecision d;
		PolicyDecision_ptr dp;

		memset(&d, 0, sizeof(d));
		d.decision = (enum Decision)resp->u.decision.decision;
		d.reason = ctext(resp->u.decision.reason);
		d.agentId = ctext(resp->u.decision.agent_id);
		d.tool = ctext(resp->u.decision.tool);
		d.action = ctext(resp->u.decision.action);
		dp = new_PolicyDecision(cs);
		write_PolicyDecision(&d, dp);
		if (resp->kind == WIRE_RESP_CHECK) {
			pr.ok_which = PolicyResponse_ok_check;
			pr.ok.check = dp;
		} else {
			pr.ok_which = PolicyResponse_ok_admit;
			pr.ok.admit = dp;
		}
		break;
	}
	case WIRE_RESP_AGENT: {
		struct AgentStatusWire a;
		AgentStatusWire_ptr ap;

		memset(&a, 0, sizeof(a));
		a.id = ctext(resp->u.agent.id);
		a.state = (enum AgentStateWire)resp->u.agent.state;
		a.pid = resp->u.agent.pid;
		a.pgid = resp->u.agent.pgid;
		a.exitStatus = resp->u.agent.exit_status;
		a.mode = ctext(resp->u.agent.mode);
		a.workspace = ctext(resp->u.agent.workspace);
		a.hasCgroup = resp->u.agent.has_cgroup ? 1 : 0;
		ap = new_AgentStatusWire(cs);
		write_AgentStatusWire(&a, ap);
		pr.ok_which = PolicyResponse_ok_agentStatus;
		pr.ok.agentStatus = ap;
		break;
	}
	case WIRE_RESP_ERROR:
	default: {
		struct PolicyError e;
		PolicyError_ptr ep2;

		memset(&e, 0, sizeof(e));
		e.code = resp->u.error.code;
		e.message = ctext(resp->u.error.message);
		ep2 = new_PolicyError(cs);
		write_PolicyError(&e, ep2);
		pr.ok_which = PolicyResponse_ok_error;
		pr.ok.error = ep2;
		break;
	}
	}

	rp = new_PolicyResponse(cs);
	write_PolicyResponse(&pr, rp);
	env.body.response = rp;
	ep = new_PolicyEnvelope(cs);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0) {
		capn_free(&c);
		return -1;
	}

	for (;;) {
		/* INT30-C-style: cap *= 2 must not wrap size_t. */
		if (cap == 0 || cap > cap_max) {
			capn_free(&c);
			return -1;
		}
		buf = malloc(cap);
		if (!buf) {
			capn_free(&c);
			return -1;
		}
		n = capn_write_mem(&c, buf, cap, 0);
		if (n >= 0)
			break;
		free(buf);
		buf = NULL;
		if (cap > cap_max / 2U) {
			capn_free(&c);
			return -1;
		}
		cap *= 2U;
	}
	/* n >= 0 here; reject absurd positive sizes past our body max. */
	if ((uint64_t)n > (uint64_t)GROK_NNG_MAX_BODY) {
		free(buf);
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	*out = buf;
	*out_len = (size_t)n;
	return 0;
}
