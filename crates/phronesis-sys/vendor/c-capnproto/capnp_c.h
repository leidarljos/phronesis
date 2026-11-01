/* vim: set sw=8 ts=8 sts=8 noet: */
/* capnp_c.h
 *
 * Copyright (C) 2013 James McKaskill
 * Copyright (C) 2014 Steve Dee
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef CAPNP_C_H
#define CAPNP_C_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/limits.h>
#ifndef UINT32_MAX
#define UINT32_MAX UINT_MAX
#endif
/* capn_init_fp is a userspace FILE* entry; keep the prototype valid. */
#define FILE void
#else /* !__KERNEL__ */
#include <stdint.h>
#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#if defined(unix) && !defined(__APPLE__) && !defined(__FreeBSD__)
#include <endian.h>
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#endif

/* ssize_t is a POSIX type, not an ISO C one...
 * Windows seems to only have SSIZE_T in BaseTsd.h
 */
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <stddef.h>
#endif
#endif /* __KERNEL__ */

/* Cross-platform macro ALIGNED_(x) aligns a struct or a field
 * by `x` bytes. When applied to a struct, it applies to the
 * aggregate but not the individual members of the struct. So
 * apply ALIGNED_(x) to individual members that need to be
 * aligned.
 * Confer: https://www.ibm.com/docs/en/i/7.1?topic=attributes-aligned-type-attribute
 */
#ifdef _MSC_VER
#define ALIGNED_(x) __declspec(align(x))
#endif
#ifdef __GNUC__
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif

/* Shared-library export. MSVC only writes an import library (capnp_c.lib)
 * when at least one symbol is __declspec(dllexport). Static builds
 * (CAPNP_C_STATIC) and non-Windows leave this empty. Define
 * CAPNP_C_BUILDING when compiling the library objects.
 */
#if defined(CAPNP_C_STATIC)
#define CAPN_EXPORT
#elif defined(_WIN32) || defined(__CYGWIN__)
#ifdef CAPNP_C_BUILDING
#define CAPN_EXPORT __declspec(dllexport)
#else
#define CAPN_EXPORT __declspec(dllimport)
#endif
#else
#define CAPN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define CAPN_INLINE static inline
#else
#define CAPN_INLINE static
#endif

#define CAPN_VERSION 1

/* struct capn is a common structure shared between segments in the same
 * session/context so that far pointers between segments will be created.
 *
 * lookup is used to lookup segments by id when derefencing a far pointer
 *
 * create is used to create or lookup an alternate segment that has at least
 * sz available (ie returned seg->len + sz <= seg->cap)
 *
 * create_local is used to create a segment for the copy tree and should be
 * allocated in the local memory space.
 *
 * Allocated segments must be zero initialized.
 *
 * create and lookup can be NULL if you don't need multiple segments and don't
 * want to support copying
 *
 * seglist and copylist are linked lists which can be used to free up segments
 * on cleanup, but should not be modified by the user.
 *
 * lookup, create, create_local, and user can be set by the user. Other values
 * should be zero initialized.
 */
struct capn {
	/* user settable */
	struct capn_segment *(*lookup)(void* /*user*/, uint32_t /*id */);
	struct capn_segment *(*create)(void* /*user*/, uint32_t /*id */, int /*sz*/);
	struct capn_segment *(*create_local)(void* /*user*/, int /*sz*/);
	void *user;
	/* 0 = 64MiB default. User may set after init, before decode. */
	size_t traversal_limit;
	/* 0 = CAPN_NESTING_DEFAULT (64). User may set after init, before decode. */
	int nesting_limit;
	/* zero initialized, user should not modify */
	uint32_t segnum;
	struct capn_tree *copy;
	struct capn_tree *segtree;
	struct capn_segment *seglist, *lastseg;
	struct capn_segment *copylist;
	size_t traversal_used;
	/* Sticky: set when read_ptr fails for OOB, budget, nesting, or a
	 * broken (non-null) pointer. Not set for a wire-null pointer word.
	 * Query with capn_ok; clear with capn_clear_err. */
	int decode_err;
};

