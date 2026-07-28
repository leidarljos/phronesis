/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "harness.h"
#include "wire/serve.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmocka.h>

#include "internal.h"

static void test_ensure_parent_does_not_chmod_existing(void **state)
{
	char base[256];
	char sock[300];
	struct stat st_before, st_after;
	mode_t mode_before;

	(void)state;
	snprintf(base, sizeof(base), "/tmp/policyd-parent-cmocka-%d", (int)getpid());
	(void)rmdir(base);
	assert_int_equal(mkdir(base, 0755), 0);
	assert_int_equal(stat(base, &st_before), 0);
	mode_before = st_before.st_mode & 0777;
	assert_true(mode_before != 0700);
	snprintf(sock, sizeof(sock), "%s/policyd.sock", base);
	assert_int_equal(grok_unix_ensure_socket_parent(sock), GROK_OK);
	assert_int_equal(stat(base, &st_after), 0);
	assert_int_equal(st_after.st_mode & 0777, mode_before);
	assert_true((st_after.st_mode & 0777) != 0700);
	assert_int_equal(rmdir(base), 0);
}

static void test_ensure_parent_creates_leaf(void **state)
{
	char base[256];
	char nested[300];
	char sock[350];
	struct stat st;

	(void)state;
	snprintf(base, sizeof(base), "/tmp/policyd-parent-create-%d", (int)getpid());
	(void)rmdir(base);
	assert_int_equal(mkdir(base, 0755), 0);
	snprintf(nested, sizeof(nested), "%s/leaf", base);
	snprintf(sock, sizeof(sock), "%s/p.sock", nested);
	assert_int_equal(grok_unix_ensure_socket_parent(sock), GROK_OK);
	assert_int_equal(stat(nested, &st), 0);
	assert_int_equal(st.st_mode & 0777, 0700);
	assert_int_equal(rmdir(nested), 0);
	assert_int_equal(rmdir(base), 0);
}

static void test_map_admit_kind_known(void **state)
{
	const char *tool = NULL;
	const char *action = NULL;

	(void)state;
	assert_int_equal(grok_policyd_map_admit_kind("seat", &tool, &action), 0);
	assert_string_equal(tool, "seat");
	assert_string_equal(action, "publish_run");

	assert_int_equal(grok_policyd_map_admit_kind("model", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(grok_policyd_map_admit_kind("", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");

	assert_int_equal(grok_policyd_map_admit_kind("agent", &tool, &action), 0);
	assert_string_equal(tool, "model");
	assert_string_equal(action, "start");
}

static void test_map_admit_kind_unknown_fail_closed(void **state)
{
	const char *tool = "model";
	const char *action = "start";

	(void)state;
	assert_int_equal(grok_policyd_map_admit_kind("weird", &tool, &action), -1);
	assert_int_equal(grok_policyd_map_admit_kind("shell", &tool, &action), -1);
	assert_int_equal(grok_policyd_map_admit_kind(NULL, &tool, &action), -1);
}

int run_wire_serve_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ensure_parent_does_not_chmod_existing),
		cmocka_unit_test(test_ensure_parent_creates_leaf),
		cmocka_unit_test(test_map_admit_kind_known),
		cmocka_unit_test(test_map_admit_kind_unknown_fail_closed),
	};
	return cmocka_run_group_tests_name("wire_serve", tests, NULL, NULL);
}
