/* SPDX-License-Identifier: MIT */
/*
 * Policyd Cap'n methods: params root in, PolicyDecision root out (no unions).
 */
#include "harness.h"
#include "phronesis/supervisor.h"
#include "policy.capnp.h"
#include "util.capnp.h"

#include <capnp_c.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cmocka.h>

struct capn_fix {
	phronesis_supervisor_t *sup;
	char st[PHRONESIS_PATH_MAX];
	char rt[PHRONESIS_PATH_MAX];
};

static int capn_setup(void **state)
{
	struct capn_fix *f = calloc(1, sizeof(*f));
	char pack[PHRONESIS_PATH_MAX];
	const char *src;

	assert_non_null(f);
	assert_int_equal(t_open_pair(&f->sup, f->st, sizeof(f->st), f->rt,
				     sizeof(f->rt), "capn"),
			 PHRONESIS_OK);
	/* Product pack: shell-check + audio-check (absolute; reload after shell suite). */
	src = getenv("PHRONESIS_SOURCE_ROOT");
	if (!src || !src[0])
		src = ".";
	snprintf(pack, sizeof(pack), "%s/policy/shell.janet", src);
	setenv("PHRONESIS_JANET_PACK", pack, 1);
	assert_int_equal(phronesis_shell_pack_reload(pack), PHRONESIS_OK);
	*state = f;
	return 0;
}

static int capn_teardown(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("PHRONESIS_JANET_PACK");
	if (f) {
		if (f->sup)
			phronesis_supervisor_close(f->sup);
		t_rm_rf(f->st);
		t_rm_rf(f->rt);
		free(f);
	}
	return 0;
}

static int write_msg(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf;
	size_t cap = 4096;
	int64_t n;

	*out = NULL;
	*out_len = 0;
	for (;;) {
		buf = malloc(cap);
		assert_non_null(buf);
		n = capn_write_mem(c, buf, cap, 0);
		if (n >= 0) {
			*out = buf;
			*out_len = (size_t)n;
			return 0;
		}
		free(buf);
		cap *= 2;
		if (cap > 1024U * 1024U)
			return -1;
	}
}

static void hex_agent(uint64_t hi, uint64_t lo, char out[PHRONESIS_ID_MAX])
{
	assert_int_not_equal(snprintf(out, PHRONESIS_ID_MAX, "%016llx%016llx",
				      (unsigned long long)hi,
				      (unsigned long long)lo),
			     0);
}

static void wait_stopped(phronesis_supervisor_t *s, const char *id)
{
	phronesis_agent_status_t stt;
	int i;

	for (i = 0; i < 50; i++) {
		phronesis_supervisor_status(s, id, &stt);
		if (stt.state != PHRONESIS_AGENT_RUNNING)
			return;
		usleep(10 * 1000);
	}
}

static void start_bits(phronesis_supervisor_t *sup, uint64_t hi, uint64_t lo,
		       char *const argv[])
{
	char hex[PHRONESIS_ID_MAX];

	hex_agent(hi, lo, hex);
	assert_int_equal(phronesis_supervisor_start(sup, hex, "develop", "/ws",
						    argv),
			 PHRONESIS_OK);
}

static void stop_bits(phronesis_supervisor_t *sup, uint64_t hi, uint64_t lo)
{
	char hex[PHRONESIS_ID_MAX];

	hex_agent(hi, lo, hex);
	(void)phronesis_supervisor_stop(sup, hex);
	wait_stopped(sup, hex);
}

static AgentId_ptr mk_agent(struct capn_segment *seg, uint64_t hi, uint64_t lo)
{
	struct AgentId id = { .hi = hi, .lo = lo };
	AgentId_ptr p = new_AgentId(seg);

	write_AgentId(&id, p);
	return p;
}

static void expect_decision(const uint8_t *msg, size_t len, enum Decision want)
{
	struct capn c;
	PolicyDecision_ptr root;
	struct PolicyDecision d;

	assert_non_null(msg);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, msg, len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	assert_int_equal(d.decision, want);
	capn_free(&c);
}

static void test_status(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *out = NULL;
	size_t out_len = 0;
	struct capn c;
	PolicydStatus_ptr root;
	struct PolicydStatus st;

	phronesis_status(f->sup, &out, &out_len);
	assert_non_null(out);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, out, out_len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicydStatus(&st, root);
	assert_int_equal(st.ready, 1);
	capn_free(&c);
	free(out);
}

static void test_check_seat_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char *argv[] = { "sleep", "30", NULL };

	start_bits(f->sup, 1, 2, argv);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 1, 2);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision(out, out_len, Decision_allow);
	free(out);
	stop_bits(f->sup, 1, 2);
}

