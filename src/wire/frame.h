/* SPDX-License-Identifier: Apache-2.0 */
/**
 * GKPP framing for the policyd Cap'n peer (not seat GKSP).
 *
 * offset  size  field
 * 0       4     magic b"GKPP"
 * 4       1     version = 1
 * 5       1     flags
 * 6       2     reserved = 0
 * 8       4     body_len LE
 * 12      N     Cap'n Proto multi-segment message (PolicyEnvelope)
 */
#ifndef GROK_POLICYD_WIRE_FRAME_H
#define GROK_POLICYD_WIRE_FRAME_H

#include <stddef.h>
#include <stdint.h>

#define GKPP_MAGIC "GKPP"
#define GKPP_FRAME_VERSION 1
#define GKPP_HEADER_LEN 12
#define GKPP_MAX_BODY (64 * 1024)

/** Read one GKPP frame body into @a body (caller frees). Returns 0 or -errno. */
int gkpp_read_frame(int fd, uint8_t **body, size_t *body_len);

/** Write one GKPP frame with Cap'n body. Returns 0 or -errno. */
int gkpp_write_frame(int fd, const uint8_t *body, size_t body_len);

#endif
