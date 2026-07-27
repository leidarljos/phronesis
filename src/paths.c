/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <bsd/string.h>
#include <cwalk.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int join_path(char *out, size_t n, const char *base, const char *child)
{
	size_t w;

	if (!out || n == 0 || !base)
		return GROK_ERR_INVAL;
	if (!child || !child[0]) {
		if (strlcpy(out, base, n) >= n)
			return GROK_ERR_INVAL;
		return GROK_OK;
	}
	w = cwk_path_join(base, child, out, n);
	if (w >= n)
		return GROK_ERR_INVAL;
	return GROK_OK;
}

int grok_paths_ensure_dir(const char *path, int mode)
{
	return grok_unix_mkdir_leaf(path, (mode_t)mode);
}

static int ensure_state_tree(const char *state)
{
	char buf[GROK_PATH_MAX];

	if (grok_paths_ensure_dir(state, 0700) != GROK_OK)
		return GROK_ERR_IO;
	if (join_path(buf, sizeof(buf), state, "log") != GROK_OK)
		return GROK_ERR_INVAL;
	if (grok_paths_ensure_dir(buf, 0700) != GROK_OK)
		return GROK_ERR_IO;
	if (join_path(buf, sizeof(buf), state, "policyd") != GROK_OK)
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
		if (strlcpy(state_dir, state_override, state_len) >= state_len)
			return GROK_ERR_INVAL;
	} else if (env_state && env_state[0]) {
		if (strlcpy(state_dir, env_state, state_len) >= state_len)
			return GROK_ERR_INVAL;
	} else if (xdg_state && xdg_state[0]) {
		if (join_path(state_dir, state_len, xdg_state, "grokos") != GROK_OK)
			return GROK_ERR_INVAL;
	} else if (home && home[0]) {
		if (join_path(tmp, sizeof(tmp), home, ".local/state") != GROK_OK)
			return GROK_ERR_INVAL;
		if (join_path(state_dir, state_len, tmp, "grokos") != GROK_OK)
			return GROK_ERR_INVAL;
	} else {
		return GROK_ERR_STATE;
	}

	if (runtime_override && runtime_override[0]) {
		if (strlcpy(runtime_dir, runtime_override, runtime_len) >= runtime_len)
			return GROK_ERR_INVAL;
	} else if (env_runtime && env_runtime[0]) {
		if (strlcpy(runtime_dir, env_runtime, runtime_len) >= runtime_len)
			return GROK_ERR_INVAL;
	} else if (xdg_runtime && xdg_runtime[0]) {
		if (join_path(runtime_dir, runtime_len, xdg_runtime, "grokos") != GROK_OK)
			return GROK_ERR_INVAL;
	} else {
		if (join_path(runtime_dir, runtime_len, state_dir, "run") != GROK_OK)
			return GROK_ERR_INVAL;
	}

	if (strcmp(runtime_dir, "/tmp") == 0 || strcmp(runtime_dir, "/tmp/") == 0)
		return GROK_ERR_STATE;

	if (env_log && env_log[0]) {
		if (strlcpy(action_log, env_log, log_len) >= log_len)
			return GROK_ERR_INVAL;
	} else {
		if (join_path(tmp, sizeof(tmp), state_dir, "log") != GROK_OK)
			return GROK_ERR_INVAL;
		if (join_path(action_log, log_len, tmp, "actions.jsonl") != GROK_OK)
			return GROK_ERR_INVAL;
	}

	if (ensure_state_tree(state_dir) != GROK_OK)
		return GROK_ERR_IO;
	if (grok_paths_ensure_dir(runtime_dir, 0700) != GROK_OK)
		return GROK_ERR_IO;
	return GROK_OK;
}
