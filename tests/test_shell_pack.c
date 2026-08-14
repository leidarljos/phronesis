/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
#include "internal.h"
#include "policy.capnp.h"
#include "util.capnp.h"

#include <capnp_c.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static capn_text ctext(const char *s)
{
	capn_text t;

	t.len = s ? (int)strlen(s) : 0;
	t.str = s ? s : "";
	t.seg = NULL;
	return t;
}

static int write_msg(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf = NULL;
	size_t cap = 8192U;
	int64_t n;

	*out = NULL;
	*out_len = 0;
	for (;;) {
		buf = malloc(cap);
		assert_non_null(buf);
		n = capn_write_mem(c, buf, cap, 0);
		if (n >= 0)
			break;
		free(buf);
		cap *= 2U;
		assert_true(cap < 1024U * 1024U);
	}
	*out = buf;
	*out_len = (size_t)n;
	return 0;
}

static void build_shell_check(const char *cwd, char **argv, int argc,
			      uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct ShellCheck sc;
	ShellCheck_ptr root;
	capn_ptr list;
	int i;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&sc, 0, sizeof(sc));
	sc.agentId.p = new_AgentId(capn_root(&c).seg).p;
	{
		struct AgentId id = { .hi = 1, .lo = 2 };

		write_AgentId(&id, (AgentId_ptr){ .p = sc.agentId.p });
	}
	sc.cwd = ctext(cwd);
	/* List(Text): pointer list. capn_new_list(sz,0,1) is composite; set_text fails. */
	list = capn_new_ptr_list(capn_root(&c).seg, argc);
	for (i = 0; i < argc; i++)
		capn_set_text(list, i, ctext(argv[i]));
	sc.argv = list;
	root = new_ShellCheck(capn_root(&c).seg);
	write_ShellCheck(&sc, root);
	assert_int_equal(capn_setp(capn_root(&c), 0, root.p), 0);
	assert_int_equal(write_msg(&c, out, out_len), 0);
	capn_free(&c);
}

static void read_decision(const uint8_t *buf, size_t len, enum Decision *dec,
			  enum PolicyReason *code, char *reason, size_t reason_n)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr root;

	assert_int_equal(capn_init_mem(&c, buf, len, 0), 0);
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	*dec = d.decision;
	*code = d.code;
	if (reason && reason_n) {
		if (d.reason.str && d.reason.len > 0) {
			size_t n = (size_t)d.reason.len < reason_n - 1
					   ? (size_t)d.reason.len
					   : reason_n - 1;
			memcpy(reason, d.reason.str, n);
			reason[n] = '\0';
		} else {
			reason[0] = '\0';
		}
	}
	capn_free(&c);
}

static void write_file(const char *path, const char *body)
{
	FILE *f = fopen(path, "w");

	assert_non_null(f);
	fputs(body, f);
	fclose(f);
}

struct shell_fix {
	grok_supervisor_t *sup;
	char st[GROK_PATH_MAX];
	char rt[GROK_PATH_MAX];
	char ws[GROK_PATH_MAX];
};

static int shell_setup(void **state)
{
	struct shell_fix *f = calloc(1, sizeof(*f));
	char *argv0[] = { "true", NULL };
	char pack[GROK_PATH_MAX];
	const char *src;

	assert_non_null(f);
	assert_int_equal(t_open_pair(&f->sup, f->st, sizeof(f->st), f->rt,
				     sizeof(f->rt), "shpack"),
			 GROK_OK);
	snprintf(f->ws, sizeof(f->ws), "%s/ws", f->rt);
	assert_int_equal(mkdir(f->ws, 0700), 0);
	assert_int_equal(
		grok_supervisor_start(f->sup, "00000000000000010000000000000002",
				      NULL, f->ws, argv0),
		GROK_OK);

	src = getenv("POLICYD_SOURCE_ROOT");
	if (!src || !src[0])
		src = ".";
	snprintf(pack, sizeof(pack), "%s/policy/shell.janet", src);
	setenv("GROKOS_POLICYD_DEV_PACK", "1", 1);
	setenv("GROKOS_POLICYD_JANET_PACK", pack, 1);

	*state = f;
	return 0;
}