static void test_check_seat_bind_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char hex[PHRONESIS_ID_MAX];

	hex_agent(9, 10, hex);
	assert_int_equal(phronesis_supervisor_bind(f->sup, hex, "agent",
						   "/ws/proj", 0),
			 PHRONESIS_OK);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 9, 10);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision(out, out_len, Decision_allow);
	free(out);
	assert_int_equal(phronesis_supervisor_stop(f->sup, hex), PHRONESIS_OK);
}

static void test_admit_model_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct AdmitModel am;
	AdmitModel_ptr ap;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char *argv[] = { "sleep", "30", NULL };

	start_bits(f->sup, 3, 4, argv);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&am, 0, sizeof(am));
	am.agentId = mk_agent(capn_root(&c).seg, 3, 4);
	am.detail.len = 0;
	am.detail.str = "";
	am.detail.seg = NULL;
	ap = new_AdmitModel(capn_root(&c).seg);
	write_AdmitModel(&am, ap);
	assert_int_equal(capn_setp(capn_root(&c), 0, ap.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_admit_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision(out, out_len, Decision_allow);
	free(out);
	stop_bits(f->sup, 3, 4);
}

static void expect_decision_code(const uint8_t *msg, size_t len,
				 enum Decision want_dec, enum PolicyReason want_code,
				 uint64_t want_hi, uint64_t want_lo)
{
	struct capn c;
	PolicyDecision_ptr root;
	struct PolicyDecision d;
	struct AgentId agent;

	assert_non_null(msg);
	memset(&c, 0, sizeof(c));
	assert_int_equal(capn_init_mem(&c, msg, len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	assert_int_equal(d.decision, want_dec);
	assert_int_equal(d.code, want_code);
	memset(&agent, 0, sizeof(agent));
	if (d.agentId.p.type != CAPN_NULL)
		read_AgentId(&agent, d.agentId);
	assert_int_equal((int)agent.hi, (int)want_hi);
	assert_int_equal((int)agent.lo, (int)want_lo);
	capn_free(&c);
}

static void test_check_seat_null_id_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 0, 0);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_invalidMessage, 0, 0);
	free(out);
}

static void test_check_seat_unknown_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 1, 2);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_toolsDefaultDeny, 1, 2);
	free(out);
}

static void test_check_seat_stopped_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char *argv[] = { "true", NULL };

	start_bits(f->sup, 7, 8, argv);
	stop_bits(f->sup, 7, 8);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 7, 8);
	sc.action = SeatAction_listRuns;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_toolsDefaultDeny, 7, 8);
	free(out);
}

static void test_check_model_unknown_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct ModelCheck mc;
	ModelCheck_ptr mp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&mc, 0, sizeof(mc));
	mc.agentId = mk_agent(capn_root(&c).seg, 5, 6);
	mc.model.len = 0;
	mc.model.str = "";
	mc.model.seg = NULL;
	mp = new_ModelCheck(capn_root(&c).seg);
	write_ModelCheck(&mc, mp);
	assert_int_equal(capn_setp(capn_root(&c), 0, mp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_toolsDefaultDeny, 5, 6);
	free(out);
}

static void test_admit_model_null_id_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct AdmitModel am;
	AdmitModel_ptr ap;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&am, 0, sizeof(am));
	am.agentId = mk_agent(capn_root(&c).seg, 0, 0);
	am.detail.len = 0;
	am.detail.str = "";
	am.detail.seg = NULL;
	ap = new_AdmitModel(capn_root(&c).seg);
	write_AdmitModel(&am, ap);
	assert_int_equal(capn_setp(capn_root(&c), 0, ap.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_admit_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_invalidMessage, 0, 0);
	free(out);
}

static void test_check_seat_deny_all(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char *argv[] = { "sleep", "30", NULL };

	start_bits(f->sup, 1, 2, argv);
	setenv("PHRONESIS_DENY_ALL", "1", 1);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId = mk_agent(capn_root(&c).seg, 1, 2);
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny, PolicyReason_denyAll, 1,
			     2);
	free(out);
	unsetenv("PHRONESIS_DENY_ALL");
	stop_bits(f->sup, 1, 2);
}

static void test_check_model_deny_all(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct ModelCheck mc;
	ModelCheck_ptr mp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char *argv[] = { "sleep", "30", NULL };

	start_bits(f->sup, 5, 6, argv);
	setenv("PHRONESIS_DENY_ALL", "1", 1);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&mc, 0, sizeof(mc));
	mc.agentId = mk_agent(capn_root(&c).seg, 5, 6);
	mc.model.len = 0;
	mc.model.str = "";
	mc.model.seg = NULL;
	mp = new_ModelCheck(capn_root(&c).seg);
	write_ModelCheck(&mc, mp);
	assert_int_equal(capn_setp(capn_root(&c), 0, mp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny, PolicyReason_denyAll, 5,
			     6);
	free(out);
	unsetenv("PHRONESIS_DENY_ALL");
	stop_bits(f->sup, 5, 6);
}