/* Default decode traversal budget (bytes). 0 in traversal_limit means this. */
#define CAPN_TRAVERSAL_DEFAULT ((size_t) 64u * 1024u * 1024u)

/* Default pointer nesting depth. 0 in nesting_limit means this. */
#define CAPN_NESTING_DEFAULT 64

/* struct capn_tree is a rb tree header used internally for the segment id
 * lookup and copy tree */
struct capn_tree {
	struct capn_tree *parent, *link[2];
	unsigned int red : 1;
};

CAPN_EXPORT struct capn_tree *capn_tree_insert(struct capn_tree *root, struct capn_tree *n);

/* struct capn_segment contains the information about a single segment.
 *
 * capn points to a struct capn that is shared between segments in the
 * same session
 *
 * id specifies the segment id, used for far pointers
 *
 * data specifies the segment data. This should not move after creation.
 *
 * len specifies the current segment length. This is 0 for a blank
 * segment.
 *
 * cap specifies the segment capacity.
 *
 * When creating new structures len will be incremented until it reaches cap,
 * at which point a new segment will be requested via capn->create. The
 * create callback can either create a new segment or expand an existing
 * one by incrementing cap and returning the expanded segment.
 *
 * data, len, and cap must all be 8 byte aligned, hence the ALIGNED_(8) macro
 * on the struct fields.
 *
 * data, len, cap, and user should all be set by the user. Other values
 * should be zero initialized.
 */

struct ALIGNED_(8) capn_segment {
	struct capn_tree hdr;
	struct capn_segment *next;
	struct capn *capn;
	uint32_t id;
	/* user settable */
	ALIGNED_(8) char *data;
	ALIGNED_(8) size_t len;
	ALIGNED_(8) size_t cap;
	ALIGNED_(8) void *user;
};

enum CAPN_TYPE {
	CAPN_NULL = 0,
	CAPN_STRUCT = 1,
	CAPN_LIST = 2,
	CAPN_PTR_LIST = 3,
	CAPN_BIT_LIST = 4,
	CAPN_FAR_POINTER = 5,
	/* Capability pointer (encoding.html: A=3, B=0). len is the
	 * capability table index. There is no object body. */
	CAPN_INTERFACE = 6,
};

struct capn_ptr {
	unsigned int type : 4;
	unsigned int has_ptr_tag : 1;
	unsigned int is_list_member : 1;
	unsigned int is_composite_list : 1;
	unsigned int datasz : 19;
	unsigned int ptrs : 16;
	int len;
	char *data;
	struct capn_segment *seg;
	/* Decode remaining hops. Trailing so positional {type,...,seg}
	 * initializers in generated constants keep working. 0/0 = unset. */
	int nesting;
	unsigned int nesting_valid;
};

struct capn_text {
	int len;
	const char *str;
	struct capn_segment *seg;
};

typedef struct capn_ptr capn_ptr;
typedef struct capn_text capn_text;
typedef struct {capn_ptr p;} capn_data;
typedef struct {capn_ptr p;} capn_list1;
typedef struct {capn_ptr p;} capn_list8;
typedef struct {capn_ptr p;} capn_list16;
typedef struct {capn_ptr p;} capn_list32;
typedef struct {capn_ptr p;} capn_list64;
/* List(Text) / List(Data) / List(AnyPointer) / List(List(...)) wrapper so
 * capn_len(list) compiles. Bare capn_ptr lists use capn_ptr_len(p). */
typedef struct {capn_ptr p;} capn_ptr_list;

/* capnp_data_t is the decoded representation of a Data field.
 * Used by codec-generated encode/decode functions to represent
 * variable-length byte blobs in user structs.
 */
typedef struct {
	uint8_t *data;
	int len;
} capnp_data_t;

