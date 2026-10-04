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

PolicyDecision_ptr new_PolicyDecision(struct capn_segment *s) {
	PolicyDecision_ptr p;
	p.p = capn_new_struct(s, 8, 2);
	return p;
}
PolicyDecision_list new_PolicyDecision_list(struct capn_segment *s, int len) {
	PolicyDecision_list p;
	p.p = capn_new_list(s, len, 8, 2);
	return p;
}
void read_PolicyDecision(struct PolicyDecision *s capnp_unused, PolicyDecision_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->decision = (enum Decision)(int) capn_read16(p.p, 0);
	s->reason = capn_get_text(p.p, 0, capn_val0);
	s->agentId.p = capn_getp(p.p, 1, 0);
	s->code = (enum PolicyReason)(int) capn_read16(p.p, 2);
}
void write_PolicyDecision(const struct PolicyDecision *s capnp_unused, PolicyDecision_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, (uint16_t) (s->decision));
	capn_set_text(p.p, 0, s->reason);
	capn_setp(p.p, 1, s->agentId.p);
	capn_write16(p.p, 2, (uint16_t) (s->code));
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

PolicydStatus_ptr new_PolicydStatus(struct capn_segment *s) {
	PolicydStatus_ptr p;
	p.p = capn_new_struct(s, 8, 3);
	return p;
}
PolicydStatus_list new_PolicydStatus_list(struct capn_segment *s, int len) {
	PolicydStatus_list p;
	p.p = capn_new_list(s, len, 8, 3);
	return p;
}
void read_PolicydStatus(struct PolicydStatus *s capnp_unused, PolicydStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->version = capn_get_text(p.p, 0, capn_val0);
	s->apiVersion = (int32_t) ((int32_t)capn_read32(p.p, 0));
	s->stateDir = capn_get_text(p.p, 1, capn_val0);
	s->runtimeDir = capn_get_text(p.p, 2, capn_val0);
	s->ready = (capn_read8(p.p, 4) & 1) != 0;
}
void write_PolicydStatus(const struct PolicydStatus *s capnp_unused, PolicydStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->version);
	capn_write32(p.p, 0, (uint32_t) (s->apiVersion));
	capn_set_text(p.p, 1, s->stateDir);
	capn_set_text(p.p, 2, s->runtimeDir);
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

AgentStatus_ptr new_AgentStatus(struct capn_segment *s) {
	AgentStatus_ptr p;
	p.p = capn_new_struct(s, 24, 3);
	return p;
}
AgentStatus_list new_AgentStatus_list(struct capn_segment *s, int len) {
	AgentStatus_list p;
	p.p = capn_new_list(s, len, 24, 3);
	return p;
}
void read_AgentStatus(struct AgentStatus *s capnp_unused, AgentStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->id.p = capn_getp(p.p, 0, 0);
	s->state = (enum AgentState)(int) capn_read16(p.p, 0);
	s->pid = (int32_t) ((int32_t)capn_read32(p.p, 4));
	s->pgid = (int32_t) ((int32_t)capn_read32(p.p, 8));
	s->exitStatus = (int32_t) ((int32_t)capn_read32(p.p, 12));
	s->mode = (enum SeatMode)(int) capn_read16(p.p, 2);
	s->workspace = capn_get_text(p.p, 1, capn_val0);
	s->hasCgroup = (capn_read8(p.p, 16) & 1) != 0;
	s->detail = capn_get_text(p.p, 2, capn_val0);
}
void write_AgentStatus(const struct AgentStatus *s capnp_unused, AgentStatus_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->id.p);
	capn_write16(p.p, 0, (uint16_t) (s->state));
	capn_write32(p.p, 4, (uint32_t) (s->pid));
	capn_write32(p.p, 8, (uint32_t) (s->pgid));
	capn_write32(p.p, 12, (uint32_t) (s->exitStatus));
	capn_write16(p.p, 2, (uint16_t) (s->mode));
	capn_set_text(p.p, 1, s->workspace);
	capn_write1(p.p, 128, s->hasCgroup != 0);
	capn_set_text(p.p, 2, s->detail);
}
void get_AgentStatus(struct AgentStatus *s, AgentStatus_list l, int i) {
	AgentStatus_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AgentStatus(s, p);
}
void set_AgentStatus(const struct AgentStatus *s, AgentStatus_list l, int i) {
	AgentStatus_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AgentStatus(s, p);
}

