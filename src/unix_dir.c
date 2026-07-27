/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Leaf directory creation with Unix primitives only.
 * suckless: mkdir + open(O_DIRECTORY) + fchmod on create; never chmod existing.
 */
#define _GNU_SOURCE
#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int grok_unix_mkdir_leaf(const char *path, mode_t mode)
{
	int dfd;
	struct stat st;

	if (!path || !path[0])
		return GROK_ERR_INVAL;

	if (mkdir(path, mode) == 0) {
		/* Created: pin mode via fchmod (survives umask). */
		dfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (dfd < 0)
			return GROK_ERR_IO;
		if (fchmod(dfd, mode) != 0) {
			close(dfd);
			return GROK_ERR_IO;
		}
		close(dfd);
		return GROK_OK;
	}
	if (errno != EEXIST)
		return GROK_ERR_IO;
	/* Existing: verify directory, do not chmod (shared parents). */
	if (stat(path, &st) != 0)
		return GROK_ERR_IO;
	if (!S_ISDIR(st.st_mode))
		return GROK_ERR_IO;
	return GROK_OK;
}

int grok_unix_ensure_socket_parent(const char *socket_path)
{
	char *dup;
	char *slash;
	int rc;

	if (!socket_path || !socket_path[0])
		return GROK_ERR_INVAL;
	dup = strdup(socket_path);
	if (!dup)
		return GROK_ERR_IO;
	slash = strrchr(dup, '/');
	if (!slash || slash == dup) {
		/* No parent component (cwd socket) — nothing to create. */
		free(dup);
		return GROK_OK;
	}
	*slash = '\0';
	rc = grok_unix_mkdir_leaf(dup, 0700);
	free(dup);
	return rc;
}
