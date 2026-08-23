/* SPDX-License-Identifier: Apache-2.0 */
#include "phronesis/supervisor.h"

const char *phronesis_version_string(void)
{
	return PHRONESIS_VERSION;
}

int phronesis_api_version(void)
{
	return PHRONESIS_API_VERSION;
}
