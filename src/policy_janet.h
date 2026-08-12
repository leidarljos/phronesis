/* SPDX-License-Identifier: Apache-2.0 */
#ifndef PHRONESIS_POLICY_JANET_H
#define PHRONESIS_POLICY_JANET_H

#include <capnp_c.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Shell content pack path.
 * Seals Cap'n ShellView, runs Janet pack, returns Cap'n PolicyDecision bytes
 * (malloc). Failures are still Cap'n PolicyDecision deny + PolicyReason code.
 * Callers free *out. Prefer passthrough of pack output as method result.
 */
void phronesis_policy_shell_pack(const char *workspace, const char *cwd,
			    capn_ptr argv, uint8_t **out, size_t *out_len);

/**
 * Audio / voice gate pack path (meta #97 Track E).
 * Passes Cap'n AudioCheck message bytes to Janet @c audio-check; returns
 * Cap'n PolicyDecision bytes (malloc). Failures are deny + pack PolicyReason.
 * Callers free *out. Product table lives in the pack (voice-law); host may
 * re-stamp agentId and apply DENY_ALL / AUDIO_ALLOW before calling.
 */
void phronesis_policy_audio_pack(const uint8_t *in, size_t in_len, uint8_t **out,
			    size_t *out_len);

/**
 * Unload any loaded packs and load @a path (colon-separated absolute
 * .janet files and/or directories of top-level *.janet packs). Each
 * file is opened under the pack root (install policy directory or
 * PHRONESIS_PACK_ROOT; not "/").
 * Returns 0 on success, -1 path invalid, -2 load failed.
 */
int phronesis_policy_shell_pack_reload_internal(const char *path);

#endif
