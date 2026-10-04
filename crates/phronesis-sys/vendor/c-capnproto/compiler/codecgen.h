/* codecgen.h - encode/decode/free generators for $C.codecgen
 *
 * Adapted from shen390s/c-capnproto compiler/codegen_codec.h.
 * Thin adapter: points at capnpc-c.c process globals. Does not own
 * a struct capn (g_valcapn stays process-lifetime).
 */

#ifndef CAPNPC_CODECGEN_H
#define CAPNPC_CODECGEN_H

#include "schema.capnp.h"
#include "str.h"

#define ANNOTATION_CODECGEN 0xcccaac86283e2609UL
#define ANNOTATION_MAPNAME 0xb9edf6fc2d8972b8UL
#define ANNOTATION_MAPLISTCOUNT 0xb6ea49eb8a9b0f9eUL
#define ANNOTATION_MAPUNIONTAG 0xdce06d41858f91acUL

struct value {
	struct Type t;
	const char *tname;
	struct str tname_buf;
	struct Value v;
	capn_ptr ptrval;
	int64_t intval;
};

struct field {
	struct Field f;
	struct value v;
	struct node *group;
};

struct node {
	struct capn_tree hdr;
	struct Node n;
	struct node *next;
	struct node *file_nodes, *next_file_node;
	struct str name;
	struct field *fields;
};

typedef struct {
	struct str *HDR;
	struct str *SRC;
	int *g_nullused;
	int *g_val0used;
	int *g_codecgen;
	int *g_fieldgetset;
} capnp_ctx_t;

struct node *find_node(uint64_t id);

const char *get_mapname(Annotation_list l);
const char *get_maplistcount(Annotation_list l);
const char *get_mapuniontag(Annotation_list l);

void encode_member(capnp_ctx_t *ctx, struct str *func, struct field *f,
		   const char *tab, const char *var, const char *var2);
void decode_member(capnp_ctx_t *ctx, struct str *func, struct field *f,
		   const char *tab, const char *var, const char *var2);
void free_member(capnp_ctx_t *ctx, struct str *func, struct field *f,
		 const char *tab, const char *var, const char *var2);
void mk_struct_list_encoder(capnp_ctx_t *ctx, struct node *n);
void mk_struct_ptr_encoder(capnp_ctx_t *ctx, struct node *n);
void mk_struct_list_decoder(capnp_ctx_t *ctx, struct node *n);
void mk_struct_ptr_decoder(capnp_ctx_t *ctx, struct node *n);
void mk_struct_list_free(capnp_ctx_t *ctx, struct node *n);
void mk_struct_ptr_free(capnp_ctx_t *ctx, struct node *n);
void declare_codec(capnp_ctx_t *ctx, struct node *file_node);

#endif /* CAPNPC_CODECGEN_H */
