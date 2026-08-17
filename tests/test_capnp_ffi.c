/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Policyd Cap'n methods: params root in, PolicyDecision root out (no unions).
 */
#include "harness.h"
#include "grok-policyd/supervisor.h"
#include "internal.h"
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
#include <cmocka.h>

struct capn_fix {
	grok_supervisor_t *sup;
	char st[GROK_PATH_MAX];
	char rt[GROK_PATH_MAX];
};

static int capn_setup(void **state)
{
	struct capn_fix *f = calloc(1, sizeof(*f));
	char pack[GROK_PATH_MAX];
	const char *src;

	assert_non_null(f);
	assert_int_equal(t_open_pair(&f->sup, f->st, sizeof(f->st), f->rt,
				     sizeof(f->rt), "capn"),
			 GROK_OK);
	/* Product pack: shell-check + audio-check (absolute; reload after shell suite). */
	src = getenv("POLICYD_SOURCE_ROOT");
	if (!src || !src[0])
		src = ".";
	snprintf(pack, sizeof(pack), "%s/policy/shell.janet", src);
	setenv("GROKOS_POLICYD_DEV_PACK", "1", 1);
	setenv("GROKOS_POLICYD_JANET_PACK", pack, 1);
	assert_int_equal(grok_policy_shell_pack_reload(pack), GROK_OK);
	*state = f;
	return 0;
}

static int capn_teardown(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("GROKOS_POLICYD_JANET_PACK");
	unsetenv("GROKOS_POLICYD_DEV_PACK");
	if (f) {
		if (f->sup)
			grok_supervisor_close(f->sup);
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

static AgentId_ptr mk_agent(struct capn_segment *seg, uint64_t hi, uint64_t lo)
{
	struct AgentId id = { .hi = hi, .lo = lo };
	AgentId_ptr p = new_AgentId(seg);

	write_AgentId(&id, p);
	return p;
}

static void start_hex_agent(grok_supervisor_t *sup, uint64_t hi, uint64_t lo)
{
	char hex[GROK_ID_MAX];
	char *argv[] = { "sleep", "30", NULL };

	grok_agent_id_to_hex(hi, lo, hex);
	assert_true(hex[0] != '\0');
	assert_int_equal(grok_supervisor_start(sup, hex, NULL, "/ws/proj", argv),
			 GROK_OK);
}

static void stop_hex_agent(grok_supervisor_t *sup, uint64_t hi, uint64_t lo)
{
	char hex[GROK_ID_MAX];

	grok_agent_id_to_hex(hi, lo, hex);
	assert_int_equal(grok_supervisor_stop(sup, hex), GROK_OK);
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

static void test_status(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *out = NULL;
	size_t out_len = 0;
	struct capn c;
	PolicydStatus_ptr root;
	struct PolicydStatus st;

	grok_policyd_status(f->sup, &out, &out_len);
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

	start_hex_agent(f->sup, 1, 2);

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

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_allow,
			     PolicyReason_seatBoardAllow, 1, 2);
	free(out);
	stop_hex_agent(f->sup, 1, 2);
}

static void test_check_seat_bind_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	char hex[GROK_ID_MAX];

	grok_agent_id_to_hex(9, 10, hex);
	assert_int_equal(grok_supervisor_bind(f->sup, hex, "agent", "/ws/proj", 0),
			 GROK_OK);

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

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_allow,
			     PolicyReason_seatBoardAllow, 9, 10);
	free(out);
	assert_int_equal(grok_supervisor_stop(f->sup, hex), GROK_OK);
}

static void test_admit_model_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct AdmitModel am;
	AdmitModel_ptr ap;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	start_hex_agent(f->sup, 3, 4);

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

	grok_policyd_admit_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_allow,
			     PolicyReason_modelStartAllow, 3, 4);
	free(out);
	stop_hex_agent(f->sup, 3, 4);
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
	sc.action = SeatAction_publishRun;
	sp = new_SeatCheck(capn_root(&c).seg);
	write_SeatCheck(&sc, sp);
	assert_int_equal(capn_setp(capn_root(&c), 0, sp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_invalidMessage, 0, 0);
	free(out);
}

static void test_check_model_null_id_deny(void **state)
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
	mc.model.len = 0;
	mc.model.str = "";
	mc.model.seg = NULL;
	mp = new_ModelCheck(capn_root(&c).seg);
	write_ModelCheck(&mc, mp);
	assert_int_equal(capn_setp(capn_root(&c), 0, mp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	grok_policyd_check_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_invalidMessage, 0, 0);
	free(out);
}

