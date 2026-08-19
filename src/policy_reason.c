/* SPDX-License-Identifier: Apache-2.0 */
#include "grok-policyd/supervisor.h"

#include <string.h>

void policyd_policy_result_set(policyd_policy_result_t *out, policyd_decision_t decision,
			    policyd_policy_reason_t code)
{
	if (!out)
		return;
	memset(out, 0, sizeof(*out));
	out->decision = decision;
	out->code = code;
	/* reason stays empty — viewers map code. */
}
