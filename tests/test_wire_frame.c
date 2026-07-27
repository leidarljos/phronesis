/* SPDX-License-Identifier: Apache-2.0 */
#include "wire/capnp_min.h"
#include "wire/frame.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void test_encode_decode_status_response(void)
{
	struct wire_response resp;
	struct wire_request req;
	uint8_t *body = NULL;
	size_t len = 0;
	/* Build a minimal status *request* using encode path: we only have
	 * encode_response; for request, craft via response error path reverse —
	 * instead test response encode + that decode fails cleanly on response.
	 */
	memset(&resp, 0, sizeof(resp));
	resp.kind = WIRE_RESP_STATUS;
	snprintf(resp.u.status.version, sizeof(resp.u.status.version), "0.1.0");
	resp.u.status.api_version = 1;
	snprintf(resp.u.status.state_dir, sizeof(resp.u.status.state_dir), "/tmp/s");
	snprintf(resp.u.status.runtime_dir, sizeof(resp.u.status.runtime_dir),
		 "/tmp/r");
	snprintf(resp.u.status.socket, sizeof(resp.u.status.socket),
		 "/tmp/p.sock");
	resp.u.status.ready = 1;
	assert(wire_encode_response(&resp, &body, &len) == 0);
	assert(body && len > 0);
	/* decode_request should reject response bodies */
	memset(&req, 0, sizeof(req));
	assert(wire_decode_request(body, len, &req) != 0);
	free(body);
	printf("ok: encode status response\n");
}

static void test_gkpp_loopback(void)
{
	int sv[2];
	const char *payload = "hello-capnp-body-pad";
	uint8_t *body = NULL;
	size_t blen = 0;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	assert(gkpp_write_frame(sv[0], (const uint8_t *)payload, strlen(payload)) ==
	       0);
	assert(gkpp_read_frame(sv[1], &body, &blen) == 0);
	assert(blen == strlen(payload));
	assert(memcmp(body, payload, blen) == 0);
	free(body);
	close(sv[0]);
	close(sv[1]);
	printf("ok: gkpp frame loopback\n");
}

/* Encode a status request by hand-building via response encoder's inverse:
 * use a tiny local builder — wire_decode needs a request. Build request
 * bytes using arena by reusing encode of a fake path: call encode after
 * temporarily exposing — simplest: construct request Cap'n with known layout
 * via wire_encode of ERROR is wrong. Implement inline request encode for test.
 */
#include <stdlib.h>

/* Duplicate minimal request encode for status void op */
static int encode_status_request(uint8_t **out, size_t *out_len)
{
	struct wire_response dummy;
	/* We need request encoding. Use serve-side pattern inverted.
	 * Build segment manually matching capnp-min encode layout for request.
	 */
	/* Use wire_encode_response is wrong. Craft with Python? Manual: */
	extern int wire_encode_response(const struct wire_response *, uint8_t **,
					size_t *);
	/* Build request by encoding response then patching tag — fragile.
	 * Instead ship a second encoder for status request only in test. */
	uint8_t *seg;
	size_t words = 6;
	uint32_t table[2];
	size_t total;
	uint64_t *w;

	seg = calloc(words, 8);
	if (!seg)
		return -1;
	w = (uint64_t *)seg;
	/* root struct ptr → word 1, data_words=1 ptrs=2 */
	w[0] = 0 | ((uint64_t)0 << 2) | (1ull << 32) | (2ull << 48);
	/* offset 0 means data at word 1 */
	w[1] = 1ull | (0ull << 32); /* protocolVersion=1, body tag=request(0) */
	w[2] = 0;		    /* empty trace text */
	/* ptr[1] → PolicyRequest at word 4: data=1 ptr=1 */
	/* pointer at word 3: offset to word 4 = 0 */
	w[3] = 0 | ((uint64_t)0 << 2) | (1ull << 32) | (1ull << 48);
	w[4] = 0; /* op tag = status */
	w[5] = 0; /* unused ptr */

	table[0] = 0;
	table[1] = (uint32_t)words;
	total = 8 + words * 8;
	*out = malloc(total);
	if (!*out) {
		free(seg);
		return -1;
	}
	memcpy(*out, table, 8);
	memcpy(*out + 8, seg, words * 8);
	free(seg);
	*out_len = total;
	(void)dummy;
	return 0;
}

static void test_decode_status_request(void)
{
	uint8_t *body = NULL;
	size_t len = 0;
	struct wire_request req;

	assert(encode_status_request(&body, &len) == 0);
	memset(&req, 0, sizeof(req));
	assert(wire_decode_request(body, len, &req) == 0);
	assert(req.op == WIRE_OP_STATUS);
	free(body);
	printf("ok: decode status request\n");
}

int main(void)
{
	test_gkpp_loopback();
	test_encode_decode_status_response();
	test_decode_status_request();
	printf("ok: wire unit tests\n");
	return 0;
}
