/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Product Cap'n path: schema encode → grok_policyd_handle_capnp → schema
 * decode → assert decision or fail-closed error.
 */
#include "harness.h"
#include "grok-policyd/supervisor.h"

#include "policy.capnp.h"

#include <capnp_c.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

struct capn_fix {
	grok_supervisor_t *sup;
	char st[GROK_PATH_MAX];
	char rt[GROK_PATH_MAX];
};

static int capn_setup(void **state)
{
	struct capn_fix *f = calloc(1, sizeof(*f));

	assert_non_null(f);
	assert_int_equal(t_open_pair(&f->sup, f->st, sizeof(f->st), f->rt,
				     sizeof(f->rt), "capn"),
			 GROK_OK);
	assert_non_null(f->sup);
	*state = f;
	return 0;
}

static int capn_teardown(void **state)
{
	struct capn_fix *f = *state;

	if (f) {
		if (f->sup)
			grok_supervisor_close(f->sup);
		t_rm_rf(f->st);
		t_rm_rf(f->rt);
		free(f);
	}
	return 0;
}

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

/** Encode a status request with the given protocolVersion. */
static int encode_status(int protocol_version, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicyEnvelope env;
	struct PolicyRequest req;
	PolicyEnvelope_ptr ep;
	PolicyRequest_ptr rp;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&req, 0, sizeof(req));
	req.op_which = PolicyRequest_op_status;
	rp = new_PolicyRequest(capn_root(&c).seg);
	write_PolicyRequest(&req, rp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = protocol_version;
	env.traceId = ctext("test");
	env.body_which = PolicyEnvelope_body_request;
	env.body.request = rp;
	ep = new_PolicyEnvelope(capn_root(&c).seg);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int encode_check(const char *agent, const char *tool, const char *action,
			const char *path, uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicyEnvelope env;
	struct PolicyRequest preq;
	struct PolicyCheck ch;
	PolicyEnvelope_ptr ep;
	PolicyRequest_ptr rp;
	PolicyCheck_ptr cp;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&ch, 0, sizeof(ch));
	ch.agentId = ctext(agent);
	ch.tool = ctext(tool);
	ch.action = ctext(action);
	ch.path = ctext(path);
	cp = new_PolicyCheck(capn_root(&c).seg);
	write_PolicyCheck(&ch, cp);
	memset(&preq, 0, sizeof(preq));
	preq.op_which = PolicyRequest_op_check;
	preq.op.check = cp;
	rp = new_PolicyRequest(capn_root(&c).seg);
	write_PolicyRequest(&preq, rp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = ctext("");
	env.body_which = PolicyEnvelope_body_request;
	env.body.request = rp;
	ep = new_PolicyEnvelope(capn_root(&c).seg);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int encode_admit(const char *agent, const char *kind, const char *detail,
			uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicyEnvelope env;
	struct PolicyRequest preq;
	struct PolicyAdmit ad;
	PolicyEnvelope_ptr ep;
	PolicyRequest_ptr rp;
	PolicyAdmit_ptr ap;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&ad, 0, sizeof(ad));
	ad.agentId = ctext(agent);
	ad.kind = ctext(kind);
	ad.detail = ctext(detail);
	ap = new_PolicyAdmit(capn_root(&c).seg);
	write_PolicyAdmit(&ad, ap);
	memset(&preq, 0, sizeof(preq));
	preq.op_which = PolicyRequest_op_admit;
	preq.op.admit = ap;
	rp = new_PolicyRequest(capn_root(&c).seg);
	write_PolicyRequest(&preq, rp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.traceId = ctext("");
	env.body_which = PolicyEnvelope_body_request;
	env.body.request = rp;
	ep = new_PolicyEnvelope(capn_root(&c).seg);
	write_PolicyEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static void expect_error_code(grok_supervisor_t *sup, const uint8_t *req,
			      size_t req_len, int32_t want)
{
	uint8_t *resp = NULL;
	size_t resp_len = 0;
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	struct PolicyError err;

	assert_int_equal(grok_policyd_handle_capnp(sup, req, req_len, &resp,
						   &resp_len),
			 0);
	assert_non_null(resp);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	assert_int_equal(env.body_which, PolicyEnvelope_body_response);
	read_PolicyResponse(&pr, env.body.response);
	assert_int_equal(pr.ok_which, PolicyResponse_ok_error);
	read_PolicyError(&err, pr.ok.error);
	assert_int_equal(err.code, want);
	capn_free(&c);
	free(resp);
}

static void expect_decision(grok_supervisor_t *sup, const uint8_t *req,
			    size_t req_len, enum PolicyResponse_ok_which which,
			    int decision)
{
	uint8_t *resp = NULL;
	size_t resp_len = 0;
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	struct PolicyDecision d;
	PolicyDecision_ptr dp;

	assert_int_equal(grok_policyd_handle_capnp(sup, req, req_len, &resp,
						   &resp_len),
			 0);
	assert_non_null(resp);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyEnvelope(&env, root);
	assert_int_equal(env.body_which, PolicyEnvelope_body_response);
	read_PolicyResponse(&pr, env.body.response);
	assert_int_equal(pr.ok_which, which);
	assert_true(which == PolicyResponse_ok_check ||
		    which == PolicyResponse_ok_admit);
	dp = (which == PolicyResponse_ok_check) ? pr.ok.check : pr.ok.admit;
	read_PolicyDecision(&d, dp);
	assert_int_equal(d.decision, decision);
	capn_free(&c);
	free(resp);
}

static void test_status_request_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL, *resp = NULL;
	size_t req_len = 0, resp_len = 0;
	struct capn c;
	PolicyEnvelope_ptr root;
	struct PolicyEnvelope env;
	struct PolicyResponse pr;
	struct PolicydStatus ps;

	assert_int_equal(encode_status(1, &req, &req_len), 0);
	assert_true(req_len > 0 && req_len <= GROK_POLICY_CAPNP_MAX_BODY);

	assert_int_equal(grok_policyd_handle_capnp(f->sup, req, req_len, &resp,
						   &resp_len),
			 0);
	assert_non_null(resp);
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
}

static void test_check_seat_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL;
	size_t req_len = 0;

	assert_int_equal(encode_check("smoke-agent", "seat", "publish_run",
				      "run-1", &req, &req_len),
			 0);
	expect_decision(f->sup, req, req_len, PolicyResponse_ok_check,
			Decision_allow);
	free(req);
}

static void test_admit_model_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL;
	size_t req_len = 0;

	assert_int_equal(encode_admit("admit-agent", "model", "start", &req,
				      &req_len),
			 0);
	expect_decision(f->sup, req, req_len, PolicyResponse_ok_admit,
			Decision_allow);
	free(req);
}

static void test_handle_rejects_garbage_body(void **state)
{
	struct capn_fix *f = *state;
	uint8_t junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	expect_error_code(f->sup, junk, sizeof(junk), GROK_ERR_INVAL);
	expect_error_code(f->sup, junk, 0, GROK_ERR_INVAL);
}

static void test_handle_rejects_bad_protocol(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL;
	size_t req_len = 0;

	assert_int_equal(encode_status(99, &req, &req_len), 0);
	expect_error_code(f->sup, req, req_len, GROK_ERR_INVAL);
	free(req);
}

int run_capnp_ffi_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_status_request_via_handle,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_via_handle,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_admit_model_via_handle,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_handle_rejects_garbage_body,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_handle_rejects_bad_protocol,
						capn_setup, capn_teardown),
	};
	return cmocka_run_group_tests_name("capnp_ffi", tests, NULL, NULL);
}
