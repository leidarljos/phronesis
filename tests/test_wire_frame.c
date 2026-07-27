/* SPDX-License-Identifier: Apache-2.0 */
#include "wire/capnp_min.h"
#include "wire/frame.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

static void ensure_fixture_dir(char *out, size_t n)
{
	const char *root = getenv("POLICYD_FIXTURE_DIR");
	if (root && root[0]) {
		snprintf(out, n, "%s", root);
	} else {
		/* tests/ run from package root via make */
		snprintf(out, n, "tests/fixtures/capnp");
	}
	if (mkdir(out, 0755) != 0 && errno != EEXIST) {
		/* try creating parents one level */
		char parent[512];
		snprintf(parent, sizeof(parent), "tests/fixtures");
		(void)mkdir("tests", 0755);
		(void)mkdir(parent, 0755);
		if (mkdir(out, 0755) != 0 && errno != EEXIST) {
			perror("mkdir fixtures");
			/* non-fatal for unit path */
		}
	}
}

static void write_fixture(const char *dir, const char *name, const uint8_t *body,
			  size_t len)
{
	char path[768];
	FILE *f;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	f = fopen(path, "wb");
	if (!f) {
		perror(path);
		return;
	}
	assert(fwrite(body, 1, len, f) == len);
	fclose(f);
	printf("ok: wrote fixture %s (%zu bytes)\n", path, len);
}

static void test_encode_decode_status_response(void)
{
	struct wire_response resp;
	struct wire_request req;
	uint8_t *body = NULL;
	size_t len = 0;
	char fixdir[512];

	memset(&resp, 0, sizeof(resp));
	resp.kind = WIRE_RESP_STATUS;
	snprintf(resp.u.status.version, sizeof(resp.u.status.version), "0.1.0");
	resp.u.status.api_version = 1;
	snprintf(resp.u.status.state_dir, sizeof(resp.u.status.state_dir),
		 "/fixture/state");
	snprintf(resp.u.status.runtime_dir, sizeof(resp.u.status.runtime_dir),
		 "/fixture/runtime");
	snprintf(resp.u.status.socket, sizeof(resp.u.status.socket),
		 "/fixture/policyd.sock");
	resp.u.status.ready = 1;
	assert(wire_encode_response(&resp, &body, &len) == 0);
	assert(body && len > 0);
	/* decode_request should reject response bodies */
	memset(&req, 0, sizeof(req));
	assert(wire_decode_request(body, len, &req) != 0);

	ensure_fixture_dir(fixdir, sizeof(fixdir));
	write_fixture(fixdir, "status_response.bin", body, len);

	/* also a hex sidecar for review */
	{
		char path[768];
		FILE *f;
		size_t i;
		snprintf(path, sizeof(path), "%s/status_response.hex", fixdir);
		f = fopen(path, "w");
		if (f) {
			for (i = 0; i < len; i++)
				fprintf(f, "%02x%s", body[i],
					((i + 1) % 16 == 0) ? "\n" : " ");
			if (len % 16)
				fputc('\n', f);
			fclose(f);
		}
	}
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

static void test_gkpp_rejects_flags(void)
{
	int sv[2];
	uint8_t hdr[12];
	uint8_t *body = NULL;
	size_t blen = 0;
	const char *payload = "x";

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	memcpy(hdr, "GKPP", 4);
	hdr[4] = 1;
	hdr[5] = 1; /* flags nonzero */
	hdr[6] = 0;
	hdr[7] = 0;
	hdr[8] = 1;
	hdr[9] = 0;
	hdr[10] = 0;
	hdr[11] = 0;
	assert(write(sv[0], hdr, 12) == 12);
	assert(write(sv[0], payload, 1) == 1);
	assert(gkpp_read_frame(sv[1], &body, &blen) != 0);
	close(sv[0]);
	close(sv[1]);
	printf("ok: gkpp rejects nonzero flags\n");
}

/* status request: body Cap'n stream matching capnp_min decode layout */
static int encode_status_request(uint8_t **out, size_t *out_len)
{
	uint8_t *seg;
	size_t words = 6;
	uint32_t table[2];
	size_t total;
	uint64_t *w;

	seg = calloc(words, 8);
	if (!seg)
		return -1;
	w = (uint64_t *)seg;
	w[0] = 0 | ((uint64_t)0 << 2) | (1ull << 32) | (2ull << 48);
	w[1] = 1ull | (0ull << 32);
	w[2] = 0;
	w[3] = 0 | ((uint64_t)0 << 2) | (1ull << 32) | (1ull << 48);
	w[4] = 0;
	w[5] = 0;

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
	return 0;
}

static void test_decode_status_request(void)
{
	uint8_t *body = NULL;
	size_t len = 0;
	struct wire_request req;
	char fixdir[512];

	assert(encode_status_request(&body, &len) == 0);
	memset(&req, 0, sizeof(req));
	assert(wire_decode_request(body, len, &req) == 0);
	assert(req.op == WIRE_OP_STATUS);
	ensure_fixture_dir(fixdir, sizeof(fixdir));
	write_fixture(fixdir, "status_request.bin", body, len);
	free(body);
	printf("ok: decode status request\n");
}

static void test_encode_check_decision(void)
{
	struct wire_response resp;
	uint8_t *body = NULL;
	size_t len = 0;
	char fixdir[512];

	memset(&resp, 0, sizeof(resp));
	resp.kind = WIRE_RESP_CHECK;
	resp.u.decision.decision = WIRE_DEC_ALLOW;
	snprintf(resp.u.decision.reason, sizeof(resp.u.decision.reason),
		 "seat Cap'n visibility");
	snprintf(resp.u.decision.agent_id, sizeof(resp.u.decision.agent_id),
		 "smoke-agent");
	snprintf(resp.u.decision.tool, sizeof(resp.u.decision.tool), "seat");
	snprintf(resp.u.decision.action, sizeof(resp.u.decision.action),
		 "publish_run");
	assert(wire_encode_response(&resp, &body, &len) == 0);
	ensure_fixture_dir(fixdir, sizeof(fixdir));
	write_fixture(fixdir, "check_allow_response.bin", body, len);
	free(body);
	printf("ok: encode check allow response\n");
}

int main(void)
{
	test_gkpp_loopback();
	test_gkpp_rejects_flags();
	test_encode_decode_status_response();
	test_decode_status_request();
	test_encode_check_decision();
	printf("ok: wire unit tests\n");
	return 0;
}
