/* SPDX-License-Identifier: Apache-2.0 */
#include "wire/capnp_min.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- arena builder (single segment) ---- */

struct arena {
	uint8_t *data;
	size_t words; /* allocated words */
	size_t used;  /* used words */
};

static int arena_init(struct arena *a, size_t words)
{
	a->data = calloc(words, 8);
	if (!a->data)
		return -1;
	a->words = words;
	a->used = 0;
	return 0;
}

static void arena_free(struct arena *a)
{
	free(a->data);
	a->data = NULL;
	a->words = a->used = 0;
}

static int arena_grow(struct arena *a, size_t need_words)
{
	size_t nw;
	uint8_t *p;

	if (a->used + need_words <= a->words)
		return 0;
	nw = a->words * 2;
	while (a->used + need_words > nw)
		nw *= 2;
	p = realloc(a->data, nw * 8);
	if (!p)
		return -1;
	memset(p + a->words * 8, 0, (nw - a->words) * 8);
	a->data = p;
	a->words = nw;
	return 0;
}

static int64_t arena_alloc_words(struct arena *a, size_t n)
{
	int64_t off;

	if (arena_grow(a, n) != 0)
		return -1;
	off = (int64_t)a->used;
	a->used += n;
	return off;
}

static uint64_t *word_at(struct arena *a, size_t wi)
{
	return (uint64_t *)(a->data + wi * 8);
}

static uint64_t make_struct_ptr(int32_t offset_words, uint16_t data_words,
				uint16_t ptr_words)
{
	uint64_t p = 0;
	/* bits 0-1 = 0 (struct) */
	p |= ((uint64_t)(uint32_t)offset_words & 0x3fffffffu) << 2;
	p |= ((uint64_t)data_words) << 32;
	p |= ((uint64_t)ptr_words) << 48;
	return p;
}

static uint64_t make_list_ptr_bytes(int32_t offset_words, uint32_t byte_count)
{
	uint64_t p = 1; /* list */
	p |= ((uint64_t)(uint32_t)offset_words & 0x3fffffffu) << 2;
	p |= (2ull << 32); /* element size: byte */
	p |= ((uint64_t)byte_count) << 35;
	return p;
}

static int set_text(struct arena *a, size_t ptr_word_index, const char *s)
{
	size_t len;
	size_t words;
	int64_t data_off;
	int32_t rel;
	uint8_t *dst;

	if (!s)
		s = "";
	len = strlen(s) + 1; /* NUL included in Cap'n text */
	words = (len + 7) / 8;
	data_off = arena_alloc_words(a, words);
	if (data_off < 0)
		return -1;
	dst = a->data + (size_t)data_off * 8;
	memcpy(dst, s, len);
	/* relative offset: from end of pointer word to start of list data */
	rel = (int32_t)(data_off - (int64_t)ptr_word_index - 1);
	*word_at(a, ptr_word_index) = make_list_ptr_bytes(rel, (uint32_t)len);
	return 0;
}

static int encode_stream(const struct arena *a, uint8_t **out, size_t *out_len)
{
	/* single segment: [0][seg0_words] then segment words */
	size_t seg_words = a->used;
	size_t hdr_u32 = 2; /* count-1=0, size */
	size_t pad_u32 = (hdr_u32 % 2) ? 0 : 0; /* 2 is even → need pad? */
	/* nseg=1 → write 1+(1) = 2 uint32s. 2 is even → pad one zero u32 → 3 u32s = not word aligned?
	 * Spec: if the number of segment size words including the count is odd, pad.
	 * Values written: (segCount-1), size0. That's 2 values — even — so NO pad when even?
	 * From capnp serialize: "if segment count is even, pad" — actually:
	 * total 32-bit words in table = 1 + segmentCount. If that is odd, pad.
	 * 1+1=2 even → no pad. */
	size_t table_bytes = 8; /* two u32 */
	size_t total;
	uint8_t *buf;
	uint32_t *t;

	(void)pad_u32;
	total = table_bytes + seg_words * 8;
	buf = malloc(total);
	if (!buf)
		return -1;
	t = (uint32_t *)buf;
	t[0] = 0; /* segmentCount - 1 */
	t[1] = (uint32_t)seg_words;
	memcpy(buf + table_bytes, a->data, seg_words * 8);
	*out = buf;
	*out_len = total;
	return 0;
}

