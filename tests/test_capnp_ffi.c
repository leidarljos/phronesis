/* SPDX-License-Identifier: Apache-2.0 */
/*
 * interface Policyd via CallEnvelope: typed CheckResults (ok|err).
 */
#include "harness.h"
#include "grok-policyd/supervisor.h"
#include "policy.capnp.h"
#include "util.capnp.h"

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

static AgentId_ptr mk_agent(struct capn_segment *seg, uint64_t hi, uint64_t lo)
{
	struct AgentId id = { .hi = hi, .lo = lo };
	AgentId_ptr p = new_AgentId(seg);

	write_AgentId(&id, p);
	return p;
}

static int encode_status(uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct CallEnvelope env;
	CallEnvelope_ptr ep;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.body_which = CallEnvelope_body_status;
	ep = new_CallEnvelope(capn_root(&c).seg);
	write_CallEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int encode_seat_check(uint64_t hi, uint64_t lo, enum SeatAction act,
			     uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct CallEnvelope env;
	struct CheckParams params;
	struct CheckBody body;
	CheckParams_ptr pp;
	CheckBody_ptr bp;
	CallEnvelope_ptr ep;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&body, 0, sizeof(body));
	body.which = CheckBody_seat;
	body.seat = act;
	bp = new_CheckBody(capn_root(&c).seg);
	write_CheckBody(&body, bp);
	memset(&params, 0, sizeof(params));
	params.agentId = mk_agent(capn_root(&c).seg, hi, lo);
	params.body = bp;
	pp = new_CheckParams(capn_root(&c).seg);
	write_CheckParams(&params, pp);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.body_which = CallEnvelope_body_check;
	env.body.check = pp;
	ep = new_CallEnvelope(capn_root(&c).seg);
	write_CallEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int encode_admit(uint64_t hi, uint64_t lo, enum AdmitKind kind,
			uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct CallEnvelope env;
	struct AdmitParams ap;
	AdmitParams_ptr app;
	CallEnvelope_ptr ep;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&ap, 0, sizeof(ap));
	ap.agentId = mk_agent(capn_root(&c).seg, hi, lo);
	ap.kind = kind;
	ap.detail = ctext("");
	app = new_AdmitParams(capn_root(&c).seg);
	write_AdmitParams(&ap, app);
	memset(&env, 0, sizeof(env));
	env.protocolVersion = 1;
	env.body_which = CallEnvelope_body_admit;
	env.body.admit = app;
	ep = new_CallEnvelope(capn_root(&c).seg);
	write_CallEnvelope(&env, ep);
	if (capn_setp(capn_root(&c), 0, ep.p) != 0 ||
	    capn_write_grow(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static void expect_check_ok(grok_supervisor_t *sup, const uint8_t *req,
			    size_t req_len, enum Decision want)
{
	uint8_t *resp = NULL;
	size_t resp_len = 0;
	struct capn c;
	CallEnvelope_ptr root;
	struct CallEnvelope env;
	struct CheckResults cr;
	struct PolicyDecision d;

	assert_int_equal(grok_policyd_handle_capnp(sup, req, req_len, &resp,
						   &resp_len),
			 0);
	assert_non_null(resp);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_CallEnvelope(&env, root);
	assert_true(env.body_which == CallEnvelope_body_checkResults ||
		    env.body_which == CallEnvelope_body_admitResults);
	if (env.body_which == CallEnvelope_body_checkResults)
		read_CheckResults(&cr, env.body.checkResults);
	else
		read_CheckResults(&cr, env.body.admitResults);
	assert_int_equal(cr.which, CheckResults_ok);
	read_PolicyDecision(&d, cr.ok);
	assert_int_equal(d.decision, want);
	capn_free(&c);
	free(resp);
}

static void test_status_request_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL, *resp = NULL;
	size_t req_len = 0, resp_len = 0;
	struct capn c;
	CallEnvelope_ptr root;
	struct CallEnvelope env;
	struct StatusResults sr;
	struct PolicydStatus st;

	assert_int_equal(encode_status(&req, &req_len), 0);
	assert_int_equal(grok_policyd_handle_capnp(f->sup, req, req_len, &resp,
						   &resp_len),
			 0);
	free(req);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, resp, resp_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_CallEnvelope(&env, root);
	assert_int_equal(env.body_which, CallEnvelope_body_statusResults);
	read_StatusResults(&sr, env.body.statusResults);
	assert_int_equal(sr.which, StatusResults_ok);
	read_PolicydStatus(&st, sr.ok);
	assert_int_equal(st.ready, 1);
	capn_free(&c);
	free(resp);
}

static void test_check_seat_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL;
	size_t req_len = 0;

	assert_int_equal(encode_seat_check(1, 2, SeatAction_publishRun, &req,
					   &req_len),
			 0);
	expect_check_ok(f->sup, req, req_len, Decision_allow);
	free(req);
}

static void test_admit_model_via_handle(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *req = NULL;
	size_t req_len = 0;

	assert_int_equal(encode_admit(1, 2, AdmitKind_model, &req, &req_len),
			 0);
	expect_check_ok(f->sup, req, req_len, Decision_allow);
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
	};
	return cmocka_run_group_tests_name("capnp_ffi", tests, NULL, NULL);
}