static void check_audio_action(phronesis_supervisor_t *sup, enum AudioAction action,
			       enum Decision want_dec, enum PolicyReason want_code)
{
	struct capn c;
	struct AudioCheck ac;
	AudioCheck_ptr ap;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&ac, 0, sizeof(ac));
	ac.agentId = mk_agent(capn_root(&c).seg, 9, 10);
	ac.action = action;
	ap = new_AudioCheck(capn_root(&c).seg);
	write_AudioCheck(&ac, ap);
	assert_int_equal(capn_setp(capn_root(&c), 0, ap.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	phronesis_check_audio(sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, want_dec, want_code, 9, 10);
	free(out);
}

/* meta #97 Track E: default deny/prompt table */
static void test_check_audio_defaults(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("PHRONESIS_AUDIO_ALLOW");
	unsetenv("PHRONESIS_DENY_ALL");

	check_audio_action(f->sup, AudioAction_micOpen, Decision_deny,
			   PolicyReason_audioMicOpenDeny);
	check_audio_action(f->sup, AudioAction_listenArm, Decision_prompt,
			   PolicyReason_audioListenArmPrompt);
	check_audio_action(f->sup, AudioAction_alwaysListen, Decision_deny,
			   PolicyReason_audioAlwaysListenDeny);
	check_audio_action(f->sup, AudioAction_networkStt, Decision_deny,
			   PolicyReason_audioNetworkSttDeny);
	check_audio_action(f->sup, AudioAction_inject, Decision_deny,
			   PolicyReason_audioInjectDeny);
}

static void test_check_audio_fixture_allow(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("PHRONESIS_DENY_ALL");
	setenv("PHRONESIS_AUDIO_ALLOW", "1", 1);

	check_audio_action(f->sup, AudioAction_micOpen, Decision_allow,
			   PolicyReason_audioFixtureAllow);
	check_audio_action(f->sup, AudioAction_listenArm, Decision_allow,
			   PolicyReason_audioFixtureAllow);
	check_audio_action(f->sup, AudioAction_alwaysListen, Decision_allow,
			   PolicyReason_audioFixtureAllow);
	check_audio_action(f->sup, AudioAction_networkStt, Decision_allow,
			   PolicyReason_audioFixtureAllow);
	check_audio_action(f->sup, AudioAction_inject, Decision_allow,
			   PolicyReason_audioFixtureAllow);

	unsetenv("PHRONESIS_AUDIO_ALLOW");
}

static void test_check_audio_deny_all_wins(void **state)
{
	struct capn_fix *f = *state;

	setenv("PHRONESIS_AUDIO_ALLOW", "1", 1);
	setenv("PHRONESIS_DENY_ALL", "1", 1);

	check_audio_action(f->sup, AudioAction_listenArm, Decision_deny,
			   PolicyReason_denyAll);

	unsetenv("PHRONESIS_DENY_ALL");
	unsetenv("PHRONESIS_AUDIO_ALLOW");
}

static void test_check_audio_unknown_action(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("PHRONESIS_AUDIO_ALLOW");
	unsetenv("PHRONESIS_DENY_ALL");

	/* Out-of-range ordinal → deny audioUnknownAction (fail closed). */
	check_audio_action(f->sup, (enum AudioAction)99, Decision_deny,
			   PolicyReason_audioUnknownAction);
}

static void test_check_audio_bad_message(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *out = NULL;
	size_t out_len = 0;

	unsetenv("PHRONESIS_AUDIO_ALLOW");
	unsetenv("PHRONESIS_DENY_ALL");

	phronesis_check_audio(f->sup, NULL, 0, &out, &out_len);
	/* Bad input: zero agent echo + invalidMessage. */
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_invalidMessage, 0, 0);
	free(out);
}

int run_capnp_ffi_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_status, capn_setup,
						capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_allow,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_bind_allow,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_admit_model_allow,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_null_id_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_unknown_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_stopped_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_model_unknown_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_admit_model_null_id_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_deny_all,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_model_deny_all,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_audio_defaults,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_audio_fixture_allow,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_audio_deny_all_wins,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_audio_unknown_action,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_audio_bad_message,
						capn_setup, capn_teardown),
	};
	return cmocka_run_group_tests_name("capnp_ffi", tests, NULL, NULL);
}