static void test_check_seat_unknown_id_deny(void **state)
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

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_toolsDefaultDeny, 1, 2);
	free(out);
}

static void test_check_model_allow(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct ModelCheck mc;
	ModelCheck_ptr mp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	start_hex_agent(f->sup, 5, 6);

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

	grok_policyd_check_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_allow,
			     PolicyReason_modelStartAllow, 5, 6);
	free(out);
	stop_hex_agent(f->sup, 5, 6);
}

static void test_check_seat_stopped_id_deny(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	start_hex_agent(f->sup, 7, 8);
	stop_hex_agent(f->sup, 7, 8);

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

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny,
			     PolicyReason_toolsDefaultDeny, 7, 8);
	free(out);
}

/* DENY_ALL must cover Cap'n PolicyDecision entries that skipped policy_eval. */
static void test_check_seat_deny_all(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct SeatCheck sc;
	SeatCheck_ptr sp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	setenv("GROKOS_POLICYD_DENY_ALL", "1", 1);

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

	grok_policyd_check_seat(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny, PolicyReason_denyAll, 1, 2);
	free(out);
	unsetenv("GROKOS_POLICYD_DENY_ALL");
}

static void test_check_model_deny_all(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct ModelCheck mc;
	ModelCheck_ptr mp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	setenv("GROKOS_POLICYD_DENY_ALL", "1", 1);

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

	grok_policyd_check_model(f->sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, Decision_deny, PolicyReason_denyAll, 5, 6);
	free(out);
	unsetenv("GROKOS_POLICYD_DENY_ALL");
}

static void test_check_risk_deny_all(void **state)
{
	struct capn_fix *f = *state;
	struct capn c;
	struct RiskCheck rc;
	RiskCheck_ptr rp;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;

	setenv("GROKOS_POLICYD_DENY_ALL", "1", 1);

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&rc, 0, sizeof(rc));
	rc.agentId = mk_agent(capn_root(&c).seg, 7, 8);
	rc.action = RiskAction_network;
	rp = new_RiskCheck(capn_root(&c).seg);
	write_RiskCheck(&rc, rp);
	assert_int_equal(capn_setp(capn_root(&c), 0, rp.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);

	grok_policyd_check_risk(f->sup, in, in_len, &out, &out_len);
	free(in);
	/* Without DENY_ALL this would be prompt; flag must force deny. */
	expect_decision_code(out, out_len, Decision_deny, PolicyReason_denyAll, 7, 8);
	free(out);
	unsetenv("GROKOS_POLICYD_DENY_ALL");
}

static void test_check_risk_secret_export_deny(void **state)
{
	struct capn_fix *f = *state;
	struct {
		enum RiskAction action;
		enum Decision dec;
		enum PolicyReason code;
	} cases[] = {
		{ RiskAction_secretExport, Decision_deny,
		  PolicyReason_secretExportDenied },
		{ RiskAction_network, Decision_prompt,
		  PolicyReason_highRiskPrompt },
	};
	size_t i;

	unsetenv("GROKOS_POLICYD_DENY_ALL");
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct capn c;
		struct RiskCheck rc;
		RiskCheck_ptr rp;
		uint8_t *in = NULL, *out = NULL;
		size_t in_len = 0, out_len = 0;

		memset(&c, 0, sizeof(c));
		capn_init_malloc(&c);
		memset(&rc, 0, sizeof(rc));
		rc.agentId = mk_agent(capn_root(&c).seg, 11, 12);
		rc.action = cases[i].action;
		rp = new_RiskCheck(capn_root(&c).seg);
		write_RiskCheck(&rc, rp);
		assert_int_equal(capn_setp(capn_root(&c), 0, rp.p), 0);
		assert_int_equal(write_msg(&c, &in, &in_len), 0);
		capn_free(&c);

		grok_policyd_check_risk(f->sup, in, in_len, &out, &out_len);
		free(in);
		expect_decision_code(out, out_len, cases[i].dec, cases[i].code,
				     11, 12);
		free(out);
	}
}

