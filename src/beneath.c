/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Open a path only under an already-open root directory.
 * Pack load consumes the fd. checkPath does not use this.
 */
#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#ifndef SYS_openat2
#define SYS_openat2 437
#endif
#ifndef RESOLVE_NO_MAGICLINKS
#define RESOLVE_NO_MAGICLINKS 0x02u
#endif
#ifndef RESOLVE_NO_SYMLINKS
#define RESOLVE_NO_SYMLINKS 0x04u
#endif
#ifndef RESOLVE_BENEATH
#define RESOLVE_BENEATH 0x08u
#endif
struct grok_open_how {
	uint64_t flags;
	uint64_t mode;
	uint64_t resolve;
};
#endif

static int has_dotdot(const char *p)
{
	const char *c;

	if (!p)
		return 1;
	for (c = p; *c; c++) {
		if (c[0] == '.' && c[1] == '.' &&
		    (c[2] == '/' || c[2] == '\0') &&
		    (c == p || c[-1] == '/'))
			return 1;
	}
	return 0;
}

static int is_fs_root(const char *root)
{
	const char *p;

	if (!root || root[0] != '/')
		return 0;
	for (p = root; *p; p++) {
		if (*p == '/')
			continue;
		if (p[0] == '.' && (p[1] == '/' || p[1] == '\0'))
			continue;
		return 0;
	}
	return 1;
}

static int bad_root(const char *root)
{
	if (!root || root[0] != '/' || is_fs_root(root))
		return 1;
	return has_dotdot(root);
}

int grok_beneath_dir(const char *root, int *outfd)
{
	int fd;

	if (!outfd || bad_root(root))
		return -1;
	*outfd = -1;
	fd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	*outfd = fd;
	return 0;
}

int grok_beneath_rel(const char *root, const char *path, char *rel, size_t n)
{
	size_t rl;

	if (!root || !path || !rel || n == 0)
		return -1;
	if (path[0] != '/') {
		if (path[0] == '\0' || has_dotdot(path) || strlen(path) >= n)
			return -1;
		memcpy(rel, path, strlen(path) + 1);
		return 0;
	}
	if (bad_root(root))
		return -1;
	rl = strlen(root);
	while (rl > 1 && root[rl - 1] == '/')
		rl--;
	if (strncmp(path, root, rl) != 0)
		return -1;
	if (path[rl] == '\0') {
		if (n < 2)
			return -1;
		rel[0] = '.';
		rel[1] = '\0';
		return 0;
	}
	if (path[rl] != '/')
		return -1;
	if (strlen(path + rl + 1) >= n)
		return -1;
	if (has_dotdot(path + rl + 1))
		return -1;
	memcpy(rel, path + rl + 1, strlen(path + rl + 1) + 1);
	return 0;
}

static int walk_open(int rootfd, const char *rel, int flags, int *outfd)
{
	char buf[PHRONESIS_PATH_MAX];
	char *save = NULL;
	char *tok;
	int cur;
	int next;

	if (!rel || !rel[0] || !outfd || strlen(rel) >= sizeof(buf))
		return -1;
	if (strcmp(rel, ".") == 0) {
		next = fcntl(rootfd, F_DUPFD_CLOEXEC, 0);
		if (next < 0)
			return -1;
		*outfd = next;
		return 0;
	}
	memcpy(buf, rel, strlen(rel) + 1);
	cur = fcntl(rootfd, F_DUPFD_CLOEXEC, 0);
	if (cur < 0)
		return -1;
	tok = strtok_r(buf, "/", &save);
	while (tok) {
		char *nxt = strtok_r(NULL, "/", &save);
		int last = (nxt == NULL);
		int of = last ? (flags | O_NOFOLLOW | O_CLOEXEC)
			      : (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

		if (tok[0] && strcmp(tok, ".") != 0) {
			if (strcmp(tok, "..") == 0) {
				close(cur);
				return -1;
			}
			next = openat(cur, tok, of);
			close(cur);
			if (next < 0)
				return -1;
			cur = next;
		}
		tok = nxt;
	}
	*outfd = cur;
	return 0;
}

int grok_beneath_open(int rootfd, const char *rel, int flags, int *outfd)
{
	if (rootfd < 0 || !rel || !rel[0] || rel[0] == '/' || !outfd)
		return -1;
	*outfd = -1;
#ifdef __linux__
	{
		struct grok_open_how how;
		long fd;

		memset(&how, 0, sizeof(how));
		how.flags = (uint64_t)(unsigned)(flags | O_CLOEXEC);
		how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS |
			      RESOLVE_NO_MAGICLINKS;
		fd = syscall(SYS_openat2, rootfd, rel, &how, sizeof(how));
		if (fd >= 0) {
			*outfd = (int)fd;
			return 0;
		}
		if (errno != ENOSYS)
			return -1;
	}
#endif
	return walk_open(rootfd, rel, flags, outfd);
}
