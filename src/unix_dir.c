/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Create a single directory component.
 *
 * mkdir(2): created mode is (mode & ~umask & 0777).
 * umask(2): only bits in the umask are turned off from the mode argument.
 *
 * We temporarily set umask(0) so mkdir(path, mode) yields exactly mode&0777,
 * then restore the previous umask. If the path already exists as a directory,
 * leave its mode alone (never chmod shared parents like /tmp or $XDG_RUNTIME_DIR).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int grok_unix_mkdir_leaf(const char *path, mode_t mode)
{
	mode_t old;
	struct stat st;
	int rc;

	if (!path || !path[0])
		return GROK_ERR_INVAL;

	if (stat(path, &st) == 0) {
		if (!S_ISDIR(st.st_mode))
			return GROK_ERR_IO;
		/* Existing directory: do not chmod (Keel / unix path safety). */
		return GROK_OK;
	}
	if (errno != ENOENT)
		return GROK_ERR_IO;

	old = umask(0);
	rc = mkdir(path, mode & 0777);
	(void)umask(old);
	if (rc == 0)
		return GROK_OK;
	if (errno == EEXIST) {
		if (stat(path, &st) != 0)
			return GROK_ERR_IO;
		if (!S_ISDIR(st.st_mode))
			return GROK_ERR_IO;
		return GROK_OK;
	}
	return GROK_ERR_IO;
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
		free(dup);
		return GROK_OK;
	}
	*slash = '\0';
	rc = grok_unix_mkdir_leaf(dup, 0700);
	free(dup);
	return rc;
}
