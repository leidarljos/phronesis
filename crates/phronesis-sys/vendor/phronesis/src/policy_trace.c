/* SPDX-License-Identifier: MIT */
/*
 * TraceEvent ring buffer + JSON export (playground TRACE builds only).
 */
#include "policy_trace.h"

#ifndef PHRONESIS_TRACE
/* Product builds compile this unit without TRACE symbols. */
typedef int pd_trace_product_empty_tu;
#else

#include "janet.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PD_TRACE_RING 64
#define PD_TRACE_SPANS_MAX 8
#define PD_TRACE_NAME_MAX 128
#define PD_TRACE_DETAIL_MAX 256
#define PD_TRACE_ROLE_MAX 32
#define PD_TRACE_DECISION_MAX 16

typedef struct {
	int index;
	char role[PD_TRACE_ROLE_MAX];
} pd_trace_span_t;

typedef struct {
	uint32_t seq;
	pd_trace_layer_t layer;
	pd_trace_phase_t phase;
	char name[PD_TRACE_NAME_MAX];
	char detail[PD_TRACE_DETAIL_MAX];
	pd_trace_span_t spans[PD_TRACE_SPANS_MAX];
	int nspans;
	int code; /* -1 → JSON null */
	char decision[PD_TRACE_DECISION_MAX]; /* empty → JSON null */
	int short_circuit;
	int used;
} pd_trace_event_t;

static pd_trace_event_t g_ring[PD_TRACE_RING];
static size_t g_head; /* next write slot */
static size_t g_count;
static uint32_t g_seq;
static pd_trace_event_t g_pending;
static int g_pending_open;

static const char *layer_str(pd_trace_layer_t layer)
{
	switch (layer) {
	case PD_TRACE_LAYER_PACK:
		return "pack";
	case PD_TRACE_LAYER_HOST:
		return "host";
	case PD_TRACE_LAYER_CAPNP:
		return "capnp";
	default:
		return "host";
	}
}

static const char *phase_str(pd_trace_phase_t phase)
{
	switch (phase) {
	case PD_TRACE_PHASE_ENTER:
		return "enter";
	case PD_TRACE_PHASE_GATE:
		return "gate";
	case PD_TRACE_PHASE_MATCH:
		return "match";
	case PD_TRACE_PHASE_SKIP:
		return "skip";
	case PD_TRACE_PHASE_DECIDE:
		return "decide";
	case PD_TRACE_PHASE_ERROR:
		return "error";
	default:
		return "enter";
	}
}

