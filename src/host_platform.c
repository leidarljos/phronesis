/* SPDX-License-Identifier: Apache-2.0 */
/* CLI only: libuv signals, systemd notify/watchdog, libcap drop. */
#include "internal.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/capability.h>
#include <systemd/sd-daemon.h>
#include <uv.h>

static uv_loop_t g_loop;
static uv_signal_t g_sigterm, g_sigint;
static uv_timer_t g_wd;
static int g_inited;
static int g_wd_on;
static volatile sig_atomic_t g_stop;

static void on_sig(uv_signal_t *h, int signum)
{
	(void)h;
	(void)signum;
	g_stop = 1;
}

static void on_wd(uv_timer_t *h)
{
	(void)h;
	(void)sd_notify(0, "WATCHDOG=1\n");
}

static void on_closed(uv_handle_t *h)
{
	(void)h;
}

int grok_host_init(void)
{
	uint64_t usec = 0;
	int rc;

	g_stop = 0;
	g_wd_on = 0;
	if (uv_loop_init(&g_loop) != 0)
		return GROK_ERR_IO;
	g_inited = 1;
	if (uv_signal_init(&g_loop, &g_sigterm) != 0 ||
	    uv_signal_init(&g_loop, &g_sigint) != 0)
		goto fail;
	if (uv_signal_start(&g_sigterm, on_sig, SIGTERM) != 0 ||
	    uv_signal_start(&g_sigint, on_sig, SIGINT) != 0)
		goto fail;
	if (sd_watchdog_enabled(0, &usec) > 0 && usec > 0) {
		uint64_t ms = usec / 2000ULL; /* half interval, ms */

		if (ms < 1)
			ms = 1;
		if (uv_timer_init(&g_loop, &g_wd) != 0)
			goto fail;
		g_wd_on = 1;
		if (uv_timer_start(&g_wd, on_wd, ms, ms) != 0)
			goto fail;
	}
	return GROK_OK;
fail:
	grok_host_fini();
	return GROK_ERR_IO;
}

int grok_host_should_stop(void)
{
	if (g_inited)
		(void)uv_run(&g_loop, UV_RUN_NOWAIT); /* dispatch uv_signal / timer */
	return g_stop ? 1 : 0;
}

void grok_host_fini(void)
{
	if (!g_inited)
		return;
	if (g_wd_on) {
		uv_timer_stop(&g_wd);
		if (!uv_is_closing((uv_handle_t *)&g_wd))
			uv_close((uv_handle_t *)&g_wd, on_closed);
	}
	uv_signal_stop(&g_sigterm);
	uv_signal_stop(&g_sigint);
	if (!uv_is_closing((uv_handle_t *)&g_sigterm))
		uv_close((uv_handle_t *)&g_sigterm, on_closed);
	if (!uv_is_closing((uv_handle_t *)&g_sigint))
		uv_close((uv_handle_t *)&g_sigint, on_closed);
	while (uv_loop_alive(&g_loop))
		(void)uv_run(&g_loop, UV_RUN_ONCE);
	(void)uv_loop_close(&g_loop);
	g_inited = g_wd_on = 0;
}

void grok_host_notify_ready(void)
{
	(void)sd_notify(0, "READY=1\nSTATUS=grok-policyd\n");
}

void grok_host_notify_stopping(void)
{
	(void)sd_notify(0, "STOPPING=1\n");
}

int grok_host_drop_bounding_caps(void)
{
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
}