struct capn_msg {
	struct capn_segment *seg;
	uint64_t iface;
	uint16_t method;
	capn_ptr args;
};

/* capn_append_segment appends a segment to a session */
CAPN_EXPORT void capn_append_segment(struct capn*, struct capn_segment*);

CAPN_EXPORT capn_ptr capn_root(struct capn *c);
CAPN_EXPORT void capn_resolve(capn_ptr *p);

/* capn_validate walks the pointer graph from the message root.
 * Returns 0 if landings are in-segment, nesting stays within
 * nesting_limit (0 means CAPN_NESTING_DEFAULT), the traversal budget
 * is not exceeded, and the graph has no cycles. Returns -1 otherwise.
 * Does not consume traversal_used. A failed hop may set decode_err
 * (same as getp). Generated accessors still trust a capn_ptr that has
 * already been decoded and do not check capn_ok.
 */
CAPN_EXPORT int capn_validate(struct capn *c);

/* capn_ok is 1 if no decode error has been recorded on this session.
 * read_ptr sets decode_err on OOB, traversal-budget, nesting, or a
 * broken (non-null) pointer. A legitimate wire-null pointer word does
 * not. Generated getters still return CAPN_NULL for both cases; check
 * this flag to tell them apart. capn_init_mem does not walk the graph.
 * capn_ok(NULL) is 0.
 */
CAPN_EXPORT int capn_ok(const struct capn *c);
CAPN_EXPORT void capn_clear_err(struct capn *c);

/* capn_canonicalize walks src and writes a single-segment canonical
 * form into dst (encoding.html): no far pointers, no holes, preorder,
 * trailing zero data/pointer words truncated, empty struct B=-1.
 * Shared objects are duplicated. dst must be a fresh
 * capn_init_malloc session (segnum == 0). Unframed bytes are
 * dst->seglist->data[0 .. len). capn_write_mem wraps them as a
 * 1-segment stream. Returns 0 on success, -1 on a bad pointer
 * graph, cycle, budget overflow, or a dst that is not writable.
 */
CAPN_EXPORT int capn_canonicalize(struct capn *src, struct capn *dst);

/* capn_set_root sets the message root to p (capn_setp(capn_root(c), 0, p)).
 * new_* / write_* only fill a struct in a segment; the message is empty
 * until the root pointer is set. Empty messages are valid. */
CAPN_EXPORT int capn_set_root(struct capn *c, capn_ptr p);

#define capn_len(list) ((list).p.type == CAPN_FAR_POINTER ? (capn_resolve(&(list).p), (list).p.len) : (list).p.len)
#define capn_ptr_len(p) ((p).type == CAPN_FAR_POINTER ? (capn_resolve(&(p)), (p).len) : (p).len)

/* capn_getp|setp functions get/set ptrs in list/structs
 * off is the list index or pointer index in a struct
 * capn_setp will copy the data, create far pointers, etc if the target
 * is in a different segment/context.
 * Both of these will use/return inner pointers for composite lists.
 * For CAPN_BIT_LIST (List(Bool), wire C=1) off is a bit index.
 * capn_getp returns a 1-element inner bit-list member (is_list_member=1;
 * ptrs holds the bit-within-byte 0..7). capn_setp copies one bit (from
 * another bit list, a struct's first bit, or CAPN_NULL=0).
 * For CAPN_PTR_LIST (wire C=6): resolve=1 chases the element pointer
 * (List(Text) / List(Data) / List(AnyPointer)). resolve=0 returns an
 * inner 0-data/1-pointer struct so List(Struct) can upgrade a C=6 list
 * (get_Foo then read_Foo). A tgt of type CAPN_NULL, or with data == NULL,
 * is encoded as a null pointer. Zero-init C structs so optional pointer
 * fields stay unset.
 */
CAPN_EXPORT capn_ptr capn_getp(capn_ptr p, int off, int resolve);
CAPN_EXPORT int capn_setp(capn_ptr p, int off, capn_ptr tgt);