static void copy_trunc(char *dst, size_t dst_n, const char *src)
{
	size_t n;

	if (!dst || dst_n == 0)
		return;
	if (!src) {
		dst[0] = '\0';
		return;
	}
	n = strlen(src);
	if (n >= dst_n)
		n = dst_n - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

static void event_reset(pd_trace_event_t *e)
{
	memset(e, 0, sizeof(*e));
	e->code = -1;
}

static void ring_push(const pd_trace_event_t *src)
{
	pd_trace_event_t *dst = &g_ring[g_head];

	*dst = *src;
	dst->used = 1;
	g_head = (g_head + 1) % PD_TRACE_RING;
	if (g_count < PD_TRACE_RING)
		g_count++;
}

void pd_trace_clear(void)
{
	memset(g_ring, 0, sizeof(g_ring));
	g_head = 0;
	g_count = 0;
	g_seq = 0;
	event_reset(&g_pending);
	g_pending_open = 0;
}

void pd_trace_begin(pd_trace_layer_t layer, pd_trace_phase_t phase,
		    const char *name, const char *detail)
{
	event_reset(&g_pending);
	g_pending.seq = ++g_seq;
	g_pending.layer = layer;
	g_pending.phase = phase;
	copy_trunc(g_pending.name, sizeof(g_pending.name), name);
	copy_trunc(g_pending.detail, sizeof(g_pending.detail), detail);
	g_pending_open = 1;
}

void pd_trace_span_argv(int index, const char *role)
{
	pd_trace_span_t *s;

	if (!g_pending_open || g_pending.nspans >= PD_TRACE_SPANS_MAX)
		return;
	s = &g_pending.spans[g_pending.nspans++];
	s->index = index;
	copy_trunc(s->role, sizeof(s->role), role ? role : "");
}

void pd_trace_set_code(int code)
{
	if (!g_pending_open)
		return;
	g_pending.code = code;
}

void pd_trace_set_decision(const char *decision)
{
	if (!g_pending_open)
		return;
	copy_trunc(g_pending.decision, sizeof(g_pending.decision), decision);
}

void pd_trace_set_short_circuit(int short_circuit)
{
	if (!g_pending_open)
		return;
	g_pending.short_circuit = short_circuit ? 1 : 0;
}

void pd_trace_commit(void)
{
	if (!g_pending_open)
		return;
	ring_push(&g_pending);
	event_reset(&g_pending);
	g_pending_open = 0;
}

void pd_trace_event(pd_trace_layer_t layer, pd_trace_phase_t phase,
		    const char *name, const char *detail, int code,
		    const char *decision, int short_circuit)
{
	pd_trace_begin(layer, phase, name, detail);
	if (code >= 0)
		pd_trace_set_code(code);
	if (decision && decision[0])
		pd_trace_set_decision(decision);
	pd_trace_set_short_circuit(short_circuit);
	pd_trace_commit();
}

static int json_escape_append(char **buf, size_t *len, size_t *cap,
			      const char *s)
{
	const unsigned char *p;

	if (!s)
		s = "";
	for (p = (const unsigned char *)s; *p; p++) {
		char esc[8];
		const char *out;
		size_t out_n;
		size_t need;

		if (*p == '"' || *p == '\\') {
			esc[0] = '\\';
			esc[1] = (char)*p;
			esc[2] = '\0';
			out = esc;
			out_n = 2;
		} else if (*p == '\n') {
			out = "\\n";
			out_n = 2;
		} else if (*p == '\r') {
			out = "\\r";
			out_n = 2;
		} else if (*p == '\t') {
			out = "\\t";
			out_n = 2;
		} else if (*p < 0x20) {
			snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
			out = esc;
			out_n = 6;
		} else {
			esc[0] = (char)*p;
			esc[1] = '\0';
			out = esc;
			out_n = 1;
		}
		need = *len + out_n + 1;
		if (need > *cap) {
			size_t ncap = *cap ? *cap * 2U : 4096U;
			char *nbuf;

			while (ncap < need)
				ncap *= 2U;
			nbuf = realloc(*buf, ncap);
			if (!nbuf)
				return -1;
			*buf = nbuf;
			*cap = ncap;
		}
		memcpy(*buf + *len, out, out_n);
		*len += out_n;
		(*buf)[*len] = '\0';
	}
	return 0;
}

static int json_append(char **buf, size_t *len, size_t *cap, const char *s)
{
	size_t n;
	size_t need;
	char *nbuf;

	if (!s)
		s = "";
	n = strlen(s);
	need = *len + n + 1;
	if (need > *cap) {
		size_t ncap = *cap ? *cap * 2U : 4096U;

		while (ncap < need)
			ncap *= 2U;
		nbuf = realloc(*buf, ncap);
		if (!nbuf)
			return -1;
		*buf = nbuf;
		*cap = ncap;
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = '\0';
	return 0;
}

static int json_append_fmt(char **buf, size_t *len, size_t *cap, const char *fmt,
			   ...)
{
	char tmp[128];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (n < 0)
		return -1;
	if ((size_t)n >= sizeof(tmp)) {
		/* Truncate long numbers; fields are small. */
		tmp[sizeof(tmp) - 1] = '\0';
	}
	return json_append(buf, len, cap, tmp);
}

char *pd_trace_to_json(void)
{
	char *buf = NULL;
	size_t len = 0;
	size_t cap = 0;
	size_t i;
	size_t start;

	if (json_append(&buf, &len, &cap, "[") != 0)
		goto fail;

	/* Oldest first: head points at next write; oldest is head when full. */
	if (g_count == PD_TRACE_RING)
		start = g_head;
	else
		start = 0;

	for (i = 0; i < g_count; i++) {
		const pd_trace_event_t *e;
		size_t si;
		size_t idx;

		if (g_count == PD_TRACE_RING)
			idx = (start + i) % PD_TRACE_RING;
		else
			idx = i;
		e = &g_ring[idx];
		if (!e->used)
			continue;

		if (i > 0 && json_append(&buf, &len, &cap, ",") != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "{") != 0)
			goto fail;
		if (json_append_fmt(&buf, &len, &cap, "\"seq\":%u,",
				    (unsigned)e->seq) != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "\"layer\":\"") != 0 ||
		    json_escape_append(&buf, &len, &cap, layer_str(e->layer)) !=
			    0 ||
		    json_append(&buf, &len, &cap, "\",") != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "\"phase\":\"") != 0 ||
		    json_escape_append(&buf, &len, &cap, phase_str(e->phase)) !=
			    0 ||
		    json_append(&buf, &len, &cap, "\",") != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "\"name\":\"") != 0 ||
		    json_escape_append(&buf, &len, &cap, e->name) != 0 ||
		    json_append(&buf, &len, &cap, "\",") != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "\"detail\":\"") != 0 ||
		    json_escape_append(&buf, &len, &cap, e->detail) != 0 ||
		    json_append(&buf, &len, &cap, "\",") != 0)
			goto fail;
		if (json_append(&buf, &len, &cap, "\"spans\":[") != 0)
			goto fail;
		for (si = 0; si < (size_t)e->nspans; si++) {
			if (si > 0 && json_append(&buf, &len, &cap, ",") != 0)
				goto fail;
			if (json_append(&buf, &len, &cap,
					"{\"target\":\"argv\",\"index\":") !=
				    0 ||
			    json_append_fmt(&buf, &len, &cap, "%d",
					    e->spans[si].index) != 0 ||
			    json_append(&buf, &len, &cap, ",\"role\":\"") != 0 ||
			    json_escape_append(&buf, &len, &cap,
					       e->spans[si].role) != 0 ||
			    json_append(&buf, &len, &cap, "\"}") != 0)
				goto fail;
		}
		if (json_append(&buf, &len, &cap, "],") != 0)
			goto fail;
		if (e->code < 0) {
			if (json_append(&buf, &len, &cap, "\"code\":null,") !=
			    0)
				goto fail;
		} else if (json_append_fmt(&buf, &len, &cap, "\"code\":%d,",
					   e->code) != 0) {
			goto fail;
		}
		if (e->decision[0]) {
			if (json_append(&buf, &len, &cap, "\"decision\":\"") !=
				    0 ||
			    json_escape_append(&buf, &len, &cap, e->decision) !=
				    0 ||
			    json_append(&buf, &len, &cap, "\",") != 0)
				goto fail;
		} else if (json_append(&buf, &len, &cap,
				       "\"decision\":null,") != 0) {
			goto fail;
		}
		if (json_append(&buf, &len, &cap,
				e->short_circuit ? "\"shortCircuit\":true" :
						   "\"shortCircuit\":false") !=
		    0)
			goto fail;
		if (json_append(&buf, &len, &cap, "}") != 0)
			goto fail;
	}

	if (json_append(&buf, &len, &cap, "]") != 0)
		goto fail;
	return buf;

