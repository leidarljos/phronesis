/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Playground KEEPALIVE C API for Emscripten MODULARIZE glue.
 * Name is historical (embind); pure C + EMSCRIPTEN_KEEPALIVE is enough.
 */
#include <emscripten.h>
#include <grok-policyd/supervisor.h>

#include "policy.capnp.h"
#include "policy_trace.h"

#include <capnp_c.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GROKOS_POLICYD_PLAYGROUND
#define GROKOS_POLICYD_PLAYGROUND 1
#endif

/*
 * Default multi-pack spec under MEMFS (seeded by memfs_seed.sh).
 * Colon list: product entry + packs.d directory (sorted top-level *.janet).
 * Same shape as GROKOS_POLICYD_JANET_PACK / reloadShellPack on the host.
 */
#define PD_DEFAULT_PACK_SPEC "/policy/shell.janet:/policy/packs.d"

EMSCRIPTEN_KEEPALIVE
policyd_supervisor_t *pd_supervisor_open(const char *state, const char *runtime)
{
	policyd_supervisor_t *s = NULL;
	const char *st = state && state[0] ? state : "/pd-state";
	const char *rt = runtime && runtime[0] ? runtime : "/pd-runtime";

	/* Prefer multi-pack colon list under MEMFS; do not override if already set. */
	setenv("GROKOS_POLICYD_JANET_PACK", PD_DEFAULT_PACK_SPEC, 0);
	if (policyd_supervisor_open(&s, st, rt) != POLICYD_OK)
		return NULL;
	return s;
}

EMSCRIPTEN_KEEPALIVE
void pd_supervisor_close(policyd_supervisor_t *s)
{
	policyd_supervisor_close(s);
}

/*
 * Run checkShell. On success writes malloc'd Cap'n PolicyDecision into
 * *out_holder and returns length (>= 0). On failure returns -1 and *out_holder
 * is NULL (or whatever grok_policyd left).
 */
EMSCRIPTEN_KEEPALIVE
int pd_check_shell(policyd_supervisor_t *s, const uint8_t *in, size_t in_len,
		   uint8_t **out_holder)
{
	uint8_t *out = NULL;
	size_t out_len = 0;

	if (!s || !in || !in_len || !out_holder)
		return -1;
	*out_holder = NULL;
	/* Fresh ring per check so take_trace_json matches this call. */
	pd_trace_clear();
	policyd_check_shell(s, in, in_len, &out, &out_len);
	if (!out || out_len == 0 || out_len > (size_t)INT32_MAX) {
		free(out);
		return -1;
	}
	*out_holder = out;
	return (int)out_len;
}

/*
 * Same Cap'n in / malloc'd PolicyDecision out shape as pd_check_shell for
 * path / seat / risk (fixture parity + playground).
 */
static int pd_check_common(policyd_supervisor_t *s, const uint8_t *in,
			   size_t in_len, uint8_t **out_holder,
			   void (*fn)(policyd_supervisor_t *, const uint8_t *,
				      size_t, uint8_t **, size_t *))
{
	uint8_t *out = NULL;
	size_t out_len = 0;

	if (!s || !in || !in_len || !out_holder || !fn)
		return -1;
	*out_holder = NULL;
	pd_trace_clear();
	fn(s, in, in_len, &out, &out_len);
	if (!out || out_len == 0 || out_len > (size_t)INT32_MAX) {
		free(out);
		return -1;
	}
	*out_holder = out;
	return (int)out_len;
}

EMSCRIPTEN_KEEPALIVE
int pd_check_path(policyd_supervisor_t *s, const uint8_t *in, size_t in_len,
		  uint8_t **out_holder)
{
	return pd_check_common(s, in, in_len, out_holder, policyd_check_path);
}

EMSCRIPTEN_KEEPALIVE
int pd_check_seat(policyd_supervisor_t *s, const uint8_t *in, size_t in_len,
		  uint8_t **out_holder)
{
	return pd_check_common(s, in, in_len, out_holder, policyd_check_seat);
}

EMSCRIPTEN_KEEPALIVE
int pd_check_risk(policyd_supervisor_t *s, const uint8_t *in, size_t in_len,
		  uint8_t **out_holder)
{
	return pd_check_common(s, in, in_len, out_holder, policyd_check_risk);
}

/*
 * Cap'n ReloadShellPack in → malloc'd PolicyDecision out (same shape as checks).
 * Author-mode path: write MEMFS pack via Module.FS, then call this.
 */
EMSCRIPTEN_KEEPALIVE
int pd_reload_shell_pack(policyd_supervisor_t *s, const uint8_t *in, size_t in_len,
			 uint8_t **out_holder)
{
	return pd_check_common(s, in, in_len, out_holder,
			       policyd_reload_shell_pack);
}

/*
 * Convenience: reload multi-pack colon list or absolute path (no Cap'n).
 * Path is the same shape as GROKOS_POLICYD_JANET_PACK / Cap'n reloadShellPack:
 * colon-separated absolute .janet files and/or directories of top-level packs.
 * Returns 0 reloaded, -1 path invalid, -2 load failed.
 */
EMSCRIPTEN_KEEPALIVE
int pd_reload_pack_path(const char *path)
{
	int rc;

	if (!path || !path[0])
		return -1;
	/* Public ABI: POLICYD_OK / POLICYD_ERR_INVAL / POLICYD_ERR_IO. Map to 0/-1/-2 for JS. */
	rc = policyd_policy_shell_pack_reload(path);
	if (rc == POLICYD_OK)
		return 0;
	if (rc == POLICYD_ERR_INVAL)
		return -1;
	return -2;
}

EMSCRIPTEN_KEEPALIVE
void pd_free(void *p)
{
	free(p);
}

/*
 * Decode Cap'n PolicyDecision root: decision (0/1/2) and PolicyReason code.
 * Returns 0 on success, -1 on parse failure.
 */
EMSCRIPTEN_KEEPALIVE
int pd_read_decision(const uint8_t *buf, size_t len, int *decision_out,
		     int *code_out)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr root;

	if (!buf || !len || !decision_out || !code_out)
		return -1;
	memset(&c, 0, sizeof(c));
	if (capn_init_mem(&c, buf, len, 0) != 0)
		return -1;
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	*decision_out = (int)d.decision;
	*code_out = (int)d.code;
	capn_free(&c);
	return 0;
}

/* Clear the TRACE ring (no-op when built without GROKOS_POLICYD_TRACE). */
EMSCRIPTEN_KEEPALIVE
void pd_clear_trace(void)
{
	pd_trace_clear();
}

/*
 * Malloc JSON array of TraceEvents and clear the ring. JS frees with pd_free.
 * Without TRACE, returns NULL.
 */
EMSCRIPTEN_KEEPALIVE
char *pd_take_trace_json(void)
{
	char *json = pd_trace_to_json();

	pd_trace_clear();
	return json;
}
