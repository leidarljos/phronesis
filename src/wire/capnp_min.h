/* SPDX-License-Identifier: Apache-2.0 */
/**
 * App-level Cap'n peer request/response shapes.
 * Encoding is c-capnproto (schema/policy.capnp.c) in codec_c_capn.c.
 * Interoperable with capnp-rust stream serialize of policy.capnp.
 */
#ifndef GROK_POLICYD_WIRE_CAPNP_MIN_H
#define GROK_POLICYD_WIRE_CAPNP_MIN_H

#include <stddef.h>
#include <stdint.h>

enum {
	WIRE_OP_STATUS = 0,
	WIRE_OP_CHECK = 1,
	WIRE_OP_ADMIT = 2,
	WIRE_OP_AGENT_STATUS = 3,
	WIRE_OP_UNKNOWN = -1
};

enum {
	WIRE_DEC_DENY = 0,
	WIRE_DEC_ALLOW = 1,
	WIRE_DEC_PROMPT = 2
};

struct wire_check {
	char agent_id[64];
	char tool[64];
	char action[64];
	char path[512];
};

struct wire_admit {
	char agent_id[64];
	char kind[32];
	char detail[512];
};

struct wire_agent_query {
	char agent_id[64];
};

struct wire_request {
	int op; /* WIRE_OP_* */
	char trace_id[128];
	union {
		struct wire_check check;
		struct wire_admit admit;
		struct wire_agent_query agent;
	} u;
};

struct wire_status {
	char version[32];
	int32_t api_version;
	char state_dir[512];
	char runtime_dir[512];
	char socket[512];
	int ready;
};

struct wire_decision {
	int decision; /* WIRE_DEC_* */
	char reason[128];
	char agent_id[64];
	char tool[64];
	char action[64];
};

struct wire_agent_status {
	char id[64];
	int state; /* 0 stopped 1 running 2 failed */
	int32_t pid;
	int32_t pgid;
	int32_t exit_status;
	char mode[32];
	char workspace[512];
	int has_cgroup;
};

struct wire_error {
	int32_t code;
	char message[128];
};

enum {
	WIRE_RESP_STATUS = 0,
	WIRE_RESP_CHECK = 1,
	WIRE_RESP_ADMIT = 2,
	WIRE_RESP_AGENT = 3,
	WIRE_RESP_ERROR = 4
};

struct wire_response {
	int kind; /* WIRE_RESP_* */
	char trace_id[128];
	union {
		struct wire_status status;
		struct wire_decision decision;
		struct wire_agent_status agent;
		struct wire_error error;
	} u;
};

/** Decode Cap'n stream body into request. Returns 0 or -1. */
int wire_decode_request(const uint8_t *body, size_t body_len, struct wire_request *out);

/** Encode response as Cap'n stream body. *out must be freed by caller. */
int wire_encode_response(const struct wire_response *resp, uint8_t **out, size_t *out_len);

#endif