fail:
	free(buf);
	return NULL;
}

/* --- Janet host Cfuns (optional pack-side spans without editing product packs) --- */

static pd_trace_layer_t layer_from_kw(const char *s)
{
	if (s && strcmp(s, "pack") == 0)
		return PD_TRACE_LAYER_PACK;
	if (s && strcmp(s, "capnp") == 0)
		return PD_TRACE_LAYER_CAPNP;
	return PD_TRACE_LAYER_HOST;
}

static pd_trace_phase_t phase_from_kw(const char *s)
{
	if (!s)
		return PD_TRACE_PHASE_ENTER;
	if (strcmp(s, "gate") == 0)
		return PD_TRACE_PHASE_GATE;
	if (strcmp(s, "match") == 0)
		return PD_TRACE_PHASE_MATCH;
	if (strcmp(s, "skip") == 0)
		return PD_TRACE_PHASE_SKIP;
	if (strcmp(s, "decide") == 0)
		return PD_TRACE_PHASE_DECIDE;
	if (strcmp(s, "error") == 0)
		return PD_TRACE_PHASE_ERROR;
	return PD_TRACE_PHASE_ENTER;
}

static const char *janet_cstr(Janet v)
{
	if (janet_checktype(v, JANET_STRING) ||
	    janet_checktype(v, JANET_SYMBOL) ||
	    janet_checktype(v, JANET_KEYWORD))
		return (const char *)janet_unwrap_string(v);
	return "";
}

