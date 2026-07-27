/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include "wire/serve.h"

#include "wire/capnp_min.h"
#include "wire/frame.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int ensure_parent_0700(const char *socket_path)
{
	char *dup = strdup(socket_path);
	char *slash;
	int rc = 0;

	if (!dup)
		return -1;
	slash = strrchr(dup, '/');
	if (slash && slash != dup) {
		*slash = '\0';
		if (mkdir(dup, 0700) != 0 && errno != EEXIST) {
			rc = -1;
			goto out;
		}
		if (chmod(dup, 0700) != 0) {
			rc = -1;
			goto out;
		}
	}
out:
	free(dup);
	return rc;
}

static int peer_uid_ok(int fd)
{
	struct ucred cred;
	socklen_t len = sizeof(cred);

	memset(&cred, 0, sizeof(cred));
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
		return 0;
	return cred.uid == getuid();
}

static void fill_error(struct wire_response *r, int32_t code, const char *msg)
{
	r->kind = WIRE_RESP_ERROR;
	r->u.error.code = code;
	snprintf(r->u.error.message, sizeof(r->u.error.message), "%s",
		 msg ? msg : "error");
}

static int handle_request(grok_supervisor_t *sup, const char *socket_path,
			  const struct wire_request *req,
			  struct wire_response *resp)
{
	snprintf(resp->trace_id, sizeof(resp->trace_id), "%s", req->trace_id);

	if (req->op == WIRE_OP_STATUS) {
		resp->kind = WIRE_RESP_STATUS;
		snprintf(resp->u.status.version, sizeof(resp->u.status.version),
			 "%s", grok_policyd_version_string());
		resp->u.status.api_version = grok_policyd_api_version();
		snprintf(resp->u.status.state_dir,
			 sizeof(resp->u.status.state_dir), "%s",
			 grok_supervisor_state_dir(sup));
		snprintf(resp->u.status.runtime_dir,
			 sizeof(resp->u.status.runtime_dir), "%s",
			 grok_supervisor_runtime_dir(sup));
		snprintf(resp->u.status.socket, sizeof(resp->u.status.socket),
			 "%s", socket_path ? socket_path : "");
		resp->u.status.ready = 1;
		return 0;
	}

	if (req->op == WIRE_OP_CHECK) {
		grok_policy_result_t pr;
		int rc = grok_policy_check(sup, req->u.check.agent_id,
					   req->u.check.tool, req->u.check.action,
					   req->u.check.path[0] ? req->u.check.path
								: NULL,
					   &pr);
		if (rc != GROK_OK) {
			fill_error(resp, rc, "policy_check failed");
			return 0;
		}
		resp->kind = WIRE_RESP_CHECK;
		resp->u.decision.decision = (int)pr.decision;
		snprintf(resp->u.decision.reason, sizeof(resp->u.decision.reason),
			 "%s", pr.reason);
		snprintf(resp->u.decision.agent_id,
			 sizeof(resp->u.decision.agent_id), "%s",
			 req->u.check.agent_id);
		snprintf(resp->u.decision.tool, sizeof(resp->u.decision.tool),
			 "%s", req->u.check.tool);
		snprintf(resp->u.decision.action, sizeof(resp->u.decision.action),
			 "%s", req->u.check.action);
		return 0;
	}

	if (req->op == WIRE_OP_ADMIT) {
		/* Map admit kinds onto policy_check tools. */
		const char *tool = "model";
		const char *action = "start";
		const char *path = NULL;
		grok_policy_result_t pr;
		int rc;

		if (strcmp(req->u.admit.kind, "seat") == 0) {
			tool = "seat";
			action = "publish_run";
			path = req->u.admit.detail;
		} else if (strcmp(req->u.admit.kind, "model") == 0 ||
			   req->u.admit.kind[0] == '\0') {
			tool = "model";
			action = "start";
			path = NULL;
		} else if (strcmp(req->u.admit.kind, "agent") == 0) {
			tool = "seat";
			action = "publish_run";
			path = req->u.admit.detail;
		}

		rc = grok_policy_check(sup, req->u.admit.agent_id, tool, action,
				       path, &pr);
		if (rc != GROK_OK) {
			fill_error(resp, rc, "admit check failed");
			return 0;
		}
		resp->kind = WIRE_RESP_ADMIT;
		resp->u.decision.decision = (int)pr.decision;
		snprintf(resp->u.decision.reason, sizeof(resp->u.decision.reason),
			 "%s", pr.reason);
		snprintf(resp->u.decision.agent_id,
			 sizeof(resp->u.decision.agent_id), "%s",
			 req->u.admit.agent_id);
		snprintf(resp->u.decision.tool, sizeof(resp->u.decision.tool),
			 "%s", tool);
		snprintf(resp->u.decision.action, sizeof(resp->u.decision.action),
			 "%s", action);
		return 0;
	}

