/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Pure unit tests for policy/lib .janet files files (no Cap'n, no supervisor).
 * Loads the same files the pack host loads before shell.janet.
 */
#include "harness.h"

#include "janet.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char lib_dir[1024];
static JanetTable *test_env;
static int janet_ready;

static int load_lib_file(const char *name)
{
	char path[1200];
	FILE *f;
	char *buf;
	long sz;
	Janet out;
	int rc;

	if (snprintf(path, sizeof(path), "%s/%s", lib_dir, name) >=
	    (int)sizeof(path))
		return -1;
	f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0 ||
	    fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	buf = malloc((size_t)sz);
	if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	rc = janet_dobytes(test_env, (const uint8_t *)buf, (int32_t)sz, path,
			   &out);
	free(buf);
	return rc == 0 ? 0 : -1;
}

static int pack_lib_setup(void **state)
{
	const char *src = getenv("POLICYD_SOURCE_ROOT");
	Janet resolved = janet_wrap_nil();
	(void)state;

	if (!src || !src[0])
		src = ".";
	if (snprintf(lib_dir, sizeof(lib_dir), "%s/policy/lib", src) >=
	    (int)sizeof(lib_dir))
		return -1;

	if (!janet_ready) {
		janet_init();
		janet_ready = 1;
	}
	test_env = janet_core_env(NULL);
	assert_non_null(test_env);
	/* No capnp_register / seal: pure law only. */
	assert_int_equal(load_lib_file("layout.janet"), 0);
	assert_int_equal(load_lib_file("python-law.janet"), 0);
	assert_int_equal(load_lib_file("shell-danger.janet"), 0);
	assert_int_equal(load_lib_file("shell-secret.janet"), 0);

	/* Sanity: entry helpers are bound. */
	assert_int_not_equal(
		janet_resolve(test_env, janet_csymbol("python-interp?"),
			      &resolved),
		JANET_BINDING_NONE);
	return 0;
}

static int pack_lib_teardown(void **state)
{
	(void)state;
	test_env = NULL;
	return 0;
}

static Janet call1(const char *name, Janet a0)
{
	Janet resolved = janet_wrap_nil();
	JanetFunction *fn;
	Janet res;
	JanetFiber *fiber = NULL;
	Janet args[1];
	JanetSignal sig;
	JanetBindingType bt;

	bt = janet_resolve(test_env, janet_csymbol(name), &resolved);
	assert_int_not_equal(bt, JANET_BINDING_NONE);
	assert_true(janet_checktype(resolved, JANET_FUNCTION));
	fn = janet_unwrap_function(resolved);
	args[0] = a0;
	sig = janet_pcall(fn, 1, args, &res, &fiber);
	assert_int_equal(sig, JANET_SIGNAL_OK);
	return res;
}

static int truthy(Janet v)
{
	return !janet_checktype(v, JANET_NIL) &&
	       !(janet_checktype(v, JANET_BOOLEAN) &&
		 !janet_unwrap_boolean(v));
}

static void test_python_interp_names(void **state)
{
	Janet v;
	(void)state;

	v = call1("python-interp?", janet_cstringv("python"));
	assert_true(truthy(v));
	v = call1("python-interp?", janet_cstringv("python3"));
	assert_true(truthy(v));
	v = call1("python-interp?", janet_cstringv("python3.12"));
	assert_true(truthy(v));
	v = call1("python-interp?", janet_cstringv("python3.12.1"));
	assert_true(truthy(v));
	v = call1("python-interp?", janet_cstringv("pypy3"));
	assert_true(truthy(v));
	v = call1("python-interp?", janet_cstringv("pythonx"));
	assert_false(truthy(v));
	v = call1("python-interp?", janet_cstringv("python3."));
	assert_false(truthy(v));
}

static Janet make_string_array(const char **xs, int n)
{
	JanetArray *a = janet_array(n);
	int i;

	for (i = 0; i < n; i++)
		janet_array_push(a, janet_cstringv(xs[i]));
	return janet_wrap_array(a);
}

static void test_uv_and_dash_c(void **state)
{
	const char *uv[] = { "uv", "run", "x.py" };
	const char *bare[] = { "python3", "x.py" };
	const char *dash[] = { "uv", "run", "python3.12", "-c", "1" };
	Janet v;
	(void)state;

	v = call1("uv-run?", make_string_array(uv, 3));
	assert_true(truthy(v));
	v = call1("uv-run?", make_string_array(bare, 2));
	assert_false(truthy(v));
	v = call1("python-dash-c?", make_string_array(dash, 5));
	assert_true(truthy(v));
	v = call1("touches-python?", make_string_array(bare, 2));
	assert_true(truthy(v));
}

static void test_pep723_heads(void **state)
{
	const char *good =
		"# /// script\n# requires-python = \">=3.11\"\n# ///\nprint(1)\n";
	const char *bad = "print(1)\n";
	const char *mid = "x # /// script\n# ///\n";
	const char *after =
		"code\n# /// script\n# deps\n# ///\n";
	Janet v;
	(void)state;

	v = call1("pep723?", janet_cstringv(good));
	assert_true(truthy(v));
	v = call1("pep723?", janet_cstringv(bad));
	assert_false(truthy(v));
	v = call1("pep723?", janet_cstringv(mid));
	assert_false(truthy(v));
	v = call1("pep723?", janet_cstringv(after));
	assert_true(truthy(v));
}

static void test_argv_base(void **state)
{
	Janet v;
	(void)state;

	v = call1("argv-base", janet_cstringv("/usr/bin/python3.12"));
	assert_true(janet_checktype(v, JANET_STRING));
	assert_string_equal((const char *)janet_unwrap_string(v), "python3.12");
}

static void test_shell_danger_laws(void **state)
{
	const char *sudo[] = { "sudo", "apt", "install", "x" };
	const char *curlsh[] = { "curl", "https://x", "sh" };
	const char *poetry[] = { "poetry", "install" };
	const char *pipi[] = { "pip", "install", "requests" };
	const char *force[] = { "git", "push", "--force", "origin", "main" };
	const char *okgit[] = { "git", "status" };
	Janet v;
	(void)state;

	v = call1("shell-danger-deny", make_string_array(sudo, 4));
	assert_true(janet_checktype(v, JANET_TUPLE) ||
		    janet_checktype(v, JANET_ARRAY));
	v = call1("shell-danger-deny", make_string_array(curlsh, 3));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-danger-deny", make_string_array(poetry, 2));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-danger-deny", make_string_array(pipi, 3));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-danger-deny", make_string_array(force, 5));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-danger-deny", make_string_array(okgit, 2));
	assert_true(janet_checktype(v, JANET_NIL));
}

