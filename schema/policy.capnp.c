#include "policy.capnp.h"
/* AUTO GENERATED - DO NOT EDIT */
#ifdef __GNUC__
# define capnp_unused __attribute__((unused))
# define capnp_use(x) (void) x;
#else
# define capnp_unused
# define capnp_use(x)
#endif

static const capn_text capn_val0 = {0,"",0};

PolicyEnvelope_ptr new_PolicyEnvelope(struct capn_segment *s) {
	PolicyEnvelope_ptr p;
	p.p = capn_new_struct(s, 8, 2);
	return p;
}
PolicyEnvelope_list new_PolicyEnvelope_list(struct capn_segment *s, int len) {
	PolicyEnvelope_list p;
	p.p = capn_new_list(s, len, 8, 2);
	return p;
}
void read_PolicyEnvelope(struct PolicyEnvelope *s capnp_unused, PolicyEnvelope_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->protocolVersion = capn_read32(p.p, 0) ^ 1u;
	s->traceId = capn_get_text(p.p, 0, capn_val0);
	s->body_which = (enum PolicyEnvelope_body_which)(int) capn_read16(p.p, 4);
	switch (s->body_which) {
	case PolicyEnvelope_body_request:
	case PolicyEnvelope_body_response:
		s->body.response.p = capn_getp(p.p, 1, 0);
		break;
	default:
		break;
	}
}
void write_PolicyEnvelope(const struct PolicyEnvelope *s capnp_unused, PolicyEnvelope_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write32(p.p, 0, s->protocolVersion ^ 1u);
	capn_set_text(p.p, 0, s->traceId);
	capn_write16(p.p, 4, s->body_which);
	switch (s->body_which) {
	case PolicyEnvelope_body_request:
	case PolicyEnvelope_body_response:
		capn_setp(p.p, 1, s->body.response.p);
		break;
	default:
		break;
	}
}
void get_PolicyEnvelope(struct PolicyEnvelope *s, PolicyEnvelope_list l, int i) {
	PolicyEnvelope_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyEnvelope(s, p);
}
void set_PolicyEnvelope(const struct PolicyEnvelope *s, PolicyEnvelope_list l, int i) {
	PolicyEnvelope_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyEnvelope(s, p);
}

PolicyRequest_ptr new_PolicyRequest(struct capn_segment *s) {
	PolicyRequest_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
PolicyRequest_list new_PolicyRequest_list(struct capn_segment *s, int len) {
	PolicyRequest_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_PolicyRequest(struct PolicyRequest *s capnp_unused, PolicyRequest_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->op_which = (enum PolicyRequest_op_which)(int) capn_read16(p.p, 0);
	switch (s->op_which) {
	case PolicyRequest_op_check:
	case PolicyRequest_op_admit:
	case PolicyRequest_op_agentStatus:
		s->op.agentStatus.p = capn_getp(p.p, 0, 0);
		break;
	default:
		break;
	}
}
void write_PolicyRequest(const struct PolicyRequest *s capnp_unused, PolicyRequest_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, s->op_which);
	switch (s->op_which) {
	case PolicyRequest_op_check:
	case PolicyRequest_op_admit:
	case PolicyRequest_op_agentStatus:
		capn_setp(p.p, 0, s->op.agentStatus.p);
		break;
	default:
		break;
	}
}
void get_PolicyRequest(struct PolicyRequest *s, PolicyRequest_list l, int i) {
	PolicyRequest_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyRequest(s, p);
}
void set_PolicyRequest(const struct PolicyRequest *s, PolicyRequest_list l, int i) {
	PolicyRequest_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyRequest(s, p);
}

PolicyCheck_ptr new_PolicyCheck(struct capn_segment *s) {
	PolicyCheck_ptr p;
	p.p = capn_new_struct(s, 0, 4);
	return p;
}
PolicyCheck_list new_PolicyCheck_list(struct capn_segment *s, int len) {
	PolicyCheck_list p;
	p.p = capn_new_list(s, len, 0, 4);
	return p;
}
void read_PolicyCheck(struct PolicyCheck *s capnp_unused, PolicyCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId = capn_get_text(p.p, 0, capn_val0);
	s->tool = capn_get_text(p.p, 1, capn_val0);
	s->action = capn_get_text(p.p, 2, capn_val0);
	s->path = capn_get_text(p.p, 3, capn_val0);
}
void write_PolicyCheck(const struct PolicyCheck *s capnp_unused, PolicyCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->agentId);
	capn_set_text(p.p, 1, s->tool);
	capn_set_text(p.p, 2, s->action);
	capn_set_text(p.p, 3, s->path);
}
void get_PolicyCheck(struct PolicyCheck *s, PolicyCheck_list l, int i) {
	PolicyCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyCheck(s, p);
}
void set_PolicyCheck(const struct PolicyCheck *s, PolicyCheck_list l, int i) {
	PolicyCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyCheck(s, p);
}

