/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>

void test_paths_suite(void);
void test_lifecycle_suite(void);
void test_kill_tree_suite(void);
void test_action_log_suite(void);
void test_persist_suite(void);
void test_policy_suite(void);

int main(void)
{
	g_failures = 0;
	g_tests = 0;

	fprintf(stderr, "=== paths ===\n");
	test_paths_suite();
	fprintf(stderr, "=== lifecycle ===\n");
	test_lifecycle_suite();
	fprintf(stderr, "=== kill_tree ===\n");
	test_kill_tree_suite();
	fprintf(stderr, "=== action_log ===\n");
	test_action_log_suite();
	fprintf(stderr, "=== persist ===\n");
	test_persist_suite();
	fprintf(stderr, "=== policy ===\n");
	test_policy_suite();

	fprintf(stderr, "\n%d test cases, %d failure(s)\n", g_tests, g_failures);
	if (g_failures) {
		fprintf(stderr, "FAILED\n");
		return 1;
	}
	printf("ok: %d supervisor test cases passed\n", g_tests);
	return 0;
}