/* (trace-event layer phase name &opt detail code decision short-circuit) */
static Janet cfun_trace_event(int32_t argc, Janet *argv)
{
	const char *layer;
	const char *phase;
	const char *name;
	const char *detail = "";
	int code = -1;
	const char *decision = NULL;
	int sc = 0;

	janet_arity(argc, 3, 7);
	layer = janet_cstr(argv[0]);
	phase = janet_cstr(argv[1]);
	name = janet_cstr(argv[2]);
	if (argc > 3)
		detail = janet_cstr(argv[3]);
	if (argc > 4 && !janet_checktype(argv[4], JANET_NIL))
		code = janet_getinteger(argv, 4);
	if (argc > 5 && !janet_checktype(argv[5], JANET_NIL))
		decision = janet_cstr(argv[5]);
	if (argc > 6 && !janet_checktype(argv[6], JANET_NIL))
		sc = janet_truthy(argv[6]) ? 1 : 0;
	pd_trace_event(layer_from_kw(layer), phase_from_kw(phase), name, detail,
		       code, decision, sc);
	return janet_wrap_nil();
}

/* (trace-begin layer phase name &opt detail) */
static Janet cfun_trace_begin(int32_t argc, Janet *argv)
{
	const char *detail = "";

	janet_arity(argc, 3, 4);
	if (argc > 3)
		detail = janet_cstr(argv[3]);
	pd_trace_begin(layer_from_kw(janet_cstr(argv[0])),
		       phase_from_kw(janet_cstr(argv[1])),
		       janet_cstr(argv[2]), detail);
	return janet_wrap_nil();
}

/* (trace-span-argv index role) */
static Janet cfun_trace_span_argv(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 2);
	pd_trace_span_argv(janet_getinteger(argv, 0), janet_cstr(argv[1]));
	return janet_wrap_nil();
}

/* (trace-commit &opt code decision short-circuit) */
static Janet cfun_trace_commit(int32_t argc, Janet *argv)
{
	janet_arity(argc, 0, 3);
	if (argc > 0 && !janet_checktype(argv[0], JANET_NIL))
		pd_trace_set_code(janet_getinteger(argv, 0));
	if (argc > 1 && !janet_checktype(argv[1], JANET_NIL))
		pd_trace_set_decision(janet_cstr(argv[1]));
	if (argc > 2 && !janet_checktype(argv[2], JANET_NIL))
		pd_trace_set_short_circuit(janet_truthy(argv[2]) ? 1 : 0);
	pd_trace_commit();
	return janet_wrap_nil();
}

static const JanetReg pd_trace_cfuns[] = {
	{ "trace-event", cfun_trace_event,
	  "(trace-event layer phase name &opt detail code decision sc)\n\n"
	  "Append a TraceEvent (playground TRACE builds)." },
	{ "trace-begin", cfun_trace_begin,
	  "(trace-begin layer phase name &opt detail)\n\n"
	  "Start a TraceEvent; attach spans then (trace-commit)." },
	{ "trace-span-argv", cfun_trace_span_argv,
	  "(trace-span-argv index role)\n\n"
	  "Attach an argv span to the open TraceEvent." },
	{ "trace-commit", cfun_trace_commit,
	  "(trace-commit &opt code decision short-circuit)\n\n"
	  "Commit the open TraceEvent into the ring." },
	{ NULL, NULL, NULL }
};

void pd_trace_register_janet(void *janet_env)
{
	JanetTable *env = (JanetTable *)janet_env;

	if (!env)
		return;
	janet_cfuns(env, "pd-trace", pd_trace_cfuns);
}

#endif /* PHRONESIS_TRACE */
