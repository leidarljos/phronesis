/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CLI host glue (not in libgrok_policyd.so).
 * Signals set a stop flag; Cap'n serve is nng-native and polls it.
 * Optional Linux: sd_notify READY/STOPPING/WATCHDOG + libcap clear.
 */
#include "internal.h"

#include <signal.h>
#include <string.h>

#if defined(GROK_HAVE_SYSTEMD)
#include <unistd.h>
#include <systemd/sd-daemon.h>
#endif

#if defined(GROK_HAVE_LIBCAP)
#include <sys/capability.h>
#include <unistd.h>
#endif

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
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return GROK_ERR_IO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
		return GROK_ERR_IO;
	return GROK_OK;
}

void grok_host_fini(void)
{
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
#if defined(GROK_HAVE_SYSTEMD)
	(void)sd_notify(0, "READY=1\nSTATUS=grok-policyd Cap'n peer\n");
#endif
}

void grok_host_notify_stopping(void)
{
#if defined(GROK_HAVE_SYSTEMD)
	(void)sd_notify(0, "STOPPING=1\n");
#endif
}

void grok_host_watchdog_ping(void)
{
#if defined(GROK_HAVE_SYSTEMD)
	(void)sd_notify(0, "WATCHDOG=1\n");
#endif
}

int grok_host_drop_bounding_caps(void)
{
#if defined(GROK_HAVE_LIBCAP)
	cap_t caps;

	if (geteuid() != 0 && getuid() != 0)
		return GROK_OK;
	caps = cap_get_proc();
	if (!caps)
		return GROK_OK;
	if (cap_clear(caps) != 0 || cap_set_proc(caps) != 0) {
		cap_free(caps);
		return GROK_ERR_IO;
	}
	cap_free(caps);
	return GROK_OK;
#else
	return GROK_OK;
#endif
}

const char *grok_host_backend_name(void)
{
#if defined(GROK_HAVE_SYSTEMD) && defined(GROK_HAVE_LIBCAP)
	return "linux-systemd+libcap";
#elif defined(GROK_HAVE_SYSTEMD)
	return "linux-systemd";
#else
	return "posix";
#endif
}