	if (req->op == WIRE_OP_AGENT_STATUS) {
		grok_agent_status_t st;
		int rc = grok_supervisor_status(sup, req->u.agent.agent_id, &st);
		if (rc != GROK_OK) {
			fill_error(resp, rc, "agent status failed");
			return 0;
		}
		resp->kind = WIRE_RESP_AGENT;
		snprintf(resp->u.agent.id, sizeof(resp->u.agent.id), "%s", st.id);
		resp->u.agent.state = (int)st.state;
		resp->u.agent.pid = (int32_t)st.pid;
		resp->u.agent.pgid = (int32_t)st.pgid;
		resp->u.agent.exit_status = st.exit_status;
		snprintf(resp->u.agent.mode, sizeof(resp->u.agent.mode), "%s",
			 st.mode);
		snprintf(resp->u.agent.workspace, sizeof(resp->u.agent.workspace),
			 "%s", st.workspace);
		resp->u.agent.has_cgroup = st.has_cgroup;
		return 0;
	}

	fill_error(resp, GROK_ERR_INVAL, "unknown op");
	return 0;
}

static void serve_one(grok_supervisor_t *sup, const char *socket_path, int cfd)
{
	uint8_t *body = NULL;
	size_t body_len = 0;
	struct wire_request req;
	struct wire_response resp;
	uint8_t *out = NULL;
	size_t out_len = 0;

	if (!peer_uid_ok(cfd)) {
		fprintf(stderr, "policyd serve: peercred denied\n");
		return;
	}

	if (gkpp_read_frame(cfd, &body, &body_len) != 0) {
		fprintf(stderr, "policyd serve: bad frame\n");
		return;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	if (wire_decode_request(body, body_len, &req) != 0) {
		fill_error(&resp, GROK_ERR_INVAL, "decode request failed");
	} else {
		handle_request(sup, socket_path, &req, &resp);
	}
	free(body);

	if (wire_encode_response(&resp, &out, &out_len) != 0) {
		fprintf(stderr, "policyd serve: encode failed\n");
		return;
	}
	if (gkpp_write_frame(cfd, out, out_len) != 0)
		fprintf(stderr, "policyd serve: write failed\n");
	free(out);
}

int grok_policyd_serve(grok_supervisor_t *sup, const char *socket_path)
{
	struct sockaddr_un addr;
	int lfd = -1;
	int rc = 1;

	if (!sup || !socket_path || !socket_path[0])
		return 2;
	if (strlen(socket_path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "socket path too long\n");
		return 2;
	}
	if (ensure_parent_0700(socket_path) != 0) {
		perror("mkdir socket parent");
		return 1;
	}
	unlink(socket_path);

	lfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (lfd < 0) {
		perror("socket");
		return 1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		goto out;
	}
	if (chmod(socket_path, 0600) != 0) {
		perror("chmod socket");
		goto out;
	}
	if (listen(lfd, 16) != 0) {
		perror("listen");
		goto out;
	}
	fprintf(stderr, "grok-policyd serve: listening on %s\n", socket_path);

	for (;;) {
		int cfd = accept4(lfd, NULL, NULL, SOCK_CLOEXEC);
		if (cfd < 0) {
			if (errno == EINTR)
				continue;
			perror("accept");
			break;
		}
		serve_one(sup, socket_path, cfd);
		close(cfd);
	}
	rc = 0;
out:
	if (lfd >= 0)
		close(lfd);
	unlink(socket_path);
	return rc;
}
