/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_SERVE_H
#define GROK_POLICYD_SERVE_H

#include "grok-policyd/supervisor.h"

#include <stddef.h>
#include <stdint.h>

/** Max Cap'n body size accepted on the nng peer (bytes). */
#define GROK_NNG_MAX_BODY (64 * 1024)

/**
 * Bind @a socket_path (0600), accept peers (NNG_OPT_PEER_UID same-uid),
 * dispatch Cap'n PolicyEnvelope ops against @a sup until stop.
 * Returns process exit code.
 */
int grok_policyd_serve(grok_supervisor_t *sup, const char *socket_path);

/**
 * Map admit.kind to policy_check tool/action. Fail-closed on unknown kind.
 * @return 0 and sets *@a tool / *@a action, or -1 if kind is unknown.
 */
int grok_policyd_map_admit_kind(const char *kind, const char **tool,
				const char **action);

/**
 * Handle one Cap'n request body; allocate Cap'n response body in *@a out
 * (caller frees). Used by the serve loop and cmocka round-trip tests.
 * @return 0 on success, -1 on encode failure (caller still may free *out).
 */
int grok_policyd_handle_capnp(grok_supervisor_t *sup, const char *socket_path,
			      const uint8_t *in, size_t in_len, uint8_t **out,
			      size_t *out_len);

#endif
