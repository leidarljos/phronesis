/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Linux seat host (optional): sd_notify READY/STOPPING + watchdog pings,
 * optional libcap clear when privileged. Signals still set a stop flag;
 * the Cap'n serve loop is nng-native (not sd_event-driven).
 *
 * Not linked into libgrok_policyd.so.
 */
#include "internal.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

#ifdef GROK_HAVE_LIBCAP
#include <sys/capability.h>
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
	(void)sd_notify(0, "READY=1\nSTATUS=grok-policyd Cap'n peer\n");
}

void grok_host_notify_stopping(void)
{
	(void)sd_notify(0, "STOPPING=1\n");
}

void grok_host_watchdog_ping(void)
{
	/* Harmless if WatchdogSec= is unset. */
	(void)sd_notify(0, "WATCHDOG=1\n");
}

int grok_host_drop_bounding_caps(void)
{
#ifdef GROK_HAVE_LIBCAP
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
#ifdef GROK_HAVE_LIBCAP
	return "linux-systemd+libcap";
#else
	return "linux-systemd";
#endif
}
