/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <stdio.h>

int main(void)
{
	int fails = 0;

	fails += run_paths_tests();
	fails += run_lifecycle_tests();
	fails += run_kill_tree_tests();
	fails += run_action_log_tests();
	fails += run_persist_tests();
	fails += run_policy_tests();
	fails += run_version_tests();
	fails += run_wire_serve_tests();
	fails += run_wire_frame_tests();

	if (fails) {
		fprintf(stderr, "FAILED: %d suite failure group(s)\n", fails);
		return 1;
	}
	printf("ok: cmocka suites passed\n");
	return 0;
}
