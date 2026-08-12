/* SPDX-License-Identifier: Apache-2.0 */
#include "phronesis/supervisor.h"

const char *policyd_version_string(void)
{
	return POLICYD_VERSION;
}

int policyd_api_version(void)
{
	return POLICYD_API_VERSION;
}
