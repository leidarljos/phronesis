/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Cap'n peer accept loop — thin dispatch over Unix primitives (unix_sock).
 * suckless: no DIY listen/bind; admit mapping is pure and fail-closed.
 */
#define _GNU_SOURCE
#include "wire/serve.h"

#include "internal.h"
#include "wire/capnp_min.h"
#include "wire/frame.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Thin wrappers for tests / API stability. */
int grok_policyd_wire_ensure_socket_parent(const char *socket_path)
{
	return grok_unix_ensure_socket_parent(socket_path) == GROK_OK ? 0 : -1;
}

int grok_policyd_wire_map_admit_kind(const char *kind, const char **tool,
				     const char **action)
{
	if (!kind || !tool || !action)
		return -1;
	if (strcmp(kind, "seat") == 0) {
		*tool = "seat";
		*action = "publish_run";
		return 0;
	}
	/* empty or "model" / "agent" → product model/agent start plane */
	if (kind[0] == '\0' || strcmp(kind, "model") == 0 ||
	    strcmp(kind, "agent") == 0) {
		*tool = "model";
		*action = "start";
		return 0;
	}
	return -1;
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
		/*
		 * Map admit kinds onto policy_check tools. Fail-closed:
		 * unknown kind is not silent model/start ALLOW.
		 *
		 * kind=""|"model" → model/start (product model admit)
		 * kind="agent"    → model/start (product agent process start)
		 * kind="seat"     → seat/publish_run (Cap'n board publish)
		 */
		const char *tool = NULL;
		const char *action = NULL;
		const char *path = NULL;
		grok_policy_result_t pr;
		int rc;

		if (grok_policyd_wire_map_admit_kind(req->u.admit.kind, &tool,
						     &action) != 0) {
			fill_error(resp, GROK_ERR_INVAL,
				   "unknown admit.kind (fail-closed)");
			return 0;
		}
		if (strcmp(tool, "seat") == 0)
			path = req->u.admit.detail;

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

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	if (!grok_unix_peer_is_self(cfd)) {
		/* Fail-closed: tell peer why when possible. */
		fill_error(&resp, GROK_ERR_DENIED, "peercred uid mismatch");
		if (wire_encode_response(&resp, &out, &out_len) == 0) {
			(void)gkpp_write_frame(cfd, out, out_len);
			free(out);
			out = NULL;
		}
		return;
	}

	if (gkpp_read_frame(cfd, &body, &body_len) != 0) {
		fprintf(stderr, "policyd serve: bad frame\n");
		return;
	}

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
	int lfd = -1;
	int rc = 1;

	if (!sup || !socket_path || !socket_path[0])
		return 2;

	if (grok_unix_stream_listen(socket_path, 0600, &lfd) != GROK_OK) {
		perror("unix listen");
		return 1;
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
	close(lfd);
	(void)unlink(socket_path);
	return rc;
}