CAPN_EXPORT capn_text capn_get_text(capn_ptr p, int off, capn_text def);
CAPN_EXPORT capn_data capn_get_data(capn_ptr p, int off);
CAPN_EXPORT int capn_set_text(capn_ptr p, int off, capn_text tgt);
/* capn_set_data writes tgt.p (generated Data setters call this).
 * A CAPN_NULL tgt, or data == NULL, is a wire null (C++ hasFoo() is
 * false). A 0-length List(UInt8) from capn_new_list8(seg, 0) is empty
 * non-null Data (hasFoo() is true). Build non-empty Data with
 * capn_new_list8 + capn_setv8, then pass the list as capn_data. */
CAPN_EXPORT int capn_set_data(capn_ptr p, int off, capn_data tgt);

/* capn_get* functions get data from a list
 * The length of the list is given by p->size
 * off specifies how far into the list to start
 * sz indicates the number of elements to get
 * The function returns the number of elements read or -1 on an error.
 * off must be byte aligned for capn_getv1
 */
CAPN_EXPORT int capn_get1(capn_list1 p, int off);
CAPN_EXPORT uint8_t capn_get8(capn_list8 p, int off);
CAPN_EXPORT uint16_t capn_get16(capn_list16 p, int off);
CAPN_EXPORT uint32_t capn_get32(capn_list32 p, int off);
CAPN_EXPORT uint64_t capn_get64(capn_list64 p, int off);
CAPN_EXPORT int capn_getv1(capn_list1 p, int off, uint8_t *data, int sz);
CAPN_EXPORT int capn_getv8(capn_list8 p, int off, uint8_t *data, int sz);
CAPN_EXPORT int capn_getv16(capn_list16 p, int off, uint16_t *data, int sz);
CAPN_EXPORT int capn_getv32(capn_list32 p, int off, uint32_t *data, int sz);
CAPN_EXPORT int capn_getv64(capn_list64 p, int off, uint64_t *data, int sz);

/* capn_set* functions set data in a list
 * off specifies how far into the list to start
 * sz indicates the number of elements to write
 * The function returns the number of elemnts written or -1 on an error.
 * off must be byte aligned for capn_setv1
 */
CAPN_EXPORT int capn_set1(capn_list1 p, int off, int v);
CAPN_EXPORT int capn_set8(capn_list8 p, int off, uint8_t v);
CAPN_EXPORT int capn_set16(capn_list16 p, int off, uint16_t v);
CAPN_EXPORT int capn_set32(capn_list32 p, int off, uint32_t v);
CAPN_EXPORT int capn_set64(capn_list64 p, int off, uint64_t v);
CAPN_EXPORT int capn_setv1(capn_list1 p, int off, const uint8_t *data, int sz);
CAPN_EXPORT int capn_setv8(capn_list8 p, int off, const uint8_t *data, int sz);
CAPN_EXPORT int capn_setv16(capn_list16 p, int off, const uint16_t *data, int sz);
CAPN_EXPORT int capn_setv32(capn_list32 p, int off, const uint32_t *data, int sz);
CAPN_EXPORT int capn_setv64(capn_list64 p, int off, const uint64_t *data, int sz);

