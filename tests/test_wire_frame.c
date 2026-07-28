/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Cap'n body round-trip via c-capnproto + nng loopback (no DTO layer).
 */
#include "harness.h"
#include "wire/serve.h"

#include "policy.capnp.h"

#include <capnp_c.h>
#include <errno.h>
#include <limits.h>
#include <nng/nng.h>
#include <sys/stat.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cmocka.h>

#include "internal.h"

static capn_text ctext(const char *s)
{
	capn_text t;
	size_t len = s ? strlen(s) : 0;

	if (len > (size_t)INT_MAX)
		len = (size_t)INT_MAX;
	t.len = (int)len;
	t.str = s ? s : "";
	t.seg = NULL;
	return t;
}

static int capn_write_grow(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf = NULL;
	size_t cap = 4096U;
	int64_t n;

	*out = NULL;
	*out_len = 0;
	for (;;) {
		if (cap > 1024U * 1024U)
			return -1;
		buf = malloc(cap);
		if (!buf)
			return -1;
		n = capn_write_mem(c, buf, cap, 0);
		if (n >= 0)
			break;
		free(buf);
		cap *= 2U;
	}
	*out = buf;
	*out_len = (size_t)n;
	return 0;
}

/** Build a Cap'n status request body with c-capnproto. */
static int encode_status_request(uint8_t **out, size_t *out_len)
{
	struct capn c;
	capn_ptr cr;
	struct capn_segment *cs;
	struct PolicyEnvelope env;
	struct PolicyRequest req;
	PolicyEnvelope_ptr ep;
	PolicyRequest_ptr rp;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	cr = capn_root(&c);
	cs = cr.seg;

	memset(&req, 0, sizeof(req));
	req.op_which = PolicyRequest_op_status;
	rp = new_PolicyRequest(cs);
	write_PolicyRequest(&req, rp);

	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = ctext("test");
	env.body_which = PolicyEnvelope_body_request;
	env.body.request = rp;
	ep = new_PolicyEnvelope(cs);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0) {
		capn_free(&c);
		return -1;
	}
	if (capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static void test_nng_body_loopback(void **state)
{
	char path[256];
	char url[320];
	const char *payload = "hello-capnp-body-pad";
	nng_socket rep;
	nng_socket req;
	nng_msg *msg = NULL;
	nng_msg *rmsg = NULL;
	nng_msg *echo = NULL;
	nng_pipe p;
	uint64_t uid = UINT64_MAX;
	int rc;
	pid_t pid = getpid();

	(void)state;
	memset(&rep, 0, sizeof(rep));
	memset(&req, 0, sizeof(req));

	snprintf(path, sizeof(path), "/var/tmp/policyd-nng-loop-%d.sock", (int)pid);
	assert_true(snprintf(url, sizeof(url), "ipc://%s", path) < (int)sizeof(url));
	(void)unlink(path);

	rc = nng_rep0_open(&rep);
	assert_int_equal(rc, 0);
	rc = nng_listen(rep, url, NULL, 0);
	assert_int_equal(rc, 0);
	rc = nng_req0_open(&req);
	assert_int_equal(rc, 0);
	rc = nng_socket_set_ms(req, NNG_OPT_RECVTIMEO, 3000);
	assert_int_equal(rc, 0);
	rc = nng_socket_set_ms(rep, NNG_OPT_RECVTIMEO, 3000);
	assert_int_equal(rc, 0);
	rc = nng_dial(req, url, NULL, 0);
	assert_int_equal(rc, 0);

	rc = nng_msg_alloc(&msg, 0);
	assert_int_equal(rc, 0);
	rc = nng_msg_append(msg, payload, strlen(payload));
	assert_int_equal(rc, 0);
	rc = nng_sendmsg(req, msg, 0);
	assert_int_equal(rc, 0);

	rc = nng_recvmsg(rep, &rmsg, 0);
	assert_int_equal(rc, 0);
	assert_int_equal(nng_msg_len(rmsg), strlen(payload));
	assert_memory_equal(nng_msg_body(rmsg), payload, strlen(payload));

	p = nng_msg_get_pipe(rmsg);
	rc = nng_pipe_get_uint64(p, NNG_OPT_PEER_UID, &uid);
	assert_int_equal(rc, 0);
	assert_int_equal(uid, (uint64_t)getuid());

	rc = nng_msg_alloc(&echo, 0);
	assert_int_equal(rc, 0);
	rc = nng_msg_append(echo, payload, strlen(payload));
	assert_int_equal(rc, 0);
	nng_msg_free(rmsg);
	rc = nng_sendmsg(rep, echo, 0);
	assert_int_equal(rc, 0);
	rc = nng_recvmsg(req, &rmsg, 0);
	assert_int_equal(rc, 0);
	assert_int_equal(nng_msg_len(rmsg), strlen(payload));
	nng_msg_free(rmsg);

	nng_close(req);
	nng_close(rep);
	(void)unlink(path);
}

static void test_status_request_via_handle(void **state)
{
	char base[256];
	char st[300];
	char rt[300];
	grok_supervisor_t *sup = NULL;
	uint8_t *req = NULL;
	uint8_t *resp = NULL;
	size_t req_len = 0, resp_len = 0;
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	struct PolicydStatus ps;

	(void)state;
	snprintf(base, sizeof(base), "/tmp/policyd-capn-status-%d", (int)getpid());
	snprintf(st, sizeof(st), "%s/state", base);
	snprintf(rt, sizeof(rt), "%s/run", base);
	assert_int_equal(mkdir(base, 0700), 0);
	assert_int_equal(mkdir(st, 0700), 0);
	assert_int_equal(mkdir(rt, 0700), 0);
	assert_int_equal(grok_supervisor_open(&sup, st, rt), GROK_OK);
	assert_non_null(sup);

	assert_int_equal(encode_status_request(&req, &req_len), 0);
	assert_true(req_len > 0);
	assert_true(req_len <= GROK_NNG_MAX_BODY);

	assert_int_equal(
		grok_policyd_handle_capnp(sup, "/tmp/fake.sock", req, req_len,
					  &resp, &resp_len),
		0);
	assert_non_null(resp);
	assert_true(resp_len > 0);

	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	assert_int_equal(env.protocolVersion, 1);
	assert_int_equal(env.body_which, PolicyEnvelope_body_response);
	read_PolicyResponse(&pr, env.body.response);
	assert_int_equal(pr.ok_which, PolicyResponse_ok_status);
	read_PolicydStatus(&ps, pr.ok.status);
	assert_int_equal(ps.ready, 1);
	assert_true(ps.version.len > 0);
	capn_free(&c);

	free(req);
	free(resp);
	grok_supervisor_close(sup);
	/* best-effort cleanup */
	(void)rmdir(rt);
	(void)rmdir(st);
	(void)rmdir(base);
}

static void test_check_seat_via_handle(void **state)
{
	char base[256];
	char st[300];
	char rt[300];
	grok_supervisor_t *sup = NULL;
	uint8_t *reqb = NULL;
	uint8_t *resp = NULL;
	size_t req_len = 0, resp_len = 0;
	struct capn c;
	capn_ptr cr;
	struct capn_segment *cs;
	struct PolicyEnvelope env;
	struct PolicyRequest preq;
	struct PolicyCheck ch;
	PolicyEnvelope_ptr ep;
	PolicyRequest_ptr rp;
	PolicyCheck_ptr cp;
	PolicyEnvelope_ptr root;
	struct PolicyResponse pr;
	struct PolicyDecision d;

	(void)state;
	snprintf(base, sizeof(base), "/tmp/policyd-capn-check-%d", (int)getpid());
	snprintf(st, sizeof(st), "%s/state", base);
	snprintf(rt, sizeof(rt), "%s/run", base);
	assert_int_equal(mkdir(base, 0700), 0);
	assert_int_equal(mkdir(st, 0700), 0);
	assert_int_equal(mkdir(rt, 0700), 0);
	assert_int_equal(grok_supervisor_open(&sup, st, rt), GROK_OK);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	cr = capn_root(&c);
	cs = cr.seg;
	memset(&ch, 0, sizeof(ch));
	ch.agentId = ctext("smoke-agent");
	ch.tool = ctext("seat");
	ch.action = ctext("publish_run");
	ch.path = ctext("run-1");
	cp = new_PolicyCheck(cs);
	write_PolicyCheck(&ch, cp);
	memset(&preq, 0, sizeof(preq));
	preq.op_which = PolicyRequest_op_check;
	preq.op.check = cp;
	rp = new_PolicyRequest(cs);
	write_PolicyRequest(&preq, rp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = ctext("");
	env.body_which = PolicyEnvelope_body_request;
	env.body.request = rp;
	ep = new_PolicyEnvelope(cs);
	write_PolicyEnvelope(&env, ep);
	assert_int_equal(capn_setp(capn_root(&c), 0, ep.p), 0);
	assert_int_equal(capn_write_grow(&c, &reqb, &req_len), 0);
	capn_free(&c);

	assert_int_equal(
		grok_policyd_handle_capnp(sup, "/tmp/fake.sock", reqb, req_len,
					  &resp, &resp_len),
		0);

	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	read_PolicyResponse(&pr, env.body.response);
	assert_int_equal(pr.ok_which, PolicyResponse_ok_check);
	read_PolicyDecision(&d, pr.ok.check);
	assert_int_equal(d.decision, Decision_allow);
	capn_free(&c);

	free(reqb);
	free(resp);
	grok_supervisor_close(sup);
	(void)rmdir(rt);
	(void)rmdir(st);
	(void)rmdir(base);
}

int run_wire_frame_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_nng_body_loopback),
		cmocka_unit_test(test_status_request_via_handle),
		cmocka_unit_test(test_check_seat_via_handle),
	};
	return cmocka_run_group_tests_name("wire_frame", tests, NULL, NULL);
}
