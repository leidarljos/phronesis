/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Playground-only forensic TraceEvent ring (compile-gated).
 *
 * Product builds leave GROKOS_POLICYD_TRACE undefined: all macros are no-ops
 * and pd_trace_to_json returns NULL. Playground emcc sets -DGROKOS_POLICYD_TRACE=1.
 */
#ifndef POLICYD_POLICY_TRACE_H
#define POLICYD_POLICY_TRACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PD_TRACE_LAYER_PACK = 0,
	PD_TRACE_LAYER_HOST = 1,
	PD_TRACE_LAYER_CAPNP = 2
} pd_trace_layer_t;

typedef enum {
	PD_TRACE_PHASE_ENTER = 0,
	PD_TRACE_PHASE_GATE = 1,
	PD_TRACE_PHASE_MATCH = 2,
	PD_TRACE_PHASE_SKIP = 3,
	PD_TRACE_PHASE_DECIDE = 4,
	PD_TRACE_PHASE_ERROR = 5
} pd_trace_phase_t;

#ifdef GROKOS_POLICYD_TRACE

void pd_trace_clear(void);

/** One-shot event (no pending spans). code < 0 → JSON null. */
void pd_trace_event(pd_trace_layer_t layer, pd_trace_phase_t phase,
		    const char *name, const char *detail, int code,
		    const char *decision, int short_circuit);

/** Build an event, attach spans, then commit. */
void pd_trace_begin(pd_trace_layer_t layer, pd_trace_phase_t phase,
		    const char *name, const char *detail);
void pd_trace_span_argv(int index, const char *role);
void pd_trace_set_code(int code);
void pd_trace_set_decision(const char *decision);
void pd_trace_set_short_circuit(int short_circuit);
void pd_trace_commit(void);

/**
 * Serialize ring to a JSON array string (malloc). Caller frees.
 * Returns empty array "[]" when no events.
 */
char *pd_trace_to_json(void);

/**
 * Optional host Cfuns for sealed Janet pack env (trace-event / trace-span-argv).
 * No-op when env is NULL. Only linked when TRACE is defined.
 */
void pd_trace_register_janet(void *janet_env);

#define PD_TRACE_CLEAR() pd_trace_clear()
#define PD_TRACE_EVENT(layer, phase, name, detail, code, dec, sc)              \
	pd_trace_event((layer), (phase), (name), (detail), (code), (dec), (sc))
#define PD_TRACE_BEGIN(layer, phase, name, detail)                             \
	pd_trace_begin((layer), (phase), (name), (detail))
#define PD_TRACE_SPAN_ARGV(index, role) pd_trace_span_argv((index), (role))
#define PD_TRACE_SET_CODE(code) pd_trace_set_code(code)
#define PD_TRACE_SET_DECISION(dec) pd_trace_set_decision(dec)
#define PD_TRACE_SET_SC(sc) pd_trace_set_short_circuit(sc)
#define PD_TRACE_COMMIT() pd_trace_commit()
#define PD_TRACE_REGISTER_JANET(env) pd_trace_register_janet(env)

#else /* !GROKOS_POLICYD_TRACE */

static inline void pd_trace_clear(void)
{
}

static inline char *pd_trace_to_json(void)
{
	return NULL;
}

static inline void pd_trace_register_janet(void *janet_env)
{
	(void)janet_env;
}

#define PD_TRACE_CLEAR() ((void)0)
#define PD_TRACE_EVENT(layer, phase, name, detail, code, dec, sc)              \
	((void)0)
#define PD_TRACE_BEGIN(layer, phase, name, detail) ((void)0)
#define PD_TRACE_SPAN_ARGV(index, role) ((void)0)
#define PD_TRACE_SET_CODE(code) ((void)0)
#define PD_TRACE_SET_DECISION(dec) ((void)0)
#define PD_TRACE_SET_SC(sc) ((void)0)
#define PD_TRACE_COMMIT() ((void)0)
#define PD_TRACE_REGISTER_JANET(env) ((void)0)

#endif /* GROKOS_POLICYD_TRACE */

#ifdef __cplusplus
}
#endif

#endif /* POLICYD_POLICY_TRACE_H */