/* capn_new_* functions create a new object
 * datasz is in bytes, ptrs is # of pointers, sz is # of elements in the list
 * On an error a CAPN_NULL pointer is returned
 *
 * List encoding (capnproto.org/encoding.html, matching C++):
 *   - List(Bool): bit list, element size C=1. Use capn_new_list1.
 *     Bits pack little-endian (index 0 = LSB of byte 0). capn_get1/set1
 *     and capn_getp/setp index bits. List(Bool) cannot be upgraded to
 *     List(Struct); do not encode bools as a composite list.
 *   - List(Text), List(Data), List(AnyPointer): pointer list, element size C=6.
 *     Generated type is capn_ptr_list (has .p) so capn_len works.
 *     Use capn_new_ptr_list(seg, n). Then capn_set_text / capn_setp per index.
 *   - List(Struct): composite list, element size C=7. Use
 *     capn_new_struct_list(seg, n, struct_datasz_bytes, struct_ptrs).
 *     Always C=7 plus a tag word (B = n), including empty lists and
 *     0-pointer / 1-word / 0-word structs. Generated new_Foo_list
 *     calls this.
 *   - capn_new_list(seg, n, datasz, ptrs) is the size-based helper:
 *     composite when ptrs || datasz > 8, else a primitive list
 *     (C=0/2/3/4/5). List(Void) is capn_new_list(seg, n, 0, 0) (C=0).
 *     Do not use it for 1-word or empty struct lists.
 *   - capn_new_list(seg, n, 0, 1) is List of 0-data/1-pointer *structs*
 *     (composite), not List(Text). capn_set_text on that list returns -1;
 *     use capn_new_ptr_list for List(Text).
 *   - capn_new_struct(seg, 0, 0) is a real empty struct (A=0 B=-1
 *     C=D=0, 0xFFFFFFFC), not a null pointer. Null is CAPN_NULL or
 *     data == NULL (all-zero word).
 *   - capn_new_interface(seg, datasz, ptrs) is a capability pointer
 *     (A=3, B=0). datasz/ptrs are unused (ABI match with capn_new_struct);
 *     there is no object body. The table index is capn_ptr.len (0 on create).
 *     A null interface is CAPN_NULL, not index 0.
 * Text is List(UInt8) with a trailing NUL included in the wire element count.
 */
CAPN_EXPORT capn_ptr capn_new_string(struct capn_segment *seg, const char *str, ssize_t sz);
CAPN_EXPORT capn_ptr capn_new_struct(struct capn_segment *seg, int datasz, int ptrs);
CAPN_EXPORT capn_ptr capn_new_interface(struct capn_segment *seg, int datasz, int ptrs);
CAPN_EXPORT capn_ptr capn_new_ptr_list(struct capn_segment *seg, int sz);
/* Alias for List(Text) / List(Data) / List(AnyPointer) (wire C=6). */
#define capn_new_text_list(seg, sz) capn_new_ptr_list((seg), (sz))
CAPN_EXPORT capn_ptr capn_new_list(struct capn_segment *seg, int sz, int datasz, int ptrs);
CAPN_EXPORT capn_ptr capn_new_struct_list(struct capn_segment *seg, int sz, int datasz, int ptrs);
CAPN_EXPORT capn_list1 capn_new_list1(struct capn_segment *seg, int sz);
CAPN_EXPORT capn_list8 capn_new_list8(struct capn_segment *seg, int sz);
CAPN_EXPORT capn_list16 capn_new_list16(struct capn_segment *seg, int sz);
CAPN_EXPORT capn_list32 capn_new_list32(struct capn_segment *seg, int sz);
CAPN_EXPORT capn_list64 capn_new_list64(struct capn_segment *seg, int sz);

/* capn_read|write* functions read/write struct values
 * off is the offset into the structure in bytes
 * Rarely should these be called directly, instead use the generated code.
 * Data must be xored with the default value
 * These are inlined
 */
CAPN_INLINE uint8_t capn_read8(capn_ptr p, int off);
CAPN_INLINE uint16_t capn_read16(capn_ptr p, int off);
CAPN_INLINE uint32_t capn_read32(capn_ptr p, int off);
CAPN_INLINE uint64_t capn_read64(capn_ptr p, int off);
CAPN_INLINE int capn_write1(capn_ptr p, int off, int val);
CAPN_INLINE int capn_write8(capn_ptr p, int off, uint8_t val);
CAPN_INLINE int capn_write16(capn_ptr p, int off, uint16_t val);
CAPN_INLINE int capn_write32(capn_ptr p, int off, uint32_t val);
CAPN_INLINE int capn_write64(capn_ptr p, int off, uint64_t val);

