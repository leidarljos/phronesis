/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Join up to three path components with '/'. Empty/null trailing parts omitted. */
static int join3(char *out, size_t n, const char *a, const char *b, const char *c)
{
	int r;

	if (!out || n == 0 || !a)
		return GROK_ERR_INVAL;
	if (c && c[0])
		r = snprintf(out, n, "%s/%s/%s", a, b ? b : "", c);
	else if (b && b[0])
		r = snprintf(out, n, "%s/%s", a, b);
	else
		r = snprintf(out, n, "%s", a);
	if (r < 0 || (size_t)r >= n)
		return GROK_ERR_INVAL;
	return GROK_OK;
}

/*
 * Create one directory leaf. Temporarily umask(0) so mode is exact (tests
 * expect 0700). Existing dirs are left alone (no chmod of shared parents).
 */
static int mkdir_one(const char *path, int mode)
{
	mode_t old;
	struct stat st;
	int rc;

	if (!path || !path[0])
		return GROK_ERR_INVAL;
	if (stat(path, &st) == 0) {
		if (!S_ISDIR(st.st_mode))
			return GROK_ERR_IO;
		return GROK_OK;
	}
	if (errno != ENOENT)
		return GROK_ERR_IO;
	old = umask(0);
	rc = mkdir(path, (mode_t)mode & 0777);
	(void)umask(old);
	if (rc == 0)
		return GROK_OK;
	if (errno == EEXIST) {
		if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
			return GROK_ERR_IO;
		return GROK_OK;
	}
	return GROK_ERR_IO;
}

/*
 * Create `path` and any missing parents. New levels get `mode`. Existing
 * parents are not chmod'd (shared ~/.local stays as the host left it).
 */
int grok_paths_ensure_dir(const char *path, int mode)
{
	char buf[GROK_PATH_MAX];
	size_t i, n;
	int rc;

	if (!path || !path[0])
		return GROK_ERR_INVAL;
	n = strlen(path);
	if (n >= sizeof(buf))
		return GROK_ERR_INVAL;
	memcpy(buf, path, n + 1);
	for (i = 1; i < n; i++) {
		if (buf[i] != '/')
			continue;
		buf[i] = '\0';
		rc = mkdir_one(buf, mode);
		buf[i] = '/';
		if (rc != GROK_OK)
			return rc;
		while (i + 1 < n && buf[i + 1] == '/')
			i++;
	}
	return mkdir_one(buf, mode);
}

static int ensure_state_tree(const char *state)
{
	char buf[GROK_PATH_MAX];

	if (grok_paths_ensure_dir(state, 0700) != GROK_OK)
		return GROK_ERR_IO;
	if (join3(buf, sizeof(buf), state, "log", NULL) != GROK_OK)
		return GROK_ERR_INVAL;
	if (grok_paths_ensure_dir(buf, 0700) != GROK_OK)
		return GROK_ERR_IO;
	if (join3(buf, sizeof(buf), state, "policyd", NULL) != GROK_OK)
		return GROK_ERR_INVAL;
	if (grok_paths_ensure_dir(buf, 0700) != GROK_OK)
		return GROK_ERR_IO;
	return GROK_OK;
}

int grok_paths_resolve(char *state_dir, size_t state_len,
		       char *runtime_dir, size_t runtime_len,
		       char *action_log, size_t log_len,
		       const char *state_override,
		       const char *runtime_override)
{
	const char *env_state = getenv("GROKOS_STATE_DIR");
	const char *env_runtime = getenv("GROKOS_RUNTIME_DIR");
	const char *env_log = getenv("GROKOS_ACTION_LOG");
	const char *xdg_state = getenv("XDG_STATE_HOME");
	const char *xdg_runtime = getenv("XDG_RUNTIME_DIR");
	const char *home = getenv("HOME");
	char tmp[GROK_PATH_MAX];

	if (state_override && state_override[0]) {
		if (snprintf(state_dir, state_len, "%s", state_override) >= (int)state_len)
			return GROK_ERR_INVAL;
	} else if (env_state && env_state[0]) {
		if (snprintf(state_dir, state_len, "%s", env_state) >= (int)state_len)
			return GROK_ERR_INVAL;
	} else if (xdg_state && xdg_state[0]) {
		if (join3(state_dir, state_len, xdg_state, "grokos", NULL) != GROK_OK)
			return GROK_ERR_INVAL;
	} else if (home && home[0]) {
		if (join3(tmp, sizeof(tmp), home, ".local/state", NULL) != GROK_OK)
			return GROK_ERR_INVAL;
		if (join3(state_dir, state_len, tmp, "grokos", NULL) != GROK_OK)
			return GROK_ERR_INVAL;
	} else {
		return GROK_ERR_STATE;
	}

	if (runtime_override && runtime_override[0]) {
		if (snprintf(runtime_dir, runtime_len, "%s", runtime_override) >= (int)runtime_len)
			return GROK_ERR_INVAL;
	} else if (env_runtime && env_runtime[0]) {
		if (snprintf(runtime_dir, runtime_len, "%s", env_runtime) >= (int)runtime_len)
			return GROK_ERR_INVAL;
	} else if (xdg_runtime && xdg_runtime[0]) {
		if (join3(runtime_dir, runtime_len, xdg_runtime, "grokos", NULL) != GROK_OK)
			return GROK_ERR_INVAL;
	} else {
		if (join3(runtime_dir, runtime_len, state_dir, "run", NULL) != GROK_OK)
			return GROK_ERR_INVAL;
	}

	if (strcmp(runtime_dir, "/tmp") == 0 || strcmp(runtime_dir, "/tmp/") == 0)
		return GROK_ERR_STATE;

	if (env_log && env_log[0]) {
		if (snprintf(action_log, log_len, "%s", env_log) >= (int)log_len)
			return GROK_ERR_INVAL;
	} else {
		if (join3(action_log, log_len, state_dir, "log", "actions.jsonl") != GROK_OK)
			return GROK_ERR_INVAL;
	}

	if (ensure_state_tree(state_dir) != GROK_OK)
		return GROK_ERR_IO;
	if (grok_paths_ensure_dir(runtime_dir, 0700) != GROK_OK)
		return GROK_ERR_IO;
	return GROK_OK;
}
