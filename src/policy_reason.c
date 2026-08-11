/* SPDX-License-Identifier: MIT */
#include "grok-policyd/supervisor.h"

#include <string.h>

void grok_policy_result_set(grok_policy_result_t *out, grok_decision_t decision,
			    grok_policy_reason_t code)
{
	if (!out)
		return;
	memset(out, 0, sizeof(*out));
	out->decision = decision;
	out->code = code;
	/* reason stays empty — viewers map code. */
}