/* capn_init_malloc inits the capn struct with a create function which
 * allocates segments on the heap using malloc.
 *
 * Each session's first write (e.g. capn_root) allocates at least one
 * 4096-byte segment. Override the floor when compiling capn-malloc.c
 * with -DCAPN_CREATE_MIN_SZ=<power of two>. For many small messages that
 * per-message malloc is the main cost: reuse one arena (one capn and its
 * segments) across messages, or skip the heap allocator and call
 * capn_init_mem / capn_append_segment on a caller buffer instead.
 *
 * capn_init_(fp|mem|fd) inits by reading segments from a FILE*, a memory
 * buffer, or a file descriptor (via a read callback) in serialized form
 * (optionally packed). It will then setup the create function ala
 * capn_init_malloc so that further segments can be created.
 *
 * capn_free frees all the segment headers and data created by the create
 * function setup by capn_init_*
 */
CAPN_EXPORT void capn_init_malloc(struct capn *c);
CAPN_EXPORT int capn_init_fp(struct capn *c, FILE *f, int packed);
CAPN_EXPORT int capn_init_mem(struct capn *c, const uint8_t *p, size_t sz, int packed);
/* capn_init_fd is the read-side pair of capn_write_fd: the caller supplies
 * a read callback (same shape as POSIX read). packed is the stream codec.
 * Returns 0 on success, -1 on a NULL callback or a framing/I/O error.
 * Under __KERNEL__ the symbol exists and returns -1 (use capn_init_mem).
 */
CAPN_EXPORT int capn_init_fd(struct capn *c, ssize_t (*read_fd)(int fd, void *p, size_t count), int fd, int packed);

/* capn_size() calculates the amount of memory required to serialise the given
 * Cap'n Proto structure in the unpacked format. It does NOT apply to packed
 * serialisation, as that may (in rare cases) actually become bigger than the
 * input. A buffer of this size can then be passed to capn_write_mem() without
 * fear of truncation (again, only in the unpacked case).
 */
CAPN_EXPORT int64_t capn_size(struct capn *c);

/* capn_write_(fp|mem) writes segments to the file/memory buffer in
 * serialized form and returns the number of bytes written.
 * capn_write_fp is a userspace FILE* entry; the prototype stays valid
 * under __KERNEL__ (FILE is void) and the implementation returns -1.
 */
CAPN_EXPORT int capn_write_fp(struct capn *c, FILE *f, int packed);
CAPN_EXPORT int capn_write_fd(struct capn *c, ssize_t (*write_fd)(int fd, const void *p, size_t count), int fd, int packed);
CAPN_EXPORT int64_t capn_write_mem(struct capn *c, uint8_t *p, size_t sz, int packed);

CAPN_EXPORT void capn_free(struct capn *c);
CAPN_EXPORT void capn_reset_copy(struct capn *c);

/* Inline functions */


CAPN_INLINE uint8_t capn_flip8(uint8_t v) {
	return v;
}
CAPN_INLINE uint16_t capn_flip16(uint16_t v) {
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
	return v;
#elif defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN) && \
      defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8
	return __builtin_bswap16(v);
#else
	union { uint16_t u; uint8_t v[2]; } s;
	s.v[0] = (uint8_t)v;
	s.v[1] = (uint8_t)(v>>8);
	return s.u;
#endif
}
CAPN_INLINE uint32_t capn_flip32(uint32_t v) {
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
	return v;
#elif defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN) && \
      defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8
	return __builtin_bswap32(v);
#else
	union { uint32_t u; uint8_t v[4]; } s;
	s.v[0] = (uint8_t)v;
	s.v[1] = (uint8_t)(v>>8);
	s.v[2] = (uint8_t)(v>>16);
	s.v[3] = (uint8_t)(v>>24);
	return s.u;
#endif
}
CAPN_INLINE uint64_t capn_flip64(uint64_t v) {
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
	return v;
#elif defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN) && \
      defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8
	return __builtin_bswap64(v);
#else
	union { uint64_t u; uint8_t v[8]; } s;
	s.v[0] = (uint8_t)v;
	s.v[1] = (uint8_t)(v>>8);
	s.v[2] = (uint8_t)(v>>16);
	s.v[3] = (uint8_t)(v>>24);
	s.v[4] = (uint8_t)(v>>32);
	s.v[5] = (uint8_t)(v>>40);
	s.v[6] = (uint8_t)(v>>48);
	s.v[7] = (uint8_t)(v>>56);
	return s.u;
#endif
}

