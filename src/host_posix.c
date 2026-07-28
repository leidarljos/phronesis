/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Portable CLI host: signal stop flag only.
 * No libsystemd, no libcap. Used when -Dsystemd=disabled (or auto miss).
 * Serve drives nng with RECVTIMEO and polls grok_host_should_stop().
 */
#include "internal.h"

#include <signal.h>
#include <string.h>

static volatile sig_atomic_t g_stop;

static void on_signal(int signo)
{
	(void)signo;
	g_stop = 1;
}

int grok_host_init(void)
{
	struct sigaction sa;

	g_stop = 0;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	/* No SA_RESTART: allow blocking calls to surface EINTR where used. */
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return GROK_ERR_IO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
		return GROK_ERR_IO;
	return GROK_OK;
}

void grok_host_fini(void)
{
	/* nothing to tear down */
}

int grok_host_should_stop(void)
{
	return g_stop ? 1 : 0;
}

void grok_host_request_stop(void)
{
	g_stop = 1;
}

void grok_host_notify_ready(void)
{
	/* no service manager */
}

void grok_host_notify_stopping(void)
{
}

void grok_host_watchdog_ping(void)
{
}

int grok_host_drop_bounding_caps(void)
{
	return GROK_OK;
}

const char *grok_host_backend_name(void)
{
	return "posix";
}
