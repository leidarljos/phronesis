/* SPDX-License-Identifier: MIT */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

static void test_version_string(void **state)
{
	(void)state;
	assert_non_null(grok_policyd_version_string());
	assert_string_equal(grok_policyd_version_string(), GROK_POLICYD_VERSION);
	assert_int_equal(grok_policyd_api_version(), GROK_POLICYD_API_VERSION);
	assert_true(GROK_POLICYD_API_VERSION >= 1);
}

/*
 * Catch VERSION/header drift: package major in the header must match the
 * first component of GROK_POLICYD_VERSION (SONAME key = package major).
 * scripts/check-version.sh is the authoritative gate; this is a runtime belt.
 */
static void test_package_major_matches_version_string(void **state)
{
	const char *v = GROK_POLICYD_VERSION;
	int major = 0;
	(void)state;
	assert_true(sscanf(v, "%d.", &major) == 1);
	assert_int_equal(major, GROK_POLICYD_VERSION_MAJOR);
}

static void test_api_version_is_positive_link_counter(void **state)
{
	(void)state;
	/* API generation is independent of package major (SONAME). */
	assert_true(GROK_POLICYD_API_VERSION >= 1);
	assert_int_equal(grok_policyd_api_version(), GROK_POLICYD_API_VERSION);
}

int run_version_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_version_string),
		cmocka_unit_test(test_package_major_matches_version_string),
		cmocka_unit_test(test_api_version_is_positive_link_counter),
	};
	return cmocka_run_group_tests_name("version", tests, NULL, NULL);
}