PolicyAdmit_ptr new_PolicyAdmit(struct capn_segment *s) {
	PolicyAdmit_ptr p;
	p.p = capn_new_struct(s, 0, 3);
	return p;
}
PolicyAdmit_list new_PolicyAdmit_list(struct capn_segment *s, int len) {
	PolicyAdmit_list p;
	p.p = capn_new_list(s, len, 0, 3);
	return p;
}
void read_PolicyAdmit(struct PolicyAdmit *s capnp_unused, PolicyAdmit_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId = capn_get_text(p.p, 0, capn_val0);
	s->kind = capn_get_text(p.p, 1, capn_val0);
	s->detail = capn_get_text(p.p, 2, capn_val0);
}
void write_PolicyAdmit(const struct PolicyAdmit *s capnp_unused, PolicyAdmit_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->agentId);
	capn_set_text(p.p, 1, s->kind);
	capn_set_text(p.p, 2, s->detail);
}
void get_PolicyAdmit(struct PolicyAdmit *s, PolicyAdmit_list l, int i) {
	PolicyAdmit_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyAdmit(s, p);
}
void set_PolicyAdmit(const struct PolicyAdmit *s, PolicyAdmit_list l, int i) {
	PolicyAdmit_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyAdmit(s, p);
}

AgentQuery_ptr new_AgentQuery(struct capn_segment *s) {
	AgentQuery_ptr p;
	p.p = capn_new_struct(s, 0, 1);
	return p;
}
AgentQuery_list new_AgentQuery_list(struct capn_segment *s, int len) {
	AgentQuery_list p;
	p.p = capn_new_list(s, len, 0, 1);
	return p;
}
void read_AgentQuery(struct AgentQuery *s capnp_unused, AgentQuery_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId = capn_get_text(p.p, 0, capn_val0);
}
void write_AgentQuery(const struct AgentQuery *s capnp_unused, AgentQuery_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->agentId);
}
void get_AgentQuery(struct AgentQuery *s, AgentQuery_list l, int i) {
	AgentQuery_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AgentQuery(s, p);
}
void set_AgentQuery(const struct AgentQuery *s, AgentQuery_list l, int i) {
	AgentQuery_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AgentQuery(s, p);
}

PolicyResponse_ptr new_PolicyResponse(struct capn_segment *s) {
	PolicyResponse_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
PolicyResponse_list new_PolicyResponse_list(struct capn_segment *s, int len) {
	PolicyResponse_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_PolicyResponse(struct PolicyResponse *s capnp_unused, PolicyResponse_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->ok_which = (enum PolicyResponse_ok_which)(int) capn_read16(p.p, 0);
	switch (s->ok_which) {
	case PolicyResponse_ok_status:
	case PolicyResponse_ok_check:
	case PolicyResponse_ok_admit:
	case PolicyResponse_ok_agentStatus:
	case PolicyResponse_ok_error:
		s->ok.error.p = capn_getp(p.p, 0, 0);
		break;
	default:
		break;
	}
}
void write_PolicyResponse(const struct PolicyResponse *s capnp_unused, PolicyResponse_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, s->ok_which);
	switch (s->ok_which) {
	case PolicyResponse_ok_status:
	case PolicyResponse_ok_check:
	case PolicyResponse_ok_admit:
	case PolicyResponse_ok_agentStatus:
	case PolicyResponse_ok_error:
		capn_setp(p.p, 0, s->ok.error.p);
		break;
	default:
		break;
	}
}
void get_PolicyResponse(struct PolicyResponse *s, PolicyResponse_list l, int i) {
	PolicyResponse_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyResponse(s, p);
}
void set_PolicyResponse(const struct PolicyResponse *s, PolicyResponse_list l, int i) {
	PolicyResponse_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyResponse(s, p);
}

PolicydStatus_ptr new_PolicydStatus(struct capn_segment *s) {
	PolicydStatus_ptr p;
	p.p = capn_new_struct(s, 8, 4);
	return p;
}
PolicydStatus_list new_PolicydStatus_list(struct capn_segment *s, int len) {
	PolicydStatus_list p;
	p.p = capn_new_list(s, len, 8, 4);
	return p;
}
void read_PolicydStatus(struct PolicydStatus *s capnp_unused, PolicydStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->version = capn_get_text(p.p, 0, capn_val0);
	s->apiVersion = (int32_t) ((int32_t)capn_read32(p.p, 0));
	s->stateDir = capn_get_text(p.p, 1, capn_val0);
	s->runtimeDir = capn_get_text(p.p, 2, capn_val0);
	s->socket = capn_get_text(p.p, 3, capn_val0);
	s->ready = (capn_read8(p.p, 4) & 1) != 0;
}
void write_PolicydStatus(const struct PolicydStatus *s capnp_unused, PolicydStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->version);
	capn_write32(p.p, 0, (uint32_t) (s->apiVersion));
	capn_set_text(p.p, 1, s->stateDir);
	capn_set_text(p.p, 2, s->runtimeDir);
	capn_set_text(p.p, 3, s->socket);
	capn_write1(p.p, 32, s->ready != 0);
}
void get_PolicydStatus(struct PolicydStatus *s, PolicydStatus_list l, int i) {
	PolicydStatus_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicydStatus(s, p);
}
void set_PolicydStatus(const struct PolicydStatus *s, PolicydStatus_list l, int i) {
	PolicydStatus_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicydStatus(s, p);
}