static int shell_teardown(void **state)
{
	struct shell_fix *f = *state;

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

static void test_true_allow(void **state)
{
	struct shell_fix *f = *state;
	char *argv[] = { "true", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	char reason[128];

	build_shell_check(f->ws, argv, 1, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, reason, sizeof(reason));
	assert_int_equal(dec, Decision_allow);
	assert_true(reason[0] != '\0'); /* pack-authored reason */
	free(out);
}

static void test_bare_python_deny(void **state)
{
	struct shell_fix *f = *state;
	char *argv[] = { "python3", "script.py", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	char reason[128];

	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, reason, sizeof(reason));
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_pythonRequiresUvRun);
	assert_non_null(strstr(reason, "uv run"));
	free(out);
}

/* Versioned basenames (python3.12) must hit python law, not fail-open. */
static void test_bare_python312_deny(void **state)
{
	struct shell_fix *f = *state;
	char *argv[] = { "/usr/bin/python3.12", "script.py", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_pythonRequiresUvRun);
	free(out);
}

static void test_uv_run_pep723_allow(void **state)
{
	struct shell_fix *f = *state;
	char script[GROK_PATH_MAX];
	char *argv[8];
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	char reason[128];

	snprintf(script, sizeof(script), "%s/ok.py", f->ws);
	write_file(script,
		   "# /// script\n"
		   "# requires-python = \">=3.11\"\n"
		   "# ///\n"
		   "print(1)\n");
	argv[0] = "uv";
	argv[1] = "run";
	argv[2] = script;
	argv[3] = NULL;
	build_shell_check(f->ws, argv, 3, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, reason, sizeof(reason));
	assert_int_equal(dec, Decision_allow);
	assert_int_equal(code, PolicyReason_shellExecAllow);
	free(out);
}

static void test_uv_run_missing_pep723_deny(void **state)
{
	struct shell_fix *f = *state;
	char script[GROK_PATH_MAX];
	char *argv[8];
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	snprintf(script, sizeof(script), "%s/bare.py", f->ws);
	write_file(script, "print(1)\n");
	argv[0] = "uv";
	argv[1] = "run";
	argv[2] = script;
	argv[3] = NULL;
	build_shell_check(f->ws, argv, 3, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_pythonMissingPep723);
	free(out);
}

static void test_python_dash_c_deny(void **state)
{
	struct shell_fix *f = *state;
	char *argv[] = { "uv", "run", "python", "-c", "print(1)", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	build_shell_check(f->ws, argv, 5, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_pythonDashCDenied);
	free(out);
}

static void test_glpat_in_argv_deny(void **state)
{
	struct shell_fix *f = *state;
	char url[96];
	snprintf(url, sizeof url, "https://oauth2:%s%s@gitlab.example/x.git",
		 "glpat-", "SecretTokenValue99");
	char *argv[] = {
		"git", "push",
		url,
		NULL
	};
uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	build_shell_check(f->ws, argv, 3, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_shellSecretInArgv);
	free(out);
}

static void test_cwd_pack_not_loaded(void **state)
{
	struct shell_fix *f = *state;
	char pdir[GROK_PATH_MAX], pack[GROK_PATH_MAX], oldcwd[GROK_PATH_MAX];
	char srcpack[GROK_PATH_MAX];
	char *argv[] = { "python3", "x.py", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	const char *src = getenv("POLICYD_SOURCE_ROOT");

	if (!src || !src[0])
		src = ".";
	snprintf(srcpack, sizeof(srcpack), "%s/policy/shell.janet", src);
	snprintf(pdir, sizeof(pdir), "%s/policy", f->rt);
	assert_int_equal(mkdir(pdir, 0700), 0);
	snprintf(pack, sizeof(pack), "%s/shell.janet", pdir);
	{
		FILE *fp = fopen(pack, "w");

		assert_non_null(fp);
		fputs("(defn shell-check [buf]\n"
		      "  (capnp/build-message 1 2\n"
		      "    @[[:u16 0 1] [:u16 2 20] [:text 0 \"cwd pack\"]]))\n",
		      fp);
		fclose(fp);
	}

	assert_non_null(getcwd(oldcwd, sizeof(oldcwd)));
	assert_int_equal(chdir(f->rt), 0);
	unsetenv("GROKOS_POLICYD_JANET_PACK");
	unsetenv("GROKOS_POLICYD_DEV_PACK");
	grok_policy_pack_reset();

	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(chdir(oldcwd), 0);

	assert_int_equal(dec, Decision_deny);
	assert_int_not_equal(code, PolicyReason_shellExecAllow);

	setenv("GROKOS_POLICYD_DEV_PACK", "1", 1);
	setenv("GROKOS_POLICYD_JANET_PACK", srcpack, 1);
	assert_int_equal(grok_policy_shell_pack_reload(srcpack), GROK_OK);
}

static void test_reload_shell_pack_hot_load(void **state)
{
	struct shell_fix *f = *state;
	char pack_a[GROK_PATH_MAX], pack_b[GROK_PATH_MAX];
	char *argv[] = { "python3", "x.py", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	const char *src = getenv("POLICYD_SOURCE_ROOT");
	struct capn c;
	struct ReloadShellPack rp;
	ReloadShellPack_ptr root;
	int rc;

	if (!src || !src[0])
		src = ".";
	snprintf(pack_a, sizeof(pack_a), "%s/policy/shell.janet", src);
	/* pack_b: allow-all shell pack (no python law) */
	snprintf(pack_b, sizeof(pack_b), "%s/allow_all.janet", f->ws);
	{
		FILE *fp = fopen(pack_b, "w");

		assert_non_null(fp);
		fputs("(defn shell-check [buf]\n"
		      "  (capnp/build-message 1 2\n"
		      "    @[[:u16 0 1] [:u16 2 20] [:text 0 \"allow all pack\"]]))\n",
		      fp);
		fclose(fp);
	}

	/* Reload to product UV pack → bare python deny */
	rc = grok_policy_shell_pack_reload(pack_a);
	assert_int_equal(rc, GROK_OK);
	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_pythonRequiresUvRun);

	/* Hot-load allow-all pack → same argv allows */
	rc = grok_policy_shell_pack_reload(pack_b);
	assert_int_equal(rc, GROK_OK);
	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_allow);
	assert_int_equal(code, PolicyReason_shellExecAllow);

	/* Cap'n reloadShellPack method */
	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&rp, 0, sizeof(rp));
	rp.path = ctext(pack_a);
	root = new_ReloadShellPack(capn_root(&c).seg);
	write_ReloadShellPack(&rp, root);
	assert_int_equal(capn_setp(capn_root(&c), 0, root.p), 0);
	assert_int_equal(write_msg(&c, &in, &in_len), 0);
	capn_free(&c);
	grok_policyd_reload_shell_pack(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_allow);
	assert_int_equal(code, PolicyReason_packReloaded);

	/* Invalid path */
	rc = grok_policy_shell_pack_reload("/no/such/pack.janet");
	assert_int_equal(rc, GROK_ERR_INVAL);
	rc = grok_policy_shell_pack_reload("relative.janet");
	assert_int_equal(rc, GROK_ERR_INVAL);
}

/* Multi-pack: allow-all + deny-true compose to deny (fail-closed). */
static void test_multi_pack_compose_deny(void **state)
{
	struct shell_fix *f = *state;
	char pack_allow[GROK_PATH_MAX], pack_deny[GROK_PATH_MAX],
		spec[GROK_PATH_MAX * 2];
	char *argv[] = { "true", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	int rc;

	snprintf(pack_allow, sizeof(pack_allow), "%s/allow_all.janet", f->ws);
	snprintf(pack_deny, sizeof(pack_deny), "%s/deny_true.janet", f->ws);
	write_file(pack_allow,
		   "(defn shell-check [buf]\n"
		   "  (capnp/build-message 1 2\n"
		   "    @[[:u16 0 1] [:u16 2 20] [:text 0 \"allow all\"]]))\n");
	/* Unconditional deny — composition must not keep the allow pack. */
	write_file(pack_deny,
		   "(defn shell-check [buf]\n"
		   "  (capnp/build-message 1 2\n"
		   "    @[[:u16 0 0] [:u16 2 25] [:text 0 \"deny pack\"]]))\n");

	snprintf(spec, sizeof(spec), "%s:%s", pack_allow, pack_deny);
	rc = grok_policy_shell_pack_reload(spec);
	assert_int_equal(rc, GROK_OK);

	build_shell_check(f->ws, argv, 1, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, 25);

	/* Reverse order: deny still wins. */
	snprintf(spec, sizeof(spec), "%s:%s", pack_deny, pack_allow);
	rc = grok_policy_shell_pack_reload(spec);
	assert_int_equal(rc, GROK_OK);
	build_shell_check(f->ws, argv, 1, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
}

/* packs.d directory: two packs + a non-entry .janet (skipped). */
static void test_multi_pack_dir(void **state)
{
	struct shell_fix *f = *state;
	char dir[GROK_PATH_MAX];
	char *argv[] = { "true", NULL };
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;
	int rc;
	char p1[GROK_PATH_MAX], p2[GROK_PATH_MAX], pdoc[GROK_PATH_MAX];

	snprintf(dir, sizeof(dir), "%s/packs.d", f->ws);
	assert_int_equal(mkdir(dir, 0700), 0);
	snprintf(p1, sizeof(p1), "%s/01-allow.janet", dir);
	snprintf(p2, sizeof(p2), "%s/02-deny.janet", dir);
	snprintf(pdoc, sizeof(pdoc), "%s/00-readme.janet", dir);
	write_file(pdoc, "(def docs \"not a pack entry\")\n");
	write_file(p1,
		   "(defn shell-check [buf]\n"
		   "  (capnp/build-message 1 2\n"
		   "    @[[:u16 0 1] [:u16 2 20] [:text 0 \"allow\"]]))\n");
	write_file(p2,
		   "(defn shell-check [buf]\n"
		   "  (capnp/build-message 1 2\n"
		   "    @[[:u16 0 0] [:u16 2 25] [:text 0 \"deny all\"]]))\n");

	rc = grok_policy_shell_pack_reload(dir);
	assert_int_equal(rc, GROK_OK);
	build_shell_check(f->ws, argv, 1, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, 25);
}

static void test_overlong_argv_denies(void **state)
{
	struct shell_fix *f = *state;
	char longarg[601];
	char *argv[3];
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	memset(longarg, 'A', 600);
	longarg[600] = '\0';
	argv[0] = "true";
	argv[1] = longarg;
	argv[2] = NULL;
	build_shell_check(f->ws, argv, 2, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_fieldTooLong);
}

static void test_overcount_argv_denies(void **state)
{
	struct shell_fix *f = *state;
	char *argv[258];
	char tok[] = "x";
	int i;
	uint8_t *in = NULL, *out = NULL;
	size_t in_len = 0, out_len = 0;
	enum Decision dec;
	enum PolicyReason code;

	argv[0] = "true";
	for (i = 1; i < 257; i++)
		argv[i] = tok;
	argv[257] = NULL;
	build_shell_check(f->ws, argv, 257, &in, &in_len);
	grok_policyd_check_shell(f->sup, in, in_len, &out, &out_len);
	free(in);
	read_decision(out, out_len, &dec, &code, NULL, 0);
	free(out);
	assert_int_equal(dec, Decision_deny);
	assert_int_equal(code, PolicyReason_fieldTooLong);
}

int run_shell_pack_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_true_allow, shell_setup,
						shell_teardown),
		cmocka_unit_test_setup_teardown(test_bare_python_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_bare_python312_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_uv_run_pep723_allow,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_uv_run_missing_pep723_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_python_dash_c_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_glpat_in_argv_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_cwd_pack_not_loaded,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_overlong_argv_denies,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_overcount_argv_denies,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_reload_shell_pack_hot_load,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_multi_pack_compose_deny,
						shell_setup, shell_teardown),
		cmocka_unit_test_setup_teardown(test_multi_pack_dir, shell_setup,
						shell_teardown),
	};
	return cmocka_run_group_tests_name("shell_pack", tests, NULL, NULL);
}
