/* SPDX-License-Identifier: MIT */
#include "phronesis/supervisor.h"

#include <string.h>

void phronesis_policy_result_set(phronesis_policy_result_t *out, phronesis_decision_t decision,
			    phronesis_policy_reason_t code)
{
	if (!out)
		return;
	memset(out, 0, sizeof(*out));
	out->decision = decision;
	out->code = code;
	/* reason stays empty — viewers map code. */
}
