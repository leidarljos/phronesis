#include "util.capnp.h"
/* AUTO GENERATED - DO NOT EDIT */
#ifdef __GNUC__
# define capnp_unused __attribute__((unused))
# define capnp_use(x) (void) x;
#else
# define capnp_unused
# define capnp_use(x)
#endif

static const capn_text capn_val0 = {0,"",0};

KeyValue_ptr new_KeyValue(struct capn_segment *s) {
	KeyValue_ptr p;
	p.p = capn_new_struct(s, 0, 2);
	return p;
}
KeyValue_list new_KeyValue_list(struct capn_segment *s, int len) {
	KeyValue_list p;
	p.p = capn_new_list(s, len, 0, 2);
	return p;
}
void read_KeyValue(struct KeyValue *s capnp_unused, KeyValue_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->key = capn_get_text(p.p, 0, capn_val0);
	s->value = capn_get_text(p.p, 1, capn_val0);
}
void write_KeyValue(const struct KeyValue *s capnp_unused, KeyValue_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->key);
	capn_set_text(p.p, 1, s->value);
}
void get_KeyValue(struct KeyValue *s, KeyValue_list l, int i) {
	KeyValue_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_KeyValue(s, p);
}
void set_KeyValue(const struct KeyValue *s, KeyValue_list l, int i) {
	KeyValue_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_KeyValue(s, p);
}

BackendState_ptr new_BackendState(struct capn_segment *s) {
	BackendState_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
BackendState_list new_BackendState_list(struct capn_segment *s, int len) {
	BackendState_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_BackendState(struct BackendState *s capnp_unused, BackendState_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->available = (capn_read8(p.p, 0) & 1) != 0;
	s->detail = capn_get_text(p.p, 0, capn_val0);
}
void write_BackendState(const struct BackendState *s capnp_unused, BackendState_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write1(p.p, 0, s->available != 0);
	capn_set_text(p.p, 0, s->detail);
}
void get_BackendState(struct BackendState *s, BackendState_list l, int i) {
	BackendState_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_BackendState(s, p);
}
void set_BackendState(const struct BackendState *s, BackendState_list l, int i) {
	BackendState_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_BackendState(s, p);
}

AgentId_ptr new_AgentId(struct capn_segment *s) {
	AgentId_ptr p;
	p.p = capn_new_struct(s, 16, 0);
	return p;
}
AgentId_list new_AgentId_list(struct capn_segment *s, int len) {
	AgentId_list p;
	p.p = capn_new_list(s, len, 16, 0);
	return p;
}
void read_AgentId(struct AgentId *s capnp_unused, AgentId_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->hi = capn_read64(p.p, 0);
	s->lo = capn_read64(p.p, 8);
}
void write_AgentId(const struct AgentId *s capnp_unused, AgentId_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write64(p.p, 0, s->hi);
	capn_write64(p.p, 8, s->lo);
}
void get_AgentId(struct AgentId *s, AgentId_list l, int i) {
	AgentId_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_AgentId(s, p);
}
void set_AgentId(const struct AgentId *s, AgentId_list l, int i) {
	AgentId_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_AgentId(s, p);
}

TraceId_ptr new_TraceId(struct capn_segment *s) {
	TraceId_ptr p;
	p.p = capn_new_struct(s, 16, 0);
	return p;
}
TraceId_list new_TraceId_list(struct capn_segment *s, int len) {
	TraceId_list p;
	p.p = capn_new_list(s, len, 16, 0);
	return p;
}
void read_TraceId(struct TraceId *s capnp_unused, TraceId_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->hi = capn_read64(p.p, 0);
	s->lo = capn_read64(p.p, 8);
}
void write_TraceId(const struct TraceId *s capnp_unused, TraceId_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write64(p.p, 0, s->hi);
	capn_write64(p.p, 8, s->lo);
}
void get_TraceId(struct TraceId *s, TraceId_list l, int i) {
	TraceId_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_TraceId(s, p);
}
void set_TraceId(const struct TraceId *s, TraceId_list l, int i) {
	TraceId_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_TraceId(s, p);
}
