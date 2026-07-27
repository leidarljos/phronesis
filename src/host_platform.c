/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CLI/daemon host glue only — not linked into libgrok_policyd.so.
 * Signal handlers: only async-signal-safe work (set flag). Seacord/Effective C.
 * Serve loop polls should_stop; nng RECVTIMEO + no SA_RESTART so recv returns.
 * libuv loop reserved for future process watches (UV_RUN_NOWAIT in should_stop).
 * libsystemd: sd_notify. libcap: optional priv drop when privileged.
 */
#include "internal.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/capability.h>
#include <systemd/sd-daemon.h>
#include <uv.h>

static uv_loop_t *g_loop;
static volatile sig_atomic_t g_stop;

/* Async-signal-safe: only assign sig_atomic_t. */
static void on_signal(int signum)
{
	(void)signum;
	g_stop = 1;
}

int grok_host_init(void)
{
	struct sigaction sa;
	int rc;

	g_stop = 0;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	/* Do not set SA_RESTART: blocking nng recv must return so serve can poll. */
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return GROK_ERR_IO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
		return GROK_ERR_IO;

	g_loop = calloc(1, sizeof(*g_loop));
	if (!g_loop)
		return GROK_ERR_IO;
	rc = uv_loop_init(g_loop);
	if (rc != 0) {
		free(g_loop);
		g_loop = NULL;
		return GROK_ERR_IO;
	}
	return GROK_OK;
}

void grok_host_fini(void)
{
	if (!g_loop)
		return;
	uv_loop_close(g_loop);
	free(g_loop);
	g_loop = NULL;
}

int grok_host_should_stop(void)
{
	if (g_loop)
		(void)uv_run(g_loop, UV_RUN_NOWAIT);
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
