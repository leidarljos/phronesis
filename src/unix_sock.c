/* SPDX-License-Identifier: Apache-2.0 */
/*
 * AF_UNIX stream listen + peercred — OS primitives only.
 * suckless: socket/bind/listen/accept4 + SO_PEERCRED; small and explicit.
 */
#define _GNU_SOURCE
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

int grok_unix_peer_uid(int fd, uid_t *uid_out)
{
	struct ucred cred;
	socklen_t len = sizeof(cred);

	if (fd < 0 || !uid_out)
		return GROK_ERR_INVAL;
	memset(&cred, 0, sizeof(cred));
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
		return GROK_ERR_IO;
	*uid_out = cred.uid;
	return GROK_OK;
}

int grok_unix_peer_is_self(int fd)
{
	uid_t peer;

	if (grok_unix_peer_uid(fd, &peer) != GROK_OK)
		return 0;
	return peer == getuid();
}

int grok_unix_stream_listen(const char *socket_path, mode_t sock_mode, int *listen_fd)
{
	struct sockaddr_un addr;
	int lfd = -1;
	int rc = GROK_ERR_IO;

	if (!socket_path || !socket_path[0] || !listen_fd)
		return GROK_ERR_INVAL;
	if (strlen(socket_path) >= sizeof(addr.sun_path))
		return GROK_ERR_INVAL;

	if (grok_unix_ensure_socket_parent(socket_path) != GROK_OK)
		return GROK_ERR_IO;

	/* Replace stale socket inode. */
	(void)unlink(socket_path);

	lfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (lfd < 0)
		return GROK_ERR_IO;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		goto out;
	/*
	 * Mode the socket inode. Prefer fchmod on the bound fd (Linux);
	 * fall back to path chmod.
	 */
	if (fchmod(lfd, sock_mode) != 0) {
		if (chmod(socket_path, sock_mode) != 0)
			goto out;
	}
	if (listen(lfd, 16) != 0)
		goto out;

	*listen_fd = lfd;
	return GROK_OK;

out:
	if (lfd >= 0) {
		close(lfd);
		(void)unlink(socket_path);
	}
	return rc;
}
