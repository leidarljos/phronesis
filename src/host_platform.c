/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/capability.h>
#include <systemd/sd-daemon.h>
#include <uv.h>

static uv_loop_t *g_loop;
static uv_signal_t g_sigterm;
static uv_signal_t g_sigint;
static volatile sig_atomic_t g_stop;

static void on_signal(uv_signal_t *handle, int signum)
{
	(void)handle;
	(void)signum;
	g_stop = 1;
	if (g_loop)
		uv_stop(g_loop);
}

int grok_host_init(void)
{
	int rc;

	g_stop = 0;
	g_loop = calloc(1, sizeof(*g_loop));
	if (!g_loop)
		return GROK_ERR_IO;
	rc = uv_loop_init(g_loop);
	if (rc != 0) {
		free(g_loop);
		g_loop = NULL;
		return GROK_ERR_IO;
	}
	uv_signal_init(g_loop, &g_sigterm);
	uv_signal_init(g_loop, &g_sigint);
	uv_signal_start(&g_sigterm, on_signal, SIGTERM);
	uv_signal_start(&g_sigint, on_signal, SIGINT);
	return GROK_OK;
}

void grok_host_fini(void)
{
	if (!g_loop)
		return;
	uv_signal_stop(&g_sigterm);
	uv_signal_stop(&g_sigint);
	uv_close((uv_handle_t *)&g_sigterm, NULL);
	uv_close((uv_handle_t *)&g_sigint, NULL);
	uv_run(g_loop, UV_RUN_DEFAULT);
	uv_loop_close(g_loop);
	free(g_loop);
	g_loop = NULL;
}

int grok_host_should_stop(void)
{
	if (g_loop)
		uv_run(g_loop, UV_RUN_NOWAIT);
	return g_stop ? 1 : 0;
}

void grok_host_notify_ready(void)
{
	(void)sd_notify(0, "READY=1\nSTATUS=grok-policyd nng Capn peer\n");
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
	if (cap_clear(caps) != 0) {
		cap_free(caps);
		return GROK_ERR_IO;
	}
	if (cap_set_proc(caps) != 0) {
		cap_free(caps);
		return GROK_ERR_IO;
	}
	cap_free(caps);
	return GROK_OK;
}
