/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_POLICY_JANET_H
#define GROK_POLICYD_POLICY_JANET_H

#include <capnp_c.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Shell content pack path.
 * Seals Cap'n ShellView, runs Janet pack, returns Cap'n PolicyDecision bytes
 * (malloc). Failures are still Cap'n PolicyDecision deny + PolicyReason code.
 * Callers free *out. Prefer passthrough of pack output as method result.
 */
void grok_policy_shell_pack(const char *workspace, const char *cwd,
			    capn_ptr argv, uint8_t **out, size_t *out_len);

#endif