SeatCheck_ptr new_SeatCheck(struct capn_segment *s) {
	SeatCheck_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
SeatCheck_list new_SeatCheck_list(struct capn_segment *s, int len) {
	SeatCheck_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_SeatCheck(struct SeatCheck *s capnp_unused, SeatCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->action = (enum SeatAction)(int) capn_read16(p.p, 0);
}
void write_SeatCheck(const struct SeatCheck *s capnp_unused, SeatCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_write16(p.p, 0, (uint16_t) (s->action));
}
void get_SeatCheck(struct SeatCheck *s, SeatCheck_list l, int i) {
	SeatCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_SeatCheck(s, p);
}
void set_SeatCheck(const struct SeatCheck *s, SeatCheck_list l, int i) {
	SeatCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_SeatCheck(s, p);
}

ModelCheck_ptr new_ModelCheck(struct capn_segment *s) {
	ModelCheck_ptr p;
	p.p = capn_new_struct(s, 0, 2);
	return p;
}
ModelCheck_list new_ModelCheck_list(struct capn_segment *s, int len) {
	ModelCheck_list p;
	p.p = capn_new_list(s, len, 0, 2);
	return p;
}
void read_ModelCheck(struct ModelCheck *s capnp_unused, ModelCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->model = capn_get_text(p.p, 1, capn_val0);
}
void write_ModelCheck(const struct ModelCheck *s capnp_unused, ModelCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_set_text(p.p, 1, s->model);
}
void get_ModelCheck(struct ModelCheck *s, ModelCheck_list l, int i) {
	ModelCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ModelCheck(s, p);
}
void set_ModelCheck(const struct ModelCheck *s, ModelCheck_list l, int i) {
	ModelCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ModelCheck(s, p);
}

PathCheck_ptr new_PathCheck(struct capn_segment *s) {
	PathCheck_ptr p;
	p.p = capn_new_struct(s, 8, 2);
	return p;
}
PathCheck_list new_PathCheck_list(struct capn_segment *s, int len) {
	PathCheck_list p;
	p.p = capn_new_list(s, len, 8, 2);
	return p;
}
void read_PathCheck(struct PathCheck *s capnp_unused, PathCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->action = (enum PathAction)(int) capn_read16(p.p, 0);
	s->path = capn_get_text(p.p, 1, capn_val0);
}
void write_PathCheck(const struct PathCheck *s capnp_unused, PathCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_write16(p.p, 0, (uint16_t) (s->action));
	capn_set_text(p.p, 1, s->path);
}
void get_PathCheck(struct PathCheck *s, PathCheck_list l, int i) {
	PathCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PathCheck(s, p);
}
void set_PathCheck(const struct PathCheck *s, PathCheck_list l, int i) {
	PathCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PathCheck(s, p);
}

ShellCheck_ptr new_ShellCheck(struct capn_segment *s) {
	ShellCheck_ptr p;
	p.p = capn_new_struct(s, 0, 3);
	return p;
}
ShellCheck_list new_ShellCheck_list(struct capn_segment *s, int len) {
	ShellCheck_list p;
	p.p = capn_new_list(s, len, 0, 3);
	return p;
}
void read_ShellCheck(struct ShellCheck *s capnp_unused, ShellCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->cwd = capn_get_text(p.p, 1, capn_val0);
	s->argv = capn_getp(p.p, 2, 0);
}
void write_ShellCheck(const struct ShellCheck *s capnp_unused, ShellCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_set_text(p.p, 1, s->cwd);
	capn_setp(p.p, 2, s->argv);
}
void get_ShellCheck(struct ShellCheck *s, ShellCheck_list l, int i) {
	ShellCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ShellCheck(s, p);
}
void set_ShellCheck(const struct ShellCheck *s, ShellCheck_list l, int i) {
	ShellCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ShellCheck(s, p);
}

PathProbe_ptr new_PathProbe(struct capn_segment *s) {
	PathProbe_ptr p;
	p.p = capn_new_struct(s, 8, 3);
	return p;
}
PathProbe_list new_PathProbe_list(struct capn_segment *s, int len) {
	PathProbe_list p;
	p.p = capn_new_list(s, len, 8, 3);
	return p;
}
void read_PathProbe(struct PathProbe *s capnp_unused, PathProbe_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->arg = capn_get_text(p.p, 0, capn_val0);
	s->resolved = capn_get_text(p.p, 1, capn_val0);
	s->exists = (capn_read8(p.p, 0) & 1) != 0;
	s->head = capn_get_text(p.p, 2, capn_val0);
}
void write_PathProbe(const struct PathProbe *s capnp_unused, PathProbe_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->arg);
	capn_set_text(p.p, 1, s->resolved);
	capn_write1(p.p, 0, s->exists != 0);
	capn_set_text(p.p, 2, s->head);
}
void get_PathProbe(struct PathProbe *s, PathProbe_list l, int i) {
	PathProbe_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_PathProbe(s, p);
}
void set_PathProbe(const struct PathProbe *s, PathProbe_list l, int i) {
	PathProbe_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_PathProbe(s, p);
}

ShellView_ptr new_ShellView(struct capn_segment *s) {
	ShellView_ptr p;
	p.p = capn_new_struct(s, 8, 3);
	return p;
}
ShellView_list new_ShellView_list(struct capn_segment *s, int len) {
	ShellView_list p;
	p.p = capn_new_list(s, len, 8, 3);
	return p;
}
void read_ShellView(struct ShellView *s capnp_unused, ShellView_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->underWorkspace = (capn_read8(p.p, 0) & 1) != 0;
	s->cwd = capn_get_text(p.p, 0, capn_val0);
	s->argv = capn_getp(p.p, 1, 0);
	s->pathProbes.p = capn_getp(p.p, 2, 0);
}
void write_ShellView(const struct ShellView *s capnp_unused, ShellView_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write1(p.p, 0, s->underWorkspace != 0);
	capn_set_text(p.p, 0, s->cwd);
	capn_setp(p.p, 1, s->argv);
	capn_setp(p.p, 2, s->pathProbes.p);
}
void get_ShellView(struct ShellView *s, ShellView_list l, int i) {
	ShellView_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ShellView(s, p);
}
void set_ShellView(const struct ShellView *s, ShellView_list l, int i) {
	ShellView_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ShellView(s, p);
}

RiskCheck_ptr new_RiskCheck(struct capn_segment *s) {
	RiskCheck_ptr p;
	p.p = capn_new_struct(s, 8, 2);
	return p;
}
RiskCheck_list new_RiskCheck_list(struct capn_segment *s, int len) {
	RiskCheck_list p;
	p.p = capn_new_list(s, len, 8, 2);
	return p;
}
void read_RiskCheck(struct RiskCheck *s capnp_unused, RiskCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->action = (enum RiskAction)(int) capn_read16(p.p, 0);
	s->path = capn_get_text(p.p, 1, capn_val0);
}
void write_RiskCheck(const struct RiskCheck *s capnp_unused, RiskCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_write16(p.p, 0, (uint16_t) (s->action));
	capn_set_text(p.p, 1, s->path);
}
void get_RiskCheck(struct RiskCheck *s, RiskCheck_list l, int i) {
	RiskCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_RiskCheck(s, p);
}
void set_RiskCheck(const struct RiskCheck *s, RiskCheck_list l, int i) {
	RiskCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_RiskCheck(s, p);
}

AudioCheck_ptr new_AudioCheck(struct capn_segment *s) {
	AudioCheck_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
AudioCheck_list new_AudioCheck_list(struct capn_segment *s, int len) {
	AudioCheck_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_AudioCheck(struct AudioCheck *s capnp_unused, AudioCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->action = (enum AudioAction)(int) capn_read16(p.p, 0);
}
void write_AudioCheck(const struct AudioCheck *s capnp_unused, AudioCheck_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_write16(p.p, 0, (uint16_t) (s->action));
}
void get_AudioCheck(struct AudioCheck *s, AudioCheck_list l, int i) {
	AudioCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AudioCheck(s, p);
}
void set_AudioCheck(const struct AudioCheck *s, AudioCheck_list l, int i) {
	AudioCheck_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AudioCheck(s, p);
}

AdmitSeat_ptr new_AdmitSeat(struct capn_segment *s) {
	AdmitSeat_ptr p;
	p.p = capn_new_struct(s, 0, 2);
	return p;
}
AdmitSeat_list new_AdmitSeat_list(struct capn_segment *s, int len) {
	AdmitSeat_list p;
	p.p = capn_new_list(s, len, 0, 2);
	return p;
}
void read_AdmitSeat(struct AdmitSeat *s capnp_unused, AdmitSeat_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->detail = capn_get_text(p.p, 1, capn_val0);
}
void write_AdmitSeat(const struct AdmitSeat *s capnp_unused, AdmitSeat_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_set_text(p.p, 1, s->detail);
}
void get_AdmitSeat(struct AdmitSeat *s, AdmitSeat_list l, int i) {
	AdmitSeat_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AdmitSeat(s, p);
}
void set_AdmitSeat(const struct AdmitSeat *s, AdmitSeat_list l, int i) {
	AdmitSeat_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AdmitSeat(s, p);
}

AdmitModel_ptr new_AdmitModel(struct capn_segment *s) {
	AdmitModel_ptr p;
	p.p = capn_new_struct(s, 0, 2);
	return p;
}
AdmitModel_list new_AdmitModel_list(struct capn_segment *s, int len) {
	AdmitModel_list p;
	p.p = capn_new_list(s, len, 0, 2);
	return p;
}
void read_AdmitModel(struct AdmitModel *s capnp_unused, AdmitModel_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->agentId.p = capn_getp(p.p, 0, 0);
	s->detail = capn_get_text(p.p, 1, capn_val0);
}
void write_AdmitModel(const struct AdmitModel *s capnp_unused, AdmitModel_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
	capn_set_text(p.p, 1, s->detail);
}
void get_AdmitModel(struct AdmitModel *s, AdmitModel_list l, int i) {
	AdmitModel_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AdmitModel(s, p);
}
void set_AdmitModel(const struct AdmitModel *s, AdmitModel_list l, int i) {
	AdmitModel_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AdmitModel(s, p);
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
	s->agentId.p = capn_getp(p.p, 0, 0);
}
void write_AgentQuery(const struct AgentQuery *s capnp_unused, AgentQuery_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->agentId.p);
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

ReloadShellPack_ptr new_ReloadShellPack(struct capn_segment *s) {
	ReloadShellPack_ptr p;
	p.p = capn_new_struct(s, 0, 1);
	return p;
}
ReloadShellPack_list new_ReloadShellPack_list(struct capn_segment *s, int len) {
	ReloadShellPack_list p;
	p.p = capn_new_list(s, len, 0, 1);
	return p;
}
void read_ReloadShellPack(struct ReloadShellPack *s capnp_unused, ReloadShellPack_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->path = capn_get_text(p.p, 0, capn_val0);
}
void write_ReloadShellPack(const struct ReloadShellPack *s capnp_unused, ReloadShellPack_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->path);
}
void get_ReloadShellPack(struct ReloadShellPack *s, ReloadShellPack_list l, int i) {
	ReloadShellPack_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ReloadShellPack(s, p);
}
void set_ReloadShellPack(const struct ReloadShellPack *s, ReloadShellPack_list l, int i) {
	ReloadShellPack_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ReloadShellPack(s, p);
}
