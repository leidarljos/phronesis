/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int g_failures;
int g_tests;

void t_expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "  FAIL: %s\n", msg);
		g_failures++;
	}
}

void t_expect_eq(long a, long b, const char *msg)
{
	if (a != b) {
		fprintf(stderr, "  FAIL: %s (got %ld want %ld)\n", msg, a, b);
		g_failures++;
	}
}

void t_expect_streq(const char *a, const char *b, const char *msg)
{
	if (!a)
		a = "";
	if (!b)
		b = "";
	if (strcmp(a, b) != 0) {
		fprintf(stderr, "  FAIL: %s (got \"%s\" want \"%s\")\n", msg, a, b);
		g_failures++;
	}
}

int t_tmpdir(char *buf, size_t n, const char *prefix)
{
	const char *base = getenv("TMPDIR");
	char tmpl[GROK_PATH_MAX];
	int r;

	if (!base || !base[0])
		base = "/tmp";
	r = snprintf(tmpl, sizeof(tmpl), "%s/%s.XXXXXX", base, prefix);
	if (r < 0 || (size_t)r >= sizeof(tmpl))
		return -1;
	if (!mkdtemp(tmpl))
		return -1;
	if (snprintf(buf, n, "%s", tmpl) >= (int)n)
		return -1;
	return 0;
}

int t_write_file(const char *path, const char *body)
{
	FILE *f = fopen(path, "w");

	if (!f)
		return -1;
	if (fputs(body, f) < 0) {
		fclose(f);
		return -1;
	}
	if (fclose(f) != 0)
		return -1;
	return 0;
}

int t_wait_file(const char *path, int timeout_ms)
{
	int waited = 0;

	while (waited < timeout_ms) {
		if (access(path, F_OK) == 0)
			return 0;
		usleep(10 * 1000);
		waited += 10;
	}
	return -1;
}

int t_pid_alive(pid_t pid)
{
	if (pid <= 0)
		return 0;
	return kill(pid, 0) == 0 || errno != ESRCH;
}

int t_read_pidfile(const char *path, pid_t *out)
{
	FILE *f;
	int v;

	f = fopen(path, "r");
	if (!f)
		return -1;
	if (fscanf(f, "%d", &v) != 1) {
		fclose(f);
		return -1;
	}
	fclose(f);
	*out = (pid_t)v;
	return 0;
}

static void rm_tree(const char *path)
{
	DIR *d;
	struct dirent *e;
	char child[GROK_PATH_MAX];
	struct stat st;

	if (lstat(path, &st) != 0)
		return;
	if (!S_ISDIR(st.st_mode)) {
		unlink(path);
		return;
	}
	d = opendir(path);
	if (!d)
		return;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		if (snprintf(child, sizeof(child), "%s/%s", path, e->d_name) >= (int)sizeof(child))
			continue;
		rm_tree(child);
	}
	closedir(d);
	rmdir(path);
}

void t_rm_rf(const char *path)
{
	if (path && path[0])
		rm_tree(path);
}

int t_open_pair(grok_supervisor_t **out, char *state, size_t sn,
		char *runtime, size_t rn, const char *tag)
{
	char pfx_s[64], pfx_r[64];

	snprintf(pfx_s, sizeof(pfx_s), "gp-%s-st", tag);
	snprintf(pfx_r, sizeof(pfx_r), "gp-%s-rt", tag);
	if (t_tmpdir(state, sn, pfx_s) != 0)
		return -1;
	if (t_tmpdir(runtime, rn, pfx_r) != 0)
		return -1;
	return grok_supervisor_open(out, state, runtime);
}

int t_run(const char *name, t_fn fn)
{
	int before = g_failures;

	g_tests++;
	fprintf(stderr, "test: %s\n", name);
	fn();
	if (g_failures == before)
		fprintf(stderr, "  ok\n");
	return g_failures == before ? 0 : -1;
}
