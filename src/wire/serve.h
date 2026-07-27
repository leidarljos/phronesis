/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_WIRE_SERVE_H
#define GROK_POLICYD_WIRE_SERVE_H

#include "grok-policyd/supervisor.h"

/**
 * Bind @a socket_path (0600), accept peers (SO_PEERCRED same-uid), dispatch
 * Cap'n PolicyEnvelope ops against @a sup until fatal error or signal.
 * Returns process exit code.
 */
int grok_policyd_serve(grok_supervisor_t *sup, const char *socket_path);

#endif
