/* SPDX-License-Identifier: MIT */
#include "grok-policyd/supervisor.h"

const char *grok_policyd_version_string(void)
{
	return GROK_POLICYD_VERSION;
}

int grok_policyd_api_version(void)
{
	return GROK_POLICYD_API_VERSION;
}