int wire_encode_response(const struct wire_response *resp, uint8_t **out,
			 size_t *out_len)
{
	struct arena a;
	int64_t root_data;
	size_t root_ptr0; /* traceId */
	size_t root_ptr1; /* body */
	int64_t resp_data;
	size_t resp_ptr0;
	uint16_t tag;
	int rc = -1;

	if (!resp || !out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	if (arena_init(&a, 64) != 0)
		return -1;

	/* Word 0: root struct pointer → data at word 1
	 * PolicyEnvelope: 1 data word (8 bytes), 2 pointers
	 * Layout: root pointer at 0, then struct data+ptrs contiguous is common pattern.
	 */
	/* Allocate root struct: 1 data + 2 ptr = 3 words, place at word 1 */
	if (arena_alloc_words(&a, 1) != 0) /* reserve word 0 for root ptr */
		goto fail;
	root_data = arena_alloc_words(&a, 1 + 2);
	if (root_data < 0)
		goto fail;
	/* root pointer at 0: offset from end of ptr (word 1) to data (root_data) = root_data - 1 */
	*word_at(&a, 0) = make_struct_ptr((int32_t)(root_data - 1), 1, 2);

	/* data word: protocolVersion=1 at bits 0-31, body union tag at bits 32-48 = 1 (response) */
	*word_at(&a, (size_t)root_data) = 1ull | (1ull << 32);
	root_ptr0 = (size_t)root_data + 1;
	root_ptr1 = (size_t)root_data + 2;
	if (set_text(&a, root_ptr0, resp->trace_id) != 0)
		goto fail;

	/* PolicyResponse: 1 data word, 1 pointer */
	resp_data = arena_alloc_words(&a, 1 + 1);
	if (resp_data < 0)
		goto fail;
	*word_at(&a, root_ptr1) =
		make_struct_ptr((int32_t)(resp_data - (int64_t)root_ptr1 - 1), 1, 1);
	resp_ptr0 = (size_t)resp_data + 1;
	tag = (uint16_t)resp->kind;
	*word_at(&a, (size_t)resp_data) = (uint64_t)tag;

	if (resp->kind == WIRE_RESP_STATUS) {
		/* PolicydStatus: 1 data, 4 ptrs */
		int64_t st = arena_alloc_words(&a, 1 + 4);
		const struct wire_status *s = &resp->u.status;
		uint64_t dw;

		if (st < 0)
			goto fail;
		*word_at(&a, resp_ptr0) =
			make_struct_ptr((int32_t)(st - (int64_t)resp_ptr0 - 1), 1, 4);
		dw = (uint64_t)(uint32_t)s->api_version;
		if (s->ready)
			dw |= (1ull << 32);
		*word_at(&a, (size_t)st) = dw;
		if (set_text(&a, (size_t)st + 1, s->version) != 0 ||
		    set_text(&a, (size_t)st + 2, s->state_dir) != 0 ||
		    set_text(&a, (size_t)st + 3, s->runtime_dir) != 0 ||
		    set_text(&a, (size_t)st + 4, s->socket) != 0)
			goto fail;
	} else if (resp->kind == WIRE_RESP_CHECK || resp->kind == WIRE_RESP_ADMIT) {
		/* PolicyDecision: 1 data, 4 ptrs */
		int64_t d = arena_alloc_words(&a, 1 + 4);
		const struct wire_decision *dec = &resp->u.decision;

		if (d < 0)
			goto fail;
		*word_at(&a, resp_ptr0) =
			make_struct_ptr((int32_t)(d - (int64_t)resp_ptr0 - 1), 1, 4);
		*word_at(&a, (size_t)d) = (uint64_t)(uint16_t)dec->decision;
		if (set_text(&a, (size_t)d + 1, dec->reason) != 0 ||
		    set_text(&a, (size_t)d + 2, dec->agent_id) != 0 ||
		    set_text(&a, (size_t)d + 3, dec->tool) != 0 ||
		    set_text(&a, (size_t)d + 4, dec->action) != 0)
			goto fail;
	} else if (resp->kind == WIRE_RESP_AGENT) {
		int64_t ag = arena_alloc_words(&a, 2 + 3);
		const struct wire_agent_status *s = &resp->u.agent;
		uint64_t *w0, *w1;

		if (ag < 0)
			goto fail;
		*word_at(&a, resp_ptr0) =
			make_struct_ptr((int32_t)(ag - (int64_t)resp_ptr0 - 1), 2, 3);
		w0 = word_at(&a, (size_t)ag);
		w1 = word_at(&a, (size_t)ag + 1);
		/* state u16 @0, pid i32 @32, pgid i32 @64, exit @96 */
		*w0 = (uint64_t)(uint16_t)s->state |
		      ((uint64_t)(uint32_t)s->pid << 32);
		*w1 = (uint64_t)(uint32_t)s->pgid |
		      ((uint64_t)(uint32_t)s->exit_status << 32);
		if (s->has_cgroup)
			*w1 |= 0; /* hasCgroup is bit in data — bits after exitStatus */
		/* AgentStatusWire: hasCgroup @7 Bool — after 16-byte data fields.
		 * data section is 16 bytes = 2 words:
		 * word0: state@0-16, pid@32-64
		 * word1: pgid@64-96, exit@96-128
		 * hasCgroup needs more data words — schema said 16 bytes data.
		 * Bool @7 at bits — Cap'n places bool after int32s.
		 * From compile: 16 bytes data, hasCgroup @7 Bool — field ordinal 7.
		 * Offsets: id ptr0, state bits[0,16), pid[32,64), pgid[64,96),
		 * exit[96,128), mode ptr1, workspace ptr2, hasCgroup bits — need check.
		 * Annotated: hasCgroup @7 :Bool — with 16 bytes data, bool may be at bit 128
		 * requiring 3rd data word. Schema said "# 16 bytes, 3 ptrs" — so bool is in
		 * the 16 bytes. Possible location: bits[16,17) next to state, or packing.
		 * Cap'n packs bools into holes: state uses 16 bits, hole at 16-32 for bools.
		 */
		*w0 |= ((uint64_t)(s->has_cgroup ? 1 : 0)) << 16;
		if (set_text(&a, (size_t)ag + 2, s->id) != 0 ||
		    set_text(&a, (size_t)ag + 3, s->mode) != 0 ||
		    set_text(&a, (size_t)ag + 4, s->workspace) != 0)
			goto fail;
	} else { /* ERROR */
		int64_t er = arena_alloc_words(&a, 1 + 1);
		const struct wire_error *e = &resp->u.error;

		if (er < 0)
			goto fail;
		*word_at(&a, resp_ptr0) =
			make_struct_ptr((int32_t)(er - (int64_t)resp_ptr0 - 1), 1, 1);
		*word_at(&a, (size_t)er) = (uint64_t)(uint32_t)e->code;
		if (set_text(&a, (size_t)er + 1, e->message) != 0)
			goto fail;
	}

	if (encode_stream(&a, out, out_len) != 0)
		goto fail;
	rc = 0;
fail:
	arena_free(&a);
	return rc;
}

/* ---- reader ---- */

struct seg {
	const uint8_t *data;
	size_t words;
};

static int parse_stream(const uint8_t *body, size_t body_len, struct seg *seg0)
{
	uint32_t seg_count_m1;
	uint32_t seg0_words;
	size_t table_u32;
	size_t table_bytes;
	size_t need;

	if (!body || body_len < 8)
		return -1;
	memcpy(&seg_count_m1, body, 4);
	if (seg_count_m1 != 0)
		return -1; /* multi-segment not needed for our messages */
	memcpy(&seg0_words, body + 4, 4);
	table_u32 = 1 + 1; /* count-1 + one size */
	if (table_u32 % 2 == 1)
		table_u32++; /* pad */
	/* 1+1=2 even → no pad per: pad when (1+nseg) is odd */
	table_bytes = 8;
	need = table_bytes + (size_t)seg0_words * 8;
	if (body_len < need)
		return -1;
	seg0->data = body + table_bytes;
	seg0->words = seg0_words;
	return 0;
}

static uint64_t load_word(const struct seg *s, size_t wi)
{
	uint64_t w;

	if (wi >= s->words)
		return 0;
	memcpy(&w, s->data + wi * 8, 8);
	return w;
}

static int follow_struct(const struct seg *s, size_t ptr_wi, size_t *data_wi,
			 uint16_t *data_words, uint16_t *ptr_words)
{
	uint64_t p = load_word(s, ptr_wi);
	int32_t offset;
	uint16_t dw, pw;

	if ((p & 3ull) != 0)
		return -1; /* not struct (null or other) */
	if (p == 0)
		return -1;
	offset = (int32_t)((p >> 2) & 0x3fffffff);
	/* sign extend 30-bit */
	if (offset & 0x20000000)
		offset |= (int32_t)0xc0000000;
	dw = (uint16_t)((p >> 32) & 0xffff);
	pw = (uint16_t)((p >> 48) & 0xffff);
	*data_wi = ptr_wi + 1 + (size_t)offset;
	*data_words = dw;
	*ptr_words = pw;
	if (*data_wi + dw + pw > s->words)
		return -1;
	return 0;
}

static int read_text(const struct seg *s, size_t ptr_wi, char *out, size_t out_len)
{
	uint64_t p = load_word(s, ptr_wi);
	int32_t offset;
	uint32_t count;
	size_t data_wi;
	size_t n;

	if (out_len == 0)
		return -1;
	out[0] = '\0';
	if (p == 0)
		return 0;
	if ((p & 3ull) != 1)
		return -1;
	offset = (int32_t)((p >> 2) & 0x3fffffff);
	if (offset & 0x20000000)
		offset |= (int32_t)0xc0000000;
	if (((p >> 32) & 7ull) != 2)
		return -1; /* not byte list */
	count = (uint32_t)(p >> 35);
	data_wi = ptr_wi + 1 + (size_t)offset;
	if (count == 0)
		return 0;
	n = count;
	if (n > 0)
		n -= 1; /* strip NUL from Cap'n text count */
	if (n >= out_len)
		n = out_len - 1;
	if (data_wi * 8 + n > s->words * 8)
		return -1;
	memcpy(out, s->data + data_wi * 8, n);
	out[n] = '\0';
	return 0;
}

int wire_decode_request(const uint8_t *body, size_t body_len,
			struct wire_request *out)
{
	struct seg seg;
	size_t root_data, req_data, inner_data;
	uint16_t dw, pw;
	uint64_t data0;
	uint16_t body_tag, op_tag;
	size_t root_ptr0, root_ptr1, req_ptr0;

	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->op = WIRE_OP_UNKNOWN;
	if (parse_stream(body, body_len, &seg) != 0)
		return -1;
	/* root pointer at word 0 */
	if (follow_struct(&seg, 0, &root_data, &dw, &pw) != 0)
		return -1;
	if (dw < 1 || pw < 2)
		return -1;
	data0 = load_word(&seg, root_data);
	/* protocolVersion in low 32 — accept any for now */
	body_tag = (uint16_t)((data0 >> 32) & 0xffff);
	if (body_tag != 0)
		return -1; /* expect request */
	root_ptr0 = root_data + dw; /* first pointer — but pointers follow data words */
	/* Cap'n: pointers section starts at data_wi + data_words */
	root_ptr0 = root_data + dw;
	root_ptr1 = root_ptr0 + 1;
	if (read_text(&seg, root_ptr0, out->trace_id, sizeof(out->trace_id)) != 0)
		return -1;
	if (follow_struct(&seg, root_ptr1, &req_data, &dw, &pw) != 0)
		return -1;
	if (dw < 1 || pw < 1)
		return -1;
	op_tag = (uint16_t)(load_word(&seg, req_data) & 0xffff);
	req_ptr0 = req_data + dw;
	out->op = (int)op_tag;

	if (op_tag == WIRE_OP_STATUS)
		return 0;
	if (op_tag == WIRE_OP_CHECK) {
		if (follow_struct(&seg, req_ptr0, &inner_data, &dw, &pw) != 0)
			return -1;
		if (pw < 4)
			return -1;
		if (read_text(&seg, inner_data + dw + 0, out->u.check.agent_id,
			      sizeof(out->u.check.agent_id)) != 0 ||
		    read_text(&seg, inner_data + dw + 1, out->u.check.tool,
			      sizeof(out->u.check.tool)) != 0 ||
		    read_text(&seg, inner_data + dw + 2, out->u.check.action,
			      sizeof(out->u.check.action)) != 0 ||
		    read_text(&seg, inner_data + dw + 3, out->u.check.path,
			      sizeof(out->u.check.path)) != 0)
			return -1;
		return 0;
	}
	if (op_tag == WIRE_OP_ADMIT) {
		if (follow_struct(&seg, req_ptr0, &inner_data, &dw, &pw) != 0)
			return -1;
		if (pw < 3)
			return -1;
		if (read_text(&seg, inner_data + dw + 0, out->u.admit.agent_id,
			      sizeof(out->u.admit.agent_id)) != 0 ||
		    read_text(&seg, inner_data + dw + 1, out->u.admit.kind,
			      sizeof(out->u.admit.kind)) != 0 ||
		    read_text(&seg, inner_data + dw + 2, out->u.admit.detail,
			      sizeof(out->u.admit.detail)) != 0)
			return -1;
		return 0;
	}
	if (op_tag == WIRE_OP_AGENT_STATUS) {
		if (follow_struct(&seg, req_ptr0, &inner_data, &dw, &pw) != 0)
			return -1;
		if (pw < 1)
			return -1;
		if (read_text(&seg, inner_data + dw + 0, out->u.agent.agent_id,
			      sizeof(out->u.agent.agent_id)) != 0)
			return -1;
		return 0;
	}
	return -1;
}