static void test_shell_secret_laws(void **state)
{
	/* Build fixtures without contiguous secret shapes in source (CI secrets:scan). */
	char glpat_url[96];
	char ghp_hdr[64];
	char pem_hdr[48];
	snprintf(glpat_url, sizeof glpat_url, "https://oauth2:%s%s@gitlab.com/x.git",
		 "glpat-", "abc123XYZ");
	snprintf(ghp_hdr, sizeof ghp_hdr, "Authorization: %s%s", "ghp_",
		 "abcdefghijklmnopqrstuv");
	snprintf(pem_hdr, sizeof pem_hdr, "-----%s%s%s", "BEGIN ", "RSA ",
		 "PRIVATE KEY-----");
	const char *glpat[] = { "git", "push", glpat_url };
	const char *ghp[] = { "curl", "-H", ghp_hdr };
	const char *ok[] = { "git", "status" };
	const char *pem[] = { "cat", pem_hdr };
	Janet v;
	(void)state;

	v = call1("shell-secret-deny", make_string_array(glpat, 3));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-secret-deny", make_string_array(ghp, 3));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-secret-deny", make_string_array(pem, 2));
	assert_false(janet_checktype(v, JANET_NIL));
	v = call1("shell-secret-deny", make_string_array(ok, 2));
	assert_true(janet_checktype(v, JANET_NIL));
}

int run_pack_lib_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_python_interp_names,
						pack_lib_setup,
						pack_lib_teardown),
		cmocka_unit_test_setup_teardown(test_uv_and_dash_c,
						pack_lib_setup,
						pack_lib_teardown),
		cmocka_unit_test_setup_teardown(test_pep723_heads,
						pack_lib_setup,
						pack_lib_teardown),
		cmocka_unit_test_setup_teardown(test_argv_base, pack_lib_setup,
						pack_lib_teardown),
		cmocka_unit_test_setup_teardown(test_shell_danger_laws,
						pack_lib_setup,
						pack_lib_teardown),
		cmocka_unit_test_setup_teardown(test_shell_secret_laws,
						pack_lib_setup,
						pack_lib_teardown),
	};
	return cmocka_run_group_tests_name("pack_lib", tests, NULL, NULL);
}