CAPN_INLINE int capn_write1(capn_ptr p, int off, int val) {
	if (off >= p.datasz*8) {
		return -1;
	} else if (val) {
		uint8_t tmp = (uint8_t)(1 << (off & 7));
		((uint8_t*) p.data)[off >> 3] |= tmp;
		return 0;
	} else {
		uint8_t tmp = (uint8_t)(~(1 << (off & 7)));
		((uint8_t*) p.data)[off >> 3] &= tmp;
		return 0;
	}
}

CAPN_INLINE uint8_t capn_read8(capn_ptr p, int off) {
	return off+1 <= p.datasz ? capn_flip8(*(uint8_t*) (p.data+off)) : 0;
}
CAPN_INLINE int capn_write8(capn_ptr p, int off, uint8_t val) {
	if (off+1 <= p.datasz) {
		*(uint8_t*) (p.data+off) = capn_flip8(val);
		return 0;
	} else {
		return -1;
	}
}

CAPN_INLINE uint16_t capn_read16(capn_ptr p, int off) {
	return off+2 <= p.datasz ? capn_flip16(*(uint16_t*) (p.data+off)) : 0;
}
CAPN_INLINE int capn_write16(capn_ptr p, int off, uint16_t val) {
	if (off+2 <= p.datasz) {
		*(uint16_t*) (p.data+off) = capn_flip16(val);
		return 0;
	} else {
		return -1;
	}
}

CAPN_INLINE uint32_t capn_read32(capn_ptr p, int off) {
	return off+4 <= p.datasz ? capn_flip32(*(uint32_t*) (p.data+off)) : 0;
}
CAPN_INLINE int capn_write32(capn_ptr p, int off, uint32_t val) {
	if (off+4 <= p.datasz) {
		*(uint32_t*) (p.data+off) = capn_flip32(val);
		return 0;
	} else {
		return -1;
	}
}

CAPN_INLINE uint64_t capn_read64(capn_ptr p, int off) {
	return off+8 <= p.datasz ? capn_flip64(*(uint64_t*) (p.data+off)) : 0;
}
CAPN_INLINE int capn_write64(capn_ptr p, int off, uint64_t val) {
	if (off+8 <= p.datasz) {
		*(uint64_t*) (p.data+off) = capn_flip64(val);
		return 0;
	} else {
		return -1;
	}
}

#ifndef __KERNEL__
union capn_conv_f32 {
	uint32_t u;
	float f;
};

union capn_conv_f64 {
	uint64_t u;
	double f;
};

CAPN_INLINE float capn_to_f32(uint32_t v) {
	union capn_conv_f32 u;
	u.u = v;
	return u.f;
}
CAPN_INLINE double capn_to_f64(uint64_t v) {
	union capn_conv_f64 u;
	u.u = v;
	return u.f;
}
CAPN_INLINE uint32_t capn_from_f32(float v) {
	union capn_conv_f32 u;
	u.f = v;
	return u.u;
}
CAPN_INLINE uint64_t capn_from_f64(double v) {
	union capn_conv_f64 u;
	u.f = v;
	return u.u;
}
#endif /* !__KERNEL__ */

#ifdef __cplusplus
}
#endif

#endif