PolicyDecision_ptr new_PolicyDecision(struct capn_segment *s) {
	PolicyDecision_ptr p;
	p.p = capn_new_struct(s, 8, 4);
	return p;
}
PolicyDecision_list new_PolicyDecision_list(struct capn_segment *s, int len) {
	PolicyDecision_list p;
	p.p = capn_new_list(s, len, 8, 4);
	return p;
}
void read_PolicyDecision(struct PolicyDecision *s capnp_unused, PolicyDecision_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->decision = (enum Decision)(int) capn_read16(p.p, 0);
	s->reason = capn_get_text(p.p, 0, capn_val0);
	s->agentId = capn_get_text(p.p, 1, capn_val0);
	s->tool = capn_get_text(p.p, 2, capn_val0);
	s->action = capn_get_text(p.p, 3, capn_val0);
}
void write_PolicyDecision(const struct PolicyDecision *s capnp_unused, PolicyDecision_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, (uint16_t) (s->decision));
	capn_set_text(p.p, 0, s->reason);
	capn_set_text(p.p, 1, s->agentId);
	capn_set_text(p.p, 2, s->tool);
	capn_set_text(p.p, 3, s->action);
}
void get_PolicyDecision(struct PolicyDecision *s, PolicyDecision_list l, int i) {
	PolicyDecision_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyDecision(s, p);
}
void set_PolicyDecision(const struct PolicyDecision *s, PolicyDecision_list l, int i) {
	PolicyDecision_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyDecision(s, p);
}

AgentStatusWire_ptr new_AgentStatusWire(struct capn_segment *s) {
	AgentStatusWire_ptr p;
	p.p = capn_new_struct(s, 16, 3);
	return p;
}
AgentStatusWire_list new_AgentStatusWire_list(struct capn_segment *s, int len) {
	AgentStatusWire_list p;
	p.p = capn_new_list(s, len, 16, 3);
	return p;
}
void read_AgentStatusWire(struct AgentStatusWire *s capnp_unused, AgentStatusWire_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->id = capn_get_text(p.p, 0, capn_val0);
	s->state = (enum AgentStateWire)(int) capn_read16(p.p, 0);
	s->pid = (int32_t) ((int32_t)capn_read32(p.p, 4));
	s->pgid = (int32_t) ((int32_t)capn_read32(p.p, 8));
	s->exitStatus = (int32_t) ((int32_t)capn_read32(p.p, 12));
	s->mode = capn_get_text(p.p, 1, capn_val0);
	s->workspace = capn_get_text(p.p, 2, capn_val0);
	s->hasCgroup = (capn_read8(p.p, 2) & 1) != 0;
}
void write_AgentStatusWire(const struct AgentStatusWire *s capnp_unused, AgentStatusWire_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->id);
	capn_write16(p.p, 0, (uint16_t) (s->state));
	capn_write32(p.p, 4, (uint32_t) (s->pid));
	capn_write32(p.p, 8, (uint32_t) (s->pgid));
	capn_write32(p.p, 12, (uint32_t) (s->exitStatus));
	capn_set_text(p.p, 1, s->mode);
	capn_set_text(p.p, 2, s->workspace);
	capn_write1(p.p, 16, s->hasCgroup != 0);
}
void get_AgentStatusWire(struct AgentStatusWire *s, AgentStatusWire_list l, int i) {
	AgentStatusWire_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AgentStatusWire(s, p);
}
void set_AgentStatusWire(const struct AgentStatusWire *s, AgentStatusWire_list l, int i) {
	AgentStatusWire_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AgentStatusWire(s, p);
}

PolicyError_ptr new_PolicyError(struct capn_segment *s) {
	PolicyError_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
PolicyError_list new_PolicyError_list(struct capn_segment *s, int len) {
	PolicyError_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_PolicyError(struct PolicyError *s capnp_unused, PolicyError_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->code = (int32_t) ((int32_t)capn_read32(p.p, 0));
	s->message = capn_get_text(p.p, 0, capn_val0);
}
void write_PolicyError(const struct PolicyError *s capnp_unused, PolicyError_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write32(p.p, 0, (uint32_t) (s->code));
	capn_set_text(p.p, 0, s->message);
}
void get_PolicyError(struct PolicyError *s, PolicyError_list l, int i) {
	PolicyError_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PolicyError(s, p);
}
void set_PolicyError(const struct PolicyError *s, PolicyError_list l, int i) {
	PolicyError_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PolicyError(s, p);
}