static void test_check_path_write_vs_delete(void **state)
{
	struct capn_fix *f = *state;
	char id[GROK_ID_MAX];
	char *argv[] = { "true", NULL };
	const char *path = "/ws/proj/out";
	struct {
		enum PathAction action;
		enum Decision dec;
		enum PolicyReason code;
	} cases[] = {
		{ PathAction_write, Decision_allow,
		  PolicyReason_pathUnderWorkspaceAllow },
		{ PathAction_delete, Decision_prompt,
		  PolicyReason_highRiskPrompt },
	};
	size_t i;

	unsetenv("GROKOS_POLICYD_DENY_ALL");
	grok_agent_id_to_hex(13, 14, id);
	assert_int_equal(
		grok_supervisor_start(f->sup, id, NULL, "/ws/proj", argv),
		GROK_OK);

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct capn c;
		struct PathCheck pc;
		PathCheck_ptr pp;
		uint8_t *in = NULL, *out = NULL;
		size_t in_len = 0, out_len = 0;

		memset(&c, 0, sizeof(c));
		capn_init_malloc(&c);
		memset(&pc, 0, sizeof(pc));
		pc.agentId = mk_agent(capn_root(&c).seg, 13, 14);
		pc.action = cases[i].action;
		pc.path.len = (int)strlen(path);
		pc.path.str = path;
		pc.path.seg = NULL;
		pp = new_PathCheck(capn_root(&c).seg);
		write_PathCheck(&pc, pp);
		assert_int_equal(capn_setp(capn_root(&c), 0, pp.p), 0);
		assert_int_equal(write_msg(&c, &in, &in_len), 0);
		capn_free(&c);

		grok_policyd_check_path(f->sup, in, in_len, &out, &out_len);
		free(in);
		expect_decision_code(out, out_len, cases[i].dec, cases[i].code,
				     13, 14);
		free(out);
	}
	assert_int_equal(grok_supervisor_stop(f->sup, id), GROK_OK);
}

static void check_audio_action(grok_supervisor_t *sup, enum AudioAction action,
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

	grok_policyd_check_audio(sup, in, in_len, &out, &out_len);
	free(in);
	expect_decision_code(out, out_len, want_dec, want_code, 9, 10);
	free(out);
}

/* meta #97 Track E: default deny/prompt table */
static void test_check_audio_defaults(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("GROKOS_POLICYD_AUDIO_ALLOW");
	unsetenv("GROKOS_POLICYD_DENY_ALL");

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

	unsetenv("GROKOS_POLICYD_DENY_ALL");
	setenv("GROKOS_POLICYD_AUDIO_ALLOW", "1", 1);

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

	unsetenv("GROKOS_POLICYD_AUDIO_ALLOW");
}

static void test_check_audio_deny_all_wins(void **state)
{
	struct capn_fix *f = *state;

	setenv("GROKOS_POLICYD_AUDIO_ALLOW", "1", 1);
	setenv("GROKOS_POLICYD_DENY_ALL", "1", 1);

	check_audio_action(f->sup, AudioAction_listenArm, Decision_deny,
			   PolicyReason_denyAll);

	unsetenv("GROKOS_POLICYD_DENY_ALL");
	unsetenv("GROKOS_POLICYD_AUDIO_ALLOW");
}

static void test_check_audio_unknown_action(void **state)
{
	struct capn_fix *f = *state;

	unsetenv("GROKOS_POLICYD_AUDIO_ALLOW");
	unsetenv("GROKOS_POLICYD_DENY_ALL");

	/* Out-of-range ordinal → deny audioUnknownAction (fail closed). */
	check_audio_action(f->sup, (enum AudioAction)99, Decision_deny,
			   PolicyReason_audioUnknownAction);
}

static void test_check_audio_bad_message(void **state)
{
	struct capn_fix *f = *state;
	uint8_t *out = NULL;
	size_t out_len = 0;

	unsetenv("GROKOS_POLICYD_AUDIO_ALLOW");
	unsetenv("GROKOS_POLICYD_DENY_ALL");

	grok_policyd_check_audio(f->sup, NULL, 0, &out, &out_len);
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
		cmocka_unit_test_setup_teardown(test_check_model_null_id_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_unknown_id_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_model_allow,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_stopped_id_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_seat_deny_all,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_model_deny_all,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_risk_deny_all,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_risk_secret_export_deny,
						capn_setup, capn_teardown),
		cmocka_unit_test_setup_teardown(test_check_path_write_vs_delete,
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
