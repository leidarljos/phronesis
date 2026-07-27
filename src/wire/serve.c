/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Cap'n peer serve loop over nng req/rep IPC.
 * Payload: Cap'n body only (no GKPP stream header). ACL: NNG_OPT_PEER_UID.
 */
#define _GNU_SOURCE
#include "wire/serve.h"

#include "internal.h"
#include "wire/capnp_min.h"
#include "wire/frame.h"

#include <errno.h>
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int peer_uid_ok(nng_msg *msg)
{
	nng_pipe p = nng_msg_get_pipe(msg);
	uint64_t uid = UINT64_MAX;
	/* nng 1.x: PEER_UID is uint64 on the pipe (not int). */
	int rc = nng_pipe_get_uint64(p, NNG_OPT_PEER_UID, &uid);

	if (rc != 0)
		return 0;
	return uid <= (uint64_t)UINT32_MAX && (uid_t)uid == getuid();
}

static void serve_one_msg(grok_supervisor_t *sup, const char *socket_path,
			  nng_socket sock, nng_msg *msg)
{
	struct wire_request req;
	struct wire_response resp;
	uint8_t *out = NULL;
	size_t out_len = 0;
	void *body;
	size_t body_len;
	nng_msg *rmsg = NULL;
	int rc;

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	if (!peer_uid_ok(msg)) {
		fill_error(&resp, GROK_ERR_DENIED, "peercred uid mismatch");
	} else {
		body = nng_msg_body(msg);
		body_len = nng_msg_len(msg);
		if (!body || body_len == 0) {
			fill_error(&resp, GROK_ERR_INVAL, "empty body");
		} else if (wire_decode_request(body, body_len, &req) != 0) {
			fill_error(&resp, GROK_ERR_INVAL, "decode request failed");
		} else {
			handle_request(sup, socket_path, &req, &resp);
		}
	}

	if (wire_encode_response(&resp, &out, &out_len) != 0) {
		fprintf(stderr, "policyd serve: encode failed\n");
		return;
	}
	rc = nng_msg_alloc(&rmsg, 0);
	if (rc != 0) {
		free(out);
		fprintf(stderr, "policyd serve: msg_alloc %d\n", rc);
		return;
	}
	rc = nng_msg_append(rmsg, out, out_len);
	free(out);
	if (rc != 0) {
		nng_msg_free(rmsg);
		fprintf(stderr, "policyd serve: msg_append %d\n", rc);
		return;
	}
	rc = nng_sendmsg(sock, rmsg, 0);
	if (rc != 0) {
		nng_msg_free(rmsg);
		fprintf(stderr, "policyd serve: sendmsg %d\n", rc);
	}
}

int grok_policyd_serve(grok_supervisor_t *sup, const char *socket_path)
{
	nng_socket sock = NNG_SOCKET_INITIALIZER;
	char url[sizeof("ipc://") + 512];
	int rc;
	nng_listener lis = NNG_LISTENER_INITIALIZER;

	if (!sup || !socket_path || !socket_path[0])
		return 2;

	if (grok_unix_ensure_socket_parent(socket_path) != GROK_OK) {
		perror("socket parent");
		return 1;
	}
	(void)unlink(socket_path);

	if (socket_path[0] != '/') {
		fprintf(stderr, "policyd serve: socket path must be absolute\n");
		return 1;
	}
	if (snprintf(url, sizeof(url), "ipc://%s", socket_path) >= (int)sizeof(url)) {
		fprintf(stderr, "policyd serve: path too long\n");
		return 1;
	}

	rc = nng_rep0_open(&sock);
	if (rc != 0) {
		fprintf(stderr, "policyd serve: rep0_open %s\n", nng_strerror(rc));
		return 1;
	}
	/* Create listener so we can set IPC mode 0600 before start. */
	rc = nng_listener_create(&lis, sock, url);
	if (rc != 0) {
		fprintf(stderr, "policyd serve: listener_create %s\n",
			nng_strerror(rc));
		nng_close(sock);
		return 1;
	}
	(void)nng_listener_set_int(lis, NNG_OPT_IPC_PERMISSIONS, 0600);
	(void)nng_socket_set_ms(sock, NNG_OPT_RECVTIMEO, 500);
	rc = nng_listener_start(lis, 0);
	if (rc != 0) {
		fprintf(stderr, "policyd serve: listener_start %s\n",
			nng_strerror(rc));
		nng_listener_close(lis);
		nng_close(sock);
		return 1;
	}
	/* Path chmod as belt-and-suspenders when the FS object exists. */
	(void)chmod(socket_path, 0600);
	fprintf(stderr, "grok-policyd serve: nng rep on %s\n", socket_path);

	for (;;) {
		nng_msg *msg = NULL;
		rc = nng_recvmsg(sock, &msg, 0);
		if (rc == NNG_ETIMEDOUT)
			continue;
		if (rc != 0) {
			fprintf(stderr, "policyd serve: recv %s\n", nng_strerror(rc));
			if (rc == NNG_ECLOSED)
				break;
			continue;
		}
		serve_one_msg(sup, socket_path, sock, msg);
		nng_msg_free(msg);
	}

	nng_close(sock);
	(void)unlink(socket_path);
	return 0;
}
