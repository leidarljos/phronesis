/* vim: set sw=8 ts=8 sts=8 noet: */
/* capn.c
 *
 * Copyright (C) 2013 James McKaskill
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "capnp_c.h"

#ifndef __KERNEL__
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifndef _MSC_VER
#include <sys/param.h>
#endif
#define capn_alloc(n) malloc(n)
#define capn_zalloc(n) calloc(1, (n))
#define capn_freemem(p) free(p)
#else /* __KERNEL__ */
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/limits.h>
#define capn_alloc(n) kmalloc((n), GFP_KERNEL)
#define capn_zalloc(n) kzalloc((n), GFP_KERNEL)
#define capn_freemem(p) kfree(p)
#endif /* __KERNEL__ */

#define STRUCT_PTR 0
#define LIST_PTR 1
#define FAR_PTR 2
#define OTHER_PTR 3
#define DOUBLE_PTR 6

#define VOID_LIST 0
#define BIT_1_LIST 1
#define BYTE_1_LIST 2
#define BYTE_2_LIST 3
#define BYTE_4_LIST 4
#define BYTE_8_LIST 5
#define PTR_LIST 6
#define COMPOSITE_LIST 7

#define U64(val) ((uint64_t) (val))
#define I64(val) ((int64_t) (val))
#define U32(val) ((uint32_t) (val))
#define I32(val) ((int32_t) (val))
#define U16(val) ((uint16_t) (val))
#define I16(val) ((int16_t) (val))

#ifndef min
static int min(int a, int b) { return (a < b) ? a : b; }
#endif

#ifdef BYTE_ORDER
#define CAPN_LITTLE (BYTE_ORDER == LITTLE_ENDIAN)
#elif defined(__BYTE_ORDER)
#define CAPN_LITTLE (__BYTE_ORDER == __LITTLE_ENDIAN)
#else
#define CAPN_LITTLE 0
#endif

struct capn_tree *capn_tree_insert(struct capn_tree *root, struct capn_tree *n) {
	n->red = 1;
	n->link[0] = n->link[1] = NULL;

	for (;;) {
		/* parent, uncle, grandparent, great grandparent link */
		struct capn_tree *p, *u, *g, **gglink;
		int dir;

		/* Case 1: N is root */
		p = n->parent;
		if (!p) {
			n->red = 0;
			root = n;
			break;
		}

		/* Case 2: p is black */
		if (!p->red) {
			break;
		}

		g = p->parent;
		dir = (p == g->link[1]);

		/* Case 3: P and U are red, switch g to red, but must
		 * loop as G could be root or have a red parent
		 *     g    to   G
		 *    / \       / \
		 *   P   U     p   u
		 *  /         /
		 * N         N
		 */
		u = g->link[!dir];
		if (u != NULL && u->red) {
			p->red = 0;
			u->red = 0;
			g->red = 1;
			n = g;
			continue;
		}

		if (!g->parent) {
			gglink = &root;
		} else if (g->parent->link[1] == g) {
			gglink = &g->parent->link[1];
		} else {
			gglink = &g->parent->link[0];
		}

		if (dir != (n == p->link[1])) {
			/* Case 4: rotate on P, then on g
			 * here dir is /
			 *     g    to   g   to   n
			 *    / \       / \      / \
			 *   P   u     N   u    P   G
			 *  / \       / \      /|  / \
			 * 1   N     P   3    1 2 3   u
			 *    / \   / \
			 *   2   3 1   2
			 */
			struct capn_tree *two = n->link[dir];
			struct capn_tree *three = n->link[!dir];
			p->link[!dir] = two;
			g->link[dir] = three;
			n->link[dir] = p;
			n->link[!dir] = g;
			*gglink = n;
			n->parent = g->parent;
			p->parent = n;
			g->parent = n;
			if (two)
				two->parent = p;
			if (three)
				three->parent = g;
			n->red = 0;
			g->red = 1;
		} else {
			/* Case 5: rotate on g
			 * here dir is /
			 *       g   to   p
			 *      / \      / \
			 *     P   u    N   G
			 *    / \      /|  / \
			 *   N   3    1 2 3   u
			 *  / \
			 * 1   2
			 */
			struct capn_tree *three = p->link[!dir];
			g->link[dir] = three;
			p->link[!dir] = g;
			*gglink = p;
			p->parent = g->parent;
			g->parent = p;
			if (three)
				three->parent = g;
			g->red = 1;
			p->red = 0;
		}

		break;
	}

	return root;
}

void capn_append_segment(struct capn *c, struct capn_segment *s) {
	s->id = c->segnum++;
	s->capn = c;
	s->next = NULL;

	if (c->lastseg) {
		c->lastseg->next = s;
		c->lastseg->hdr.link[1] = &s->hdr;
		s->hdr.parent = &c->lastseg->hdr;
	} else {
		c->seglist = s;
		s->hdr.parent = NULL;
	}

	c->lastseg = s;
	c->segtree = capn_tree_insert(c->segtree, &s->hdr);
}

static char *new_data(struct capn *c, int sz, struct capn_segment **ps) {
	struct capn_segment *s;

	/* find a segment with sufficient data */
	for (s = c->seglist; s != NULL; s = s->next) {
		if (s->len + sz <= s->cap) {
			goto end;
		}
	}

	s = c->create ? c->create(c->user, c->segnum, sz) : NULL;
	if (!s) {
		*ps = NULL;
		return NULL;
	}

	capn_append_segment(c, s);
end:
	*ps = s;
	s->len += sz;
	return s->data + s->len - sz;
}

static struct capn_segment *lookup_segment(struct capn* c, struct capn_segment *s, uint32_t id) {
	struct capn_tree **x;
	struct capn_segment *y = NULL;

	if (s && s->id == id)
		return s;
	if (!c)
		return NULL;

	if (id < c->segnum) {
		x = &c->segtree;
		while (*x) {
			y = (struct capn_segment*) *x;
			if (id == y->id) {
				return y;
			} else if (id < y->id) {
				x = &y->hdr.link[0];
			} else {
				x = &y->hdr.link[1];
			}
		}
	} else {
		/* Otherwise `x` may be uninitialized */
		return NULL;
	}

	s = c->lookup ? c->lookup(c->user, id) : NULL;
	if (!s)
		return NULL;

	if (id < c->segnum) {
		s->id = id;
		s->capn = c;
		s->next = c->seglist;
		c->seglist = s;
		s->hdr.parent = &y->hdr;
		*x = &s->hdr;
		c->segtree = capn_tree_insert(c->segtree, &s->hdr);
	} else {
		c->segnum = id;
		capn_append_segment(c, s);
	}

	return s;
}

/* p in [s->data, s->data+s->len] and p+bytes does not exceed the
 * segment. Overflow-safe: no pointer wrap, no size_t wrap. */
static int bounds_ok(struct capn_segment *s, const char *p, size_t bytes)
{
	uintptr_t base, end, addr;

	if (!s || !s->data || !p)
		return 0;

	base = (uintptr_t) (void *) s->data;
	if (s->len > (uintptr_t) -1 - base)
		return 0;
	end = base + s->len;

	addr = (uintptr_t) (void *) p;
	if (addr < base || addr > end)
		return 0;
	if (bytes == 0)
		return 1;
	if (bytes > end - addr)
		return 0;
	return 1;
}

static int mul_size(size_t a, size_t b, size_t *out)
{
	if (b != 0 && a > (size_t) -1 / b)
		return 0;
	*out = a * b;
	return 1;
}

static int charge_traversal(struct capn *c, size_t bytes, int len)
{
	size_t add, limit, used;

	if (!c)
		return 0;

	add = bytes;
	/* Zero-size / void lists still cost one word per element. */
	if (add == 0 && len > 0) {
		if (!mul_size((size_t) len, 8, &add))
			return -1;
	}
	if (add == 0)
		return 0;

	limit = c->traversal_limit ? c->traversal_limit : CAPN_TRAVERSAL_DEFAULT;
	used = c->traversal_used;
	if (add > limit || used > limit - add)
		return -1;
	c->traversal_used = used + add;
	return 0;
}

static int session_nesting(struct capn *c)
{
	if (c && c->nesting_limit > 0)
		return c->nesting_limit;
	return CAPN_NESTING_DEFAULT;
}

static int hop_remaining(capn_ptr p)
{
	if (p.nesting_valid)
		return p.nesting;
	return p.seg ? session_nesting(p.seg->capn) : CAPN_NESTING_DEFAULT;
}

/* Advance *pd by (signed 30-bit word offset + 1) words from the
 * pointer word. Double-far uses a sentinel one word before s->data. */
static int apply_offset(struct capn_segment *s, char **pd, uint64_t val)
{
	int32_t off_words;
	int64_t rel, next;
	uintptr_t daddr, saddr;

	if (!s || !s->data || !pd || !*pd)
		return 0;

	off_words = (int32_t) ((int32_t) U32(val) >> 2);
	daddr = (uintptr_t) (void *) *pd;
	saddr = (uintptr_t) (void *) s->data;
	rel = (int64_t) daddr - (int64_t) saddr;
	next = rel + ((int64_t) off_words + 1) * 8;
	if (next < 0 || (uint64_t) next > s->len)
		return 0;
	*pd = s->data + (size_t) next;
	return 1;
}

static uint64_t lookup_double(struct capn_segment **s, char **d, uint64_t val) {
	uint64_t far, tag;
	size_t off;
	char *p;
	struct capn_segment *seg;

	if (!s || !*s) {
		if (s)
			*s = NULL;
		return 0;
	}

	off = (size_t) (U32(val) >> 3) * 8;
	seg = lookup_segment((*s)->capn, *s, U32(val >> 32));
	if (!seg) {
		*s = NULL;
		return 0;
	}
	*s = seg;

	if (off > seg->len || seg->len - off < 16) {
		*s = NULL;
		return 0;
	}

	p = seg->data + off;
	far = capn_flip64(*(uint64_t*) p);
	tag = capn_flip64(*(uint64_t*) (p+8));

	/* the far tag should not be another double, and the tag
	 * should be struct/list and have no offset */
	if ((far&7) != FAR_PTR || U32(tag) > LIST_PTR) {
		*s = NULL;
		return 0;
	}

	seg = lookup_segment(seg->capn, seg, U32(far >> 32));
	if (!seg) {
		*s = NULL;
		return 0;
	}
	*s = seg;

	/* -8 because far pointers reference from the start of
	 * the segment, but offsets reference the end of the
	 * pointer data. Here *d points to where an equivalent
	 * ptr would be.
	 */
	*d = seg->data - 8;
	return U64(U32(far) >> 3 << 2) | tag;
}

static uint64_t lookup_far(struct capn_segment **s, char **d, uint64_t val) {
	size_t off;
	struct capn_segment *seg;

	if (!s || !*s) {
		if (s)
			*s = NULL;
		return 0;
	}

	off = (size_t) (U32(val) >> 3) * 8;
	seg = lookup_segment((*s)->capn, *s, U32(val >> 32));
	if (!seg) {
		*s = NULL;
		return 0;
	}
	*s = seg;

	if (off > seg->len || seg->len - off < 8) {
		*s = NULL;
		return 0;
	}

	*d = seg->data + off;
	return capn_flip64(*(uint64_t*)*d);
}

/* rem is remaining hops including this one. rem==0 rejects.
 * rem<0 skips the nesting check (copy path).
 * err (OOB / budget / nesting / broken) sets decode_err. A wire-null
 * pointer word returns CAPN_NULL without taking this path. */
static capn_ptr read_ptr(struct capn_segment *s, char *d, int rem) {
	capn_ptr ret = {CAPN_NULL};
	uint64_t val;
	size_t nbytes = 0;
	struct capn *c = s ? s->capn : NULL;

	if (rem == 0)
		goto err;

	if (!s || !bounds_ok(s, d, 8))
		goto err;

	val = capn_flip64(*(uint64_t*) d);

	switch (val&7) {
	case FAR_PTR:
		val = lookup_far(&s, &d, val);
		if (!s)
			goto err;
		ret.has_ptr_tag = (U32(val) >> 2) == 0;
		break;
	case DOUBLE_PTR:
		val = lookup_double(&s, &d, val);
		if (!s)
			goto err;
		break;
	}

	/* Capability / other pointer: A=3. No landing object.
	 * B (bits 2-31) must be 0; C (high 32) is the table index.
	 * Do not apply_offset or charge_traversal: C is the table
	 * index, not an object size or list length. */
	if ((val & 3) == OTHER_PTR) {
		if ((U32(val) >> 2) != 0)
			goto err;
		ret.type = CAPN_INTERFACE;
		ret.len = (int) U32(val >> 32);
		ret.seg = s;
		ret.data = d;
		if (rem > 0) {
			ret.nesting_valid = 1;
			ret.nesting = rem - 1;
		}
		return ret;
	}

	if (!apply_offset(s, &d, val))
		goto err;

	switch (val & 3) {
	case STRUCT_PTR:
		ret.type = val ? CAPN_STRUCT : CAPN_NULL;
		ret.datasz = U32(U16(val >> 32)) * 8;
		ret.ptrs = U32(U16(val >> 48));
		if ((size_t) ret.datasz > (size_t) -1 - 8u * (size_t) ret.ptrs)
			goto err;
		nbytes = (size_t) ret.datasz + 8u * (size_t) ret.ptrs;
		break;

	case LIST_PTR:
		ret.type = CAPN_LIST;
		ret.len = (int) (val >> 35);
		if (ret.len < 0)
			goto err;

		switch ((val >> 32) & 7) {
		case VOID_LIST:
			nbytes = 0;
			break;
		case BIT_1_LIST:
			ret.type = CAPN_BIT_LIST;
			nbytes = ((size_t) ret.len + 7) / 8;
			if (nbytes > (1u << 19) - 1)
				goto err;
			ret.datasz = (unsigned) nbytes;
			break;
		case BYTE_1_LIST:
			ret.datasz = 1;
			if (!mul_size((size_t) ret.len, 1, &nbytes))
				goto err;
			break;
		case BYTE_2_LIST:
			ret.datasz = 2;
			if (!mul_size((size_t) ret.len, 2, &nbytes))
				goto err;
			break;
		case BYTE_4_LIST:
			ret.datasz = 4;
			if (!mul_size((size_t) ret.len, 4, &nbytes))
				goto err;
			break;
		case BYTE_8_LIST:
			ret.datasz = 8;
			if (!mul_size((size_t) ret.len, 8, &nbytes))
				goto err;
			break;
		case PTR_LIST:
			ret.type = CAPN_PTR_LIST;
			if (!mul_size((size_t) ret.len, 8, &nbytes))
				goto err;
			break;
		case COMPOSITE_LIST: {
			size_t words, stride, total;
			uint64_t tag;

			if (!mul_size((size_t) ret.len, 8, &words))
				goto err;
			if (!bounds_ok(s, d, 8))
				goto err;

			tag = capn_flip64(*(uint64_t*) d);
			d += 8;
			if (!bounds_ok(s, d, words))
				goto err;

			ret.datasz = U32(U16(tag >> 32)) * 8;
			ret.ptrs = U32(U16(tag >> 48));
			ret.len = (int) (U32(tag) >> 2);
			ret.is_composite_list = 1;
			if (ret.len < 0)
				goto err;

			if ((size_t) ret.datasz > (size_t) -1 - 8u * (size_t) ret.ptrs)
				goto err;
			stride = (size_t) ret.datasz + 8u * (size_t) ret.ptrs;
			if (!mul_size(stride, (size_t) ret.len, &total))
				goto err;
			if (total != words)
				goto err;
			nbytes = words;
			break;
		}
		default:
			goto err;
		}
		break;

	default:
		goto err;
	}

	if (!bounds_ok(s, d, nbytes))
		goto err;

	if (charge_traversal(s->capn, nbytes, ret.len) != 0)
		goto err;

	ret.data = d;
	ret.seg = s;
	if (rem > 0) {
		ret.nesting_valid = 1;
		ret.nesting = rem - 1;
	}
	return ret;
err:
	if (c)
		c->decode_err = 1;
	memset(&ret, 0, sizeof(ret));
	return ret;
}

/* First data word of a struct pointer, or NULL. minsz is bytes.
 * read_ptr checks A=0 and converts C (words) to datasz (bytes). */
static char *struct_ptr(struct capn_segment *s, char *d, int minsz) {
	capn_ptr p;

	if (!s)
		return NULL;

	p = read_ptr(s, d, -1);
	if (p.type != CAPN_STRUCT || !p.data)
		return NULL;
	if (minsz > 0 && p.datasz < minsz)
		return NULL;
	return p.data;
}

void capn_resolve(capn_ptr *p) {
	if (p->type == CAPN_FAR_POINTER) {
		*p = read_ptr(p->seg, p->data, hop_remaining(*p));
	}
}

#define CAPN_V_EMPTY 0
#define CAPN_V_PATH 1
#define CAPN_V_DONE 2

/* Key is (seg, data, type, is_composite_list). Composite List(T)
 * decode sets data to element 0, so (seg, data) alone aliases the
 * list and T[0]. */
struct capn_vslot {
	struct capn_segment *seg;
	char *data;
	unsigned type;
	unsigned is_composite_list;
	unsigned state;
};

struct capn_vset {
	struct capn_vslot *tab;
	size_t cap;
	size_t n;
};

struct capn_frame {
	capn_ptr p;
	int idx;
	int nchild;
};

static size_t vhash(struct capn_segment *s, char *d, unsigned type,
		    unsigned is_comp)
{
	uintptr_t a = (uintptr_t) (void *) s;
	uintptr_t b = (uintptr_t) (void *) d;
	uintptr_t t = (uintptr_t) type + ((uintptr_t) is_comp << 8);
	return (size_t) ((a * (uintptr_t) 11400714819323198485ull)
			 ^ (b * (uintptr_t) 14029467366897019727ull)
			 ^ (t * (uintptr_t) 1609587929392839161ull));
}

static int vset_init(struct capn_vset *vs)
{
	vs->cap = 64;
	vs->n = 0;
	vs->tab = (struct capn_vslot *) capn_zalloc(vs->cap * sizeof(*vs->tab));
	return vs->tab ? 0 : -1;
}

static void vset_free(struct capn_vset *vs)
{
	capn_freemem(vs->tab);
	vs->tab = NULL;
	vs->cap = 0;
	vs->n = 0;
}

static struct capn_vslot *vset_find(struct capn_vset *vs, struct capn_segment *s,
				    char *d, unsigned type, unsigned is_comp,
				    int insert)
{
	size_t mask, i;

	if (!vs->tab || vs->cap == 0)
		return NULL;
	mask = vs->cap - 1;
	i = vhash(s, d, type, is_comp) & mask;
	for (;;) {
		struct capn_vslot *sl = &vs->tab[i];
		if (sl->state == CAPN_V_EMPTY)
			return insert ? sl : NULL;
		if (sl->seg == s && sl->data == d && sl->type == type
		    && sl->is_composite_list == is_comp)
			return sl;
		i = (i + 1) & mask;
	}
}

static int vset_grow(struct capn_vset *vs)
{
	struct capn_vslot *old = vs->tab;
	size_t oldcap = vs->cap, i, oldn = vs->n;

	vs->cap *= 2;
	vs->tab = (struct capn_vslot *) capn_zalloc(vs->cap * sizeof(*vs->tab));
	if (!vs->tab) {
		vs->tab = old;
		vs->cap = oldcap;
		return -1;
	}
	vs->n = 0;
	for (i = 0; i < oldcap; i++) {
		if (old[i].state != CAPN_V_EMPTY) {
			struct capn_vslot *sl = vset_find(vs, old[i].seg, old[i].data,
							  old[i].type,
							  old[i].is_composite_list, 1);
			if (!sl) {
				capn_freemem(vs->tab);
				vs->tab = old;
				vs->cap = oldcap;
				vs->n = oldn;
				return -1;
			}
			*sl = old[i];
			vs->n++;
		}
	}
	capn_freemem(old);
	return 0;
}

/* -1 cycle/oom, 0 newly visiting, 1 already done, 2 skip (null). */
static int vset_enter(struct capn_vset *vs, capn_ptr p)
{
	struct capn_vslot *sl;

	if (p.type == CAPN_NULL || !p.data || !p.seg)
		return 2;
	if (vs->n * 3 > vs->cap * 2) {
		if (vset_grow(vs))
			return -1;
	}
	sl = vset_find(vs, p.seg, p.data, p.type, p.is_composite_list, 1);
	if (!sl)
		return -1;
	if (sl->state == CAPN_V_PATH)
		return -1;
	if (sl->state == CAPN_V_DONE)
		return 1;
	sl->seg = p.seg;
	sl->data = p.data;
	sl->type = p.type;
	sl->is_composite_list = p.is_composite_list;
	sl->state = CAPN_V_PATH;
	vs->n++;
	return 0;
}

static void vset_leave(struct capn_vset *vs, capn_ptr p)
{
	struct capn_vslot *sl;

	if (p.type == CAPN_NULL || !p.data || !p.seg)
		return;
	sl = vset_find(vs, p.seg, p.data, p.type, p.is_composite_list, 0);
	if (sl)
		sl->state = CAPN_V_DONE;
}

/* Like vset_enter, but a DONE object is visited again (canonical
 * form duplicates shared objects). PATH is still a cycle. */
static int vset_reenter(struct capn_vset *vs, capn_ptr p)
{
	struct capn_vslot *sl;

	if (p.type == CAPN_NULL || p.type == CAPN_INTERFACE || !p.data || !p.seg)
		return 2;
	if (vs->n * 3 > vs->cap * 2) {
		if (vset_grow(vs))
			return -1;
	}
	sl = vset_find(vs, p.seg, p.data, p.type, p.is_composite_list, 1);
	if (!sl)
		return -1;
	if (sl->state == CAPN_V_PATH)
		return -1;
	if (sl->state == CAPN_V_EMPTY) {
		sl->seg = p.seg;
		sl->data = p.data;
		sl->type = p.type;
		sl->is_composite_list = p.is_composite_list;
		vs->n++;
	}
	sl->state = CAPN_V_PATH;
	return 0;
}

static int object_nchild(capn_ptr p)
{
	switch (p.type) {
	case CAPN_STRUCT:
		return (int) p.ptrs;
	case CAPN_PTR_LIST:
		return p.len;
	case CAPN_LIST:
		if (p.ptrs > 0 && p.len > 0) {
			if (p.len > INT_MAX / (int) p.ptrs)
				return -1;
			return p.len * (int) p.ptrs;
		}
		return 0;
	default:
		return 0;
	}
}

static int child_slot(capn_ptr p, int idx, struct capn_segment **s, char **d)
{
	switch (p.type) {
	case CAPN_STRUCT:
		*s = p.seg;
		*d = p.data + p.datasz + 8 * idx;
		return 0;
	case CAPN_PTR_LIST:
		*s = p.seg;
		*d = p.data + 8 * idx;
		return 0;
	case CAPN_LIST: {
		int mem, poff;
		if (p.ptrs <= 0)
			return -1;
		mem = idx / (int) p.ptrs;
		poff = idx % (int) p.ptrs;
		*s = p.seg;
		*d = p.data + mem * (p.datasz + 8 * (int) p.ptrs)
			+ p.datasz + 8 * poff;
		return 0;
	}
	default:
		return -1;
	}
}

static int ptr_word_null(struct capn_segment *s, char *d)
{
	if (!s || !bounds_ok(s, d, 8))
		return -1;
	return capn_flip64(*(uint64_t *) d) == 0;
}

int capn_validate(struct capn *c)
{
	size_t saved;
	int rc = -1, limit, nchild, isnull, rem, ent;
	struct capn_vset vs;
	struct capn_frame *stack = NULL;
	size_t sp = 0, sc = 0;
	capn_ptr root, first, child;
	struct capn_segment *s;
	char *d;

	if (!c)
		return -1;
	saved = c->traversal_used;
	memset(&vs, 0, sizeof(vs));
	limit = session_nesting(c);

	root = capn_root(c);
	if (!root.seg || !root.data) {
		rc = 0;
		goto done;
	}

	isnull = ptr_word_null(root.seg, root.data);
	if (isnull < 0)
		goto done;
	if (isnull) {
		rc = 0;
		goto done;
	}

	first = read_ptr(root.seg, root.data, limit);
	if (first.type == CAPN_NULL)
		goto done;

	if (vset_init(&vs))
		goto done;

	nchild = object_nchild(first);
	if (nchild < 0)
		goto done;

	sc = 64;
	stack = (struct capn_frame *) capn_alloc(sc * sizeof(*stack));
	if (!stack)
		goto done;
	if (vset_enter(&vs, first) != 0)
		goto done;
	stack[0].p = first;
	stack[0].idx = 0;
	stack[0].nchild = nchild;
	sp = 1;

	while (sp) {
		struct capn_frame *f = &stack[sp - 1];

		if (f->idx >= f->nchild) {
			vset_leave(&vs, f->p);
			sp--;
			continue;
		}

		if (child_slot(f->p, f->idx, &s, &d))
			goto done;
		f->idx++;

		isnull = ptr_word_null(s, d);
		if (isnull < 0)
			goto done;
		if (isnull)
			continue;

		rem = hop_remaining(f->p);
		child = read_ptr(s, d, rem);
		if (child.type == CAPN_NULL)
			goto done;

		ent = vset_enter(&vs, child);
		if (ent < 0)
			goto done;
		if (ent > 0)
			continue;

		if ((int) sp >= limit)
			goto done;

		if (sp == sc) {
			size_t nsc = sc * 2;
			struct capn_frame *ns;

			ns = (struct capn_frame *) capn_alloc(nsc * sizeof(*ns));
			if (!ns)
				goto done;
			memcpy(ns, stack, sc * sizeof(*ns));
			capn_freemem(stack);
			stack = ns;
			sc = nsc;
		}
		nchild = object_nchild(child);
		if (nchild < 0)
			goto done;
		stack[sp].p = child;
		stack[sp].idx = 0;
		stack[sp].nchild = nchild;
		sp++;
	}

	rc = 0;
done:
	c->traversal_used = saved;
	capn_freemem(stack);
	vset_free(&vs);
	return rc;
}

int capn_ok(const struct capn *c)
{
	return c != NULL && c->decode_err == 0;
}

void capn_clear_err(struct capn *c)
{
	if (c)
		c->decode_err = 0;
}

capn_ptr capn_getp(capn_ptr p, int off, int resolve) {
	capn_ptr ret = {CAPN_FAR_POINTER};
	int rem;

	ret.seg = p.seg;

	capn_resolve(&p);
	rem = hop_remaining(p);

	switch (p.type) {
	case CAPN_LIST:
		/* Return an inner pointer */
		if (off < p.len) {
			capn_ptr inner = {CAPN_STRUCT};
			inner.is_list_member = 1;
			inner.data = p.data + off * (p.datasz + 8*p.ptrs);
			inner.seg = p.seg;
			inner.datasz = p.datasz;
			inner.ptrs = p.ptrs;
			inner.nesting_valid = 1;
			inner.nesting = rem;
			return inner;
		} else {
			goto err;
		}

	case CAPN_BIT_LIST: {
		/* Inner 1-bit view. data points at the containing byte;
		 * ptrs is the bit within that byte. Not a pointer. */
		int bit;
		capn_ptr inner = {CAPN_BIT_LIST};
		if (off < 0 || off >= p.len || !p.data)
			goto err;
		bit = off + (int) p.ptrs;
		inner.is_list_member = 1;
		inner.seg = p.seg;
		inner.data = p.data + (bit >> 3);
		inner.datasz = 1;
		inner.len = 1;
		inner.ptrs = (unsigned) (bit & 7);
		inner.nesting_valid = 1;
		inner.nesting = rem;
		return inner;
	}

	case CAPN_STRUCT:
		if (off >= p.ptrs) {
			goto err;
		}
		ret.data = p.data + p.datasz + 8*off;
		break;

	case CAPN_PTR_LIST:
		if (off >= p.len) {
			goto err;
		}
		if (!resolve) {
			/* List(Struct) upgrade: C=6 is 0 data words + 1
			 * pointer per element (encoding.html). get_Foo uses
			 * resolve=0 then read_Foo. List(Text) / List(Data) /
			 * List(AnyPointer) use resolve=1 and keep chasing. */
			capn_ptr inner = {CAPN_STRUCT};
			inner.is_list_member = 1;
			inner.data = p.data + 8 * off;
			inner.seg = p.seg;
			inner.datasz = 0;
			inner.ptrs = 1;
			inner.nesting_valid = 1;
			inner.nesting = rem;
			return inner;
		}
		ret.data = p.data + 8*off;
		break;

	default:
		goto err;
	}

	ret.nesting_valid = 1;
	ret.nesting = rem;
	if (resolve) {
		ret = read_ptr(ret.seg, ret.data, rem);
	}

	return ret;

err:
	memset(&p, 0, sizeof(p));
	return p;
}

/* 30-bit signed pointer offset, in words. */
#define CAPN_PTR_OFF_MIN_WORDS (-(int64_t)(1 << 29))
#define CAPN_PTR_OFF_MAX_WORDS ((int64_t)(1 << 29) - 1)

static int write_ptr_tag(char *d, capn_ptr p, int64_t off) {
	if (p.type == CAPN_INTERFACE) {
		/* A=3, B=0, C = capability table index (p.len). */
		uint64_t cap = OTHER_PTR | (U64((uint32_t) p.len) << 32);
		*(uint64_t*) d = capn_flip64(cap);
		return 0;
	}

	/*
	lsb                      struct pointer                       msb
	+-+-----------------------------+---------------+---------------+
	|A|             B               |       C       |       D       |
	+-+-----------------------------+---------------+---------------+

	A (2 bits) = 0, to indicate that this is a struct pointer.
	B (30 bits) = Offset, in words, from the end of the pointer to the
		start of the struct's data section.  Signed.
	C (16 bits) = Size of the struct's data section, in words.
	D (16 bits) = Size of the struct's pointer section, in words.

	For B we can't simply left-shift by 2 bits since C11 6.5.7.4
	https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
	says we can get undefined behavior when the left-shift exceeds
	the signed integer (ie. values run into the sign bit). The
	ASAN detector will rightly complain. So we do two's complement
	manually, and check bounds, to stay within unsigned arithmetic.

	off is the byte offset (typically pdata - d - 8). It is int64_t so a
	ptrdiff_t on LP64 is not truncated to int before the 30-bit check.
	On overflow the destination is left unwritten and -1 is returned.
	*/
	const int64_t off_words = off / 8;
	uint64_t val;

	if (off_words < CAPN_PTR_OFF_MIN_WORDS || off_words > CAPN_PTR_OFF_MAX_WORDS)
		return -1;

	if (off_words < 0) {
		uint32_t twos = 1u + ~(U32((uint64_t)(-off_words)) << 2);
		val = U64(twos);
	} else {
		val = U64(U32((uint64_t)off_words) << 2);
	}

	switch (p.type) {
	case CAPN_STRUCT:
		val |= STRUCT_PTR | (U64(p.datasz/8) << 32) | (U64(p.ptrs) << 48);
		break;

	case CAPN_LIST:
		if (p.is_composite_list) {
			val |= LIST_PTR | (U64(COMPOSITE_LIST) << 32) | (U64(p.len * (p.datasz/8 + p.ptrs)) << 35);
		} else {
			val |= LIST_PTR | (U64(p.len) << 35);

			switch (p.datasz) {
			case 8:
				val |= (U64(BYTE_8_LIST) << 32);
				break;
			case 4:
				val |= (U64(BYTE_4_LIST) << 32);
				break;
			case 2:
				val |= (U64(BYTE_2_LIST) << 32);
				break;
			case 1:
				val |= (U64(BYTE_1_LIST) << 32);
				break;
			case 0:
				val |= (U64(VOID_LIST) << 32);
				break;
			}
		}
		break;

	case CAPN_BIT_LIST:
		val |= LIST_PTR | (U64(BIT_1_LIST) << 32) | (U64(p.len) << 35);
		break;

	case CAPN_PTR_LIST:
		val |= LIST_PTR | (U64(PTR_LIST) << 32) | (U64(p.len) << 35);
		break;

	default:
		val = 0;
		break;
	}

	*(uint64_t*) d = capn_flip64(val);
	return 0;
}

static void write_far_ptr(char *d, struct capn_segment *s, char *tgt) {
	*(uint64_t*) d = capn_flip64(FAR_PTR | U64(tgt - s->data) | (U64(s->id) << 32));
}

static void write_double_far(char *d, struct capn_segment *s, char *tgt) {
	*(uint64_t*) d = capn_flip64(DOUBLE_PTR | U64(tgt - s->data) | (U64(s->id) << 32));
}

#define NEED_TO_COPY 1

static int write_ptr(struct capn_segment *s, char *d, capn_ptr p) {
	/* note p.seg can be NULL if its a ptr to static data */
	char *pdata;

	/* Capability pointers have no object; the whole value is the tag. */
	if (p.type == CAPN_INTERFACE)
		return write_ptr_tag(d, p, 0);

	pdata = p.data - 8*p.is_composite_list;

	/* CAPN_NULL or data == NULL encode as a null pointer (all-zero).
	 * Zero-init C structs (`= {0}` / memset) omit optional pointer
	 * fields. A STRUCT with datasz==0 && ptrs==0 and allocated data
	 * is a real empty struct: A=0 B=-1 C=D=0 (0xFFFFFFFC). Offset 0
	 * with C=D=0 is indistinguishable from null. */
	if (p.type == CAPN_NULL || p.data == NULL) {
		return write_ptr_tag(d, p, 0);

	} else if (p.type == CAPN_STRUCT && p.datasz == 0 && p.ptrs == 0) {
		return write_ptr_tag(d, p, -8);

	} else if (!p.seg || p.seg->capn != s->capn || p.is_list_member) {
		return NEED_TO_COPY;

	} else if (p.seg == s) {
		return write_ptr_tag(d, p, pdata - d - 8);

	} else if (p.has_ptr_tag) {
		/* By lucky chance, the data has a tag in front
		 * of it. This happens when new_object had to move
		 * the data to a new segment. */
		write_far_ptr(d, p.seg, pdata-8);
		return 0;

	} else if (p.seg->len + 8 <= p.seg->cap) {
		/* The target segment has enough room for tag */
		char *t = p.seg->data + p.seg->len;
		if (write_ptr_tag(t, p, pdata - t - 8))
			return -1;
		write_far_ptr(d, p.seg, t);
		p.seg->len += 8;
		return 0;

	} else {
		/* have to allocate room for a double far
		 * pointer */
		char *t;

		if (s->len + 16 <= s->cap) {
			/* Try and allocate in the src segment
			 * first. This should improve lookup on
			 * read. */
			t = s->data + s->len;
			s->len += 16;
		} else {
			t = new_data(s->capn, 16, &s);
			if (!t) return -1;
		}

		write_far_ptr(t, p.seg, pdata);
		if (write_ptr_tag(t+8, p, 0))
			return -1;
		write_double_far(d, s, t);
		return 0;
	}
}

struct copy {
	struct capn_tree hdr;
	struct capn_ptr to, from;
	char *fbegin, *fend;
};

static capn_ptr new_clone(struct capn_segment *s, capn_ptr p) {
	switch (p.type) {
	case CAPN_STRUCT:
		return capn_new_struct(s, p.datasz, p.ptrs);
	case CAPN_PTR_LIST:
		return capn_new_ptr_list(s, p.len);
	case CAPN_BIT_LIST:
		return capn_new_list1(s, p.len).p;
	case CAPN_LIST:
		if (p.is_composite_list)
			return capn_new_struct_list(s, p.len, p.datasz, p.ptrs);
		return capn_new_list(s, p.len, p.datasz, p.ptrs);
	case CAPN_INTERFACE: {
		capn_ptr n = capn_new_interface(s, p.datasz, p.ptrs);
		n.len = p.len;
		return n;
	}
	default:
		return p;
	}
}

static int is_ptr_equal(const struct capn_ptr *a, const struct capn_ptr *b) {
	return a->data == b->data
		&& a->type == b->type
		&& a->len == b->len
		&& a->datasz == b->datasz
		&& a->ptrs == b->ptrs;
}

static int data_size(struct capn_ptr p) {
	switch (p.type) {
	case CAPN_BIT_LIST:
		return p.datasz;
	case CAPN_PTR_LIST:
		return p.len*8;
	case CAPN_STRUCT:
		return p.datasz + 8*p.ptrs;
	case CAPN_LIST:
		return p.len * (p.datasz + 8*p.ptrs) + 8*p.is_composite_list;
	default:
		return 0;
	}
}

static int copy_ptr(struct capn_segment *seg, char *data, struct capn_ptr *t, struct capn_ptr *f, int *dep) {
	struct capn *c = seg->capn;
	struct copy *cp = NULL;
	struct capn_tree **xcp;
	char *fbegin = f->data - 8*f->is_composite_list;
	char *fend = fbegin + data_size(*f);
	int zero_sized = (fend == fbegin)
		|| (f->type == CAPN_BIT_LIST && f->is_list_member);

	/* We always copy list members as it would otherwise be an
	 * overlapped pointer (the data is owned by the enclosing list).
	 * We do not bother with the overlapped lookup for zero sized
	 * structures/lists as they never overlap. Nor do we add them to
	 * the copy list as there is no data to be shared by multiple
	 * pointers.
	 */

	xcp = &c->copy;
	while (*xcp && !zero_sized) {
		cp = (struct copy*) *xcp;
		if (fend <= cp->fbegin) {
			xcp = &cp->hdr.link[0];
		} else if (cp->fend <= fbegin) {
			xcp = &cp->hdr.link[1];
		} else if (is_ptr_equal(f, &cp->from)) {
			/* we already have a copy so just point to that */
			return write_ptr(seg, data, cp->to);
		} else {
			/* pointer to overlapped data */
			return -1;
		}
	}

	/* no copy found - have to create a new copy */
	*t = new_clone(seg, *f);

	if (write_ptr(seg, data, *t))
		return -1;

	/* add the copy to the copy tree so we can look for overlapping
	 * source pointers and handle recursive structures */
	if (!zero_sized) {
		struct copy *n;
		struct capn_segment *cs = c->copylist;

		/* need to allocate a struct copy */
		if (!cs || cs->len + (int)sizeof(*n) > cs->cap) {
			cs = c->create_local ? c->create_local(c->user, sizeof(*n)) : NULL;
			if (!cs) {
				/* can't allocate a copy structure */
				return -1;
			}
			cs->next = c->copylist;
			c->copylist = cs;
		}

		n = (struct copy*) (cs->data + cs->len);
		cs->len += sizeof(*n);

		n->from = *f;
		n->to = *t;
		n->fbegin = fbegin;
		n->fend = fend;

		*xcp = &n->hdr;
		n->hdr.parent = cp ? &cp->hdr : NULL;

		c->copy = capn_tree_insert(c->copy, &n->hdr);
	}

	/* minimize the number of types the main copy routine has to
	 * deal with to just CAPN_LIST and CAPN_PTR_LIST. ptr list only
	 * needs t->type, t->len, t->data, t->seg, f->data, f->seg to
	 * be valid */
	switch (t->type) {
	case CAPN_STRUCT:
		if (t->datasz) {
			memcpy(t->data, f->data, t->datasz);
			t->data += t->datasz;
			f->data += t->datasz;
		}
		if (t->ptrs) {
			t->type = CAPN_PTR_LIST;
			t->len = t->ptrs;
			(*dep)++;
		}
		return 0;

	case CAPN_BIT_LIST:
		if (f->is_list_member) {
			capn_list1 fl;
			fl.p = *f;
			t->data[0] = (char) (capn_get1(fl, 0) ? 1 : 0);
			return 0;
		}
		memcpy(t->data, f->data, t->datasz);
		return 0;

	case CAPN_LIST:
		if (!t->len) {
			/* empty list - nothing to copy */
		} else if (t->ptrs && t->datasz) {
			(*dep)++;
		} else if (t->datasz) {
			memcpy(t->data, f->data, t->len * t->datasz);
		} else if (t->ptrs) {
			t->type = CAPN_PTR_LIST;
			t->len *= t->ptrs;
			(*dep)++;
		}
		return 0;

	case CAPN_PTR_LIST:
		if (t->len) {
			(*dep)++;
		}
		return 0;

	case CAPN_INTERFACE:
		return 0;

	default:
		return -1;
	}
}

static void copy_list_member(capn_ptr* t, capn_ptr *f, int *dep) {
	/* copy struct data */
	int sz = min(t->datasz, f->datasz);
	memcpy(t->data, f->data, sz);
	memset(t->data + sz, 0, t->datasz - sz);
	t->data += t->datasz;
	f->data += f->datasz;

	/* reset excess pointers */
	sz = min(t->ptrs, f->ptrs);
	memset(t->data + sz, 0, 8*(t->ptrs - sz));

	/* create a pointer list for the main loop to copy */
	if (t->ptrs) {
		t->type = CAPN_PTR_LIST;
		t->len = t->ptrs;
		(*dep)++;
	}
}

#define MAX_COPY_DEPTH 32

int capn_setp(capn_ptr p, int off, capn_ptr tgt) {
	struct capn_ptr to[MAX_COPY_DEPTH], from[MAX_COPY_DEPTH];
	char *data;
	int err, dep = 0;

	capn_resolve(&p);

	if (tgt.type == CAPN_FAR_POINTER && tgt.seg->capn == p.seg->capn) {
		uint64_t val = capn_flip64(*(uint64_t*) tgt.data);
		if ((val & 3) == FAR_PTR) {
			*(uint64_t*) p.data = *(uint64_t*) tgt.data;
			return 0;
		}
	}

	capn_resolve(&tgt);

	switch (p.type) {
	case CAPN_LIST:
		if (off >= p.len || tgt.type != CAPN_STRUCT)
			return -1;

		to[0] = p;
		to[0].data += off * (p.datasz + 8*p.ptrs);
		from[0] = tgt;
		copy_list_member(to, from, &dep);
		break;

	case CAPN_BIT_LIST: {
		capn_list1 dst;
		int val;
		if (off < 0 || off >= p.len)
			return -1;
		if (tgt.type == CAPN_NULL || tgt.data == NULL) {
			val = 0;
		} else if (tgt.type == CAPN_BIT_LIST) {
			capn_list1 src;
			src.p = tgt;
			val = capn_get1(src, 0);
		} else if (tgt.type == CAPN_STRUCT && tgt.datasz >= 1 && tgt.data) {
			val = (tgt.data[0] & 1) != 0;
		} else {
			return -1;
		}
		dst.p = p;
		return capn_set1(dst, off, val);
	}

	case CAPN_PTR_LIST:
		if (off >= p.len)
			return -1;
		data = p.data + 8*off;
		goto copy_ptr;

	case CAPN_STRUCT:
		if (off >= p.ptrs)
			return -1;
		data = p.data + p.datasz + 8*off;
		goto copy_ptr;

	copy_ptr:
		err = write_ptr(p.seg, data, tgt);
		if (err != NEED_TO_COPY)
			return err;

		/* Depth first copy the source whilst using a pointer stack to
		 * maintain the ptr to set and size left to copy at each level.
		 * We also maintain a rbtree (capn->copy) of the copies indexed
		 * by the source data. This way we can detect overlapped
		 * pointers in the source (and bail) and recursive structures
		 * (and point to the previous copy).
		 */

		from[0] = tgt;
		if (copy_ptr(p.seg, data, to, from, &dep))
			return -1;
		break;

	default:
		return -1;
	}

	while (dep) {
		struct capn_ptr *tc = &to[dep-1], *tn = &to[dep];
		struct capn_ptr *fc = &from[dep-1], *fn = &from[dep];

		if (dep+1 == MAX_COPY_DEPTH) {
			return -1;
		}

		if (!tc->len) {
			dep--;
			continue;
		}

		if (tc->type == CAPN_LIST) {
			*fn = capn_getp(*fc, 0, 1);
			*tn = capn_getp(*tc, 0, 1);

			copy_list_member(tn, fn, &dep);

			fc->data += fc->datasz + 8*fc->ptrs;
			tc->data += tc->datasz + 8*tc->ptrs;
			tc->len--;

		} else { /* CAPN_PTR_LIST */
			*fn = read_ptr(fc->seg, fc->data, -1);

			if (fn->type && copy_ptr(tc->seg, tc->data, tn, fn, &dep))
				return -1;

			fc->data += 8;
			tc->data += 8;
			tc->len--;
		}
	}

	return 0;
}

int capn_get1(capn_list1 l, int off) {
	char *d;
	int bit;
	capn_ptr p;
	capn_resolve(&l.p);
	p = l.p;
	if (off < 0)
		return 0;

	switch (p.type) {
	case CAPN_BIT_LIST:
		if (off >= p.len || !p.data)
			return 0;
		bit = off + (int) p.ptrs;
		return (p.data[bit / 8] & (1 << (bit % 8))) != 0;

	case CAPN_LIST:
		if (off >= p.len || p.datasz < 1 || !p.data)
			return 0;
		d = p.data + off * (p.datasz + 8 * p.ptrs);
		return (d[0] & 1) != 0;

	case CAPN_PTR_LIST: {
		capn_ptr el;
		if (off >= p.len)
			return 0;
		el = read_ptr(p.seg, p.data + 8 * off, hop_remaining(p));
		if (el.type == CAPN_STRUCT && el.datasz >= 1 && el.data)
			return (el.data[0] & 1) != 0;
		if (el.type == CAPN_BIT_LIST) {
			capn_list1 bl;
			bl.p = el;
			return capn_get1(bl, 0);
		}
		return 0;
	}

	default:
		return 0;
	}
}

int capn_set1(capn_list1 l, int off, int val) {
	char *d;
	int bit;
	capn_ptr p;
	capn_resolve(&l.p);
	p = l.p;
	if (off < 0)
		return -1;

	switch (p.type) {
	case CAPN_BIT_LIST:
		if (off >= p.len || !p.data)
			return -1;
		bit = off + (int) p.ptrs;
		if (val) {
			p.data[bit / 8] |= 1 << (bit % 8);
		} else {
			p.data[bit / 8] &= ~(1 << (bit % 8));
		}
		return 0;

	case CAPN_LIST:
		if (off >= p.len || p.datasz < 1 || !p.data)
			return -1;
		d = p.data + off * (p.datasz + 8 * p.ptrs);
		if (val)
			d[0] |= 1;
		else
			d[0] &= ~1;
		return 0;

	case CAPN_PTR_LIST: {
		capn_ptr el;
		if (off >= p.len)
			return -1;
		el = read_ptr(p.seg, p.data + 8 * off, hop_remaining(p));
		if (el.type == CAPN_STRUCT && el.datasz >= 1 && el.data) {
			if (val)
				el.data[0] |= 1;
			else
				el.data[0] &= ~1;
			return 0;
		}
		if (el.type == CAPN_BIT_LIST) {
			capn_list1 bl;
			bl.p = el;
			return capn_set1(bl, 0, val);
		}
		return -1;
	}

	default:
		return -1;
	}
}

int capn_getv1(capn_list1 l, int off, uint8_t *data, int sz) {
	/* Note we only support aligned reads */
	int bsz;
	capn_ptr p;
	capn_resolve(&l.p);
	p = l.p;
	if (p.type != CAPN_BIT_LIST || (off & 7) != 0 || p.is_list_member)
		return -1;

	bsz = (sz + 7) / 8;
	off /= 8;

	if (off + sz > p.datasz) {
		memcpy(data, p.data + off, p.datasz - off);
		return p.len - off*8;
	} else {
		memcpy(data, p.data + off, bsz);
		return sz;
	}
}

int capn_setv1(capn_list1 l, int off, const uint8_t *data, int sz) {
	/* Note we only support aligned writes */
	int bsz;
	capn_ptr p = l.p;
	if (p.type != CAPN_BIT_LIST || (off & 7) != 0 || p.is_list_member)
		return -1;

	bsz = (sz + 7) / 8;
	off /= 8;

	if (off + sz > p.datasz) {
		memcpy(p.data + off, data, p.datasz - off);
		return p.len - off*8;
	} else {
		memcpy(p.data + off, data, bsz);
		return sz;
	}
}

/* pull out whether we add a tag or not as a define so the unit test can
 * test double far pointers by not creating tags */
#ifndef ADD_TAG
#define ADD_TAG 1
#endif

static void new_object(capn_ptr *p, int bytes) {
	struct capn_segment *s = p->seg;

	if (!s) {
		memset(p, 0, sizeof(*p));
		return;
	}

	/* pointer needs to be initialised to get a valid offset on write */
	if (!bytes) {
		p->data = s->data + s->len;
		return;
	}

	/* all allocations are 8 byte aligned */
	bytes = (bytes + 7) & ~7;

	if (s->len + bytes <= s->cap) {
		p->data = s->data + s->len;
		s->len += bytes;
		return;
	}

	/* add a tag whenever we switch segments so that write_ptr can
	 * use it */
	p->data = new_data(s->capn, bytes + ADD_TAG*8, &p->seg);
	if (!p->data) {
		memset(p, 0, sizeof(*p));
		return;
	}

	if (ADD_TAG) {
		if (write_ptr_tag(p->data, *p, 0)) {
			memset(p, 0, sizeof(*p));
			return;
		}
		p->data += 8;
		p->has_ptr_tag = 1;
	}
}

capn_ptr capn_root(struct capn *c) {
	capn_ptr r = {CAPN_PTR_LIST};
	r.seg = lookup_segment(c, NULL, 0);
	r.data = r.seg ? r.seg->data : new_data(c, 8, &r.seg);
	r.len = 1;
	r.nesting_valid = 1;
	r.nesting = session_nesting(c);

	if (!r.seg || r.seg->cap < 8) {
		memset(&r, 0, sizeof(r));
	} else if (r.seg->len < 8) {
		r.seg->len = 8;
	}

	return r;
}

int capn_set_root(struct capn *c, capn_ptr p) {
	return capn_setp(capn_root(c), 0, p);
}

static int word_zero(const char *d)
{
	return capn_flip64(*(const uint64_t *) d) == 0;
}

static int trunc_data_words(const char *data, int datasz)
{
	int n = datasz / 8;

	while (n > 0 && word_zero(data + (n - 1) * 8))
		n--;
	return n;
}

static int trunc_ptr_words(const char *data, int datasz, int ptrs)
{
	int n = ptrs;

	while (n > 0 && word_zero(data + datasz + (n - 1) * 8))
		n--;
	return n;
}

static void trunc_composite(capn_ptr list, int *out_dw, int *out_pw)
{
	int dw = (int) (list.datasz / 8);
	int pw = (int) list.ptrs;
	int i, stride;

	stride = (int) list.datasz + 8 * (int) list.ptrs;
	while (dw > 0) {
		int allz = 1;
		for (i = 0; i < list.len; i++) {
			if (!word_zero(list.data + i * stride + (dw - 1) * 8)) {
				allz = 0;
				break;
			}
		}
		if (!allz)
			break;
		dw--;
	}
	while (pw > 0) {
		int allz = 1;
		for (i = 0; i < list.len; i++) {
			if (!word_zero(list.data + i * stride + list.datasz + (pw - 1) * 8)) {
				allz = 0;
				break;
			}
		}
		if (!allz)
			break;
		pw--;
	}
	*out_dw = dw;
	*out_pw = pw;
}

static size_t pad8(size_t n)
{
	if (n > (size_t) -1 - 7)
		return (size_t) -1;
	return (n + 7) & ~(size_t) 7;
}

static int load_child(struct capn_segment *s, char *d, int rem, capn_ptr *out)
{
	int z;

	if (rem == 0)
		return -1;
	z = ptr_word_null(s, d);
	if (z < 0)
		return -1;
	if (z) {
		memset(out, 0, sizeof(*out));
		return 0;
	}
	*out = read_ptr(s, d, rem);
	if (out->type == CAPN_NULL)
		return -1;
	return 0;
}

static int canon_add(size_t *total, size_t add, size_t limit)
{
	if (add > limit || *total > limit - add)
		return -1;
	*total += add;
	return 0;
}

static char *canon_alloc(struct capn_segment *s, size_t n)
{
	char *p;

	if (n == 0)
		return s->data + s->len;
	if (s->len > s->cap || n > s->cap - s->len)
		return NULL;
	p = s->data + s->len;
	s->len += n;
	return p;
}

static int canon_emit(struct capn_segment *ds, char *ptr_loc, capn_ptr p,
		      int rem, struct capn_vset *vs);

static int canon_measure(capn_ptr p, int rem, struct capn_vset *vs,
			 size_t *total, size_t limit)
{
	int i, j, ent, dw, pw, stride;
	size_t add, nbytes;
	capn_ptr ch;

	if (p.type == CAPN_NULL || p.type == CAPN_INTERFACE)
		return 0;

	ent = vset_reenter(vs, p);
	if (ent < 0)
		return -1;
	if (ent == 2)
		return 0;

	switch (p.type) {
	case CAPN_STRUCT:
		dw = trunc_data_words(p.data, (int) p.datasz);
		pw = trunc_ptr_words(p.data, (int) p.datasz, (int) p.ptrs);
		if (dw == 0 && pw == 0) {
			vset_leave(vs, p);
			return 0;
		}
		add = (size_t) dw * 8u + (size_t) pw * 8u;
		if (canon_add(total, add, limit))
			goto fail;
		for (i = 0; i < pw; i++) {
			if (load_child(p.seg, p.data + p.datasz + 8 * i, rem, &ch))
				goto fail;
			if (canon_measure(ch, rem - 1, vs, total, limit))
				goto fail;
		}
		break;

	case CAPN_PTR_LIST:
		if (!mul_size((size_t) p.len, 8, &add))
			goto fail;
		if (canon_add(total, add, limit))
			goto fail;
		for (i = 0; i < p.len; i++) {
			if (load_child(p.seg, p.data + 8 * i, rem, &ch))
				goto fail;
			if (canon_measure(ch, rem - 1, vs, total, limit))
				goto fail;
		}
		break;

	case CAPN_BIT_LIST:
		nbytes = ((size_t) p.len + 7) / 8;
		add = pad8(nbytes);
		if (add == (size_t) -1 || canon_add(total, add, limit))
			goto fail;
		break;

	case CAPN_LIST:
		if (p.is_composite_list) {
			trunc_composite(p, &dw, &pw);
			if (!mul_size((size_t) p.len, (size_t) dw + (size_t) pw, &add))
				goto fail;
			if (!mul_size(add, 8, &add))
				goto fail;
			if (add > (size_t) -1 - 8)
				goto fail;
			add += 8;
			if (canon_add(total, add, limit))
				goto fail;
			stride = (int) p.datasz + 8 * (int) p.ptrs;
			for (i = 0; i < p.len; i++) {
				char *el = p.data + i * stride;
				for (j = 0; j < pw; j++) {
					if (load_child(p.seg, el + p.datasz + 8 * j, rem, &ch))
						goto fail;
					if (canon_measure(ch, rem - 1, vs, total, limit))
						goto fail;
				}
			}
		} else {
			if (p.datasz <= 0 || p.len <= 0) {
				add = 0;
			} else {
				if (!mul_size((size_t) p.len, (size_t) p.datasz, &nbytes))
					goto fail;
				add = pad8(nbytes);
				if (add == (size_t) -1)
					goto fail;
			}
			if (canon_add(total, add, limit))
				goto fail;
		}
		break;

	default:
		goto fail;
	}

	vset_leave(vs, p);
	return 0;
fail:
	vset_leave(vs, p);
	return -1;
}

static int canon_emit(struct capn_segment *ds, char *ptr_loc, capn_ptr p,
		      int rem, struct capn_vset *vs)
{
	int i, j, ent, dw, pw, stride, tstride;
	size_t add, nbytes;
	char *obj, *el, *dst_el;
	capn_ptr ch, tag;
	uint64_t hdr;

	if (p.type == CAPN_NULL) {
		memset(ptr_loc, 0, 8);
		return 0;
	}
	if (p.type == CAPN_INTERFACE)
		return write_ptr_tag(ptr_loc, p, 0);

	ent = vset_reenter(vs, p);
	if (ent < 0)
		return -1;
	if (ent == 2) {
		memset(ptr_loc, 0, 8);
		return 0;
	}

	switch (p.type) {
	case CAPN_STRUCT:
		dw = trunc_data_words(p.data, (int) p.datasz);
		pw = trunc_ptr_words(p.data, (int) p.datasz, (int) p.ptrs);
		if (dw == 0 && pw == 0) {
			tag = p;
			tag.datasz = 0;
			tag.ptrs = 0;
			if (write_ptr_tag(ptr_loc, tag, -8))
				goto fail;
			vset_leave(vs, p);
			return 0;
		}
		add = (size_t) dw * 8u + (size_t) pw * 8u;
		obj = canon_alloc(ds, add);
		if (!obj)
			goto fail;
		tag = p;
		tag.datasz = (unsigned) dw * 8u;
		tag.ptrs = (unsigned) pw;
		if (write_ptr_tag(ptr_loc, tag, (int64_t) (obj - ptr_loc - 8)))
			goto fail;
		if (dw)
			memcpy(obj, p.data, (size_t) dw * 8u);
		for (i = 0; i < pw; i++) {
			if (load_child(p.seg, p.data + p.datasz + 8 * i, rem, &ch))
				goto fail;
			if (canon_emit(ds, obj + (size_t) dw * 8u + 8 * (size_t) i,
				       ch, rem - 1, vs))
				goto fail;
		}
		break;

	case CAPN_PTR_LIST:
		if (!mul_size((size_t) p.len, 8, &add))
			goto fail;
		obj = canon_alloc(ds, add);
		if (!obj && add != 0)
			goto fail;
		if (write_ptr_tag(ptr_loc, p, (int64_t) (obj - ptr_loc - 8)))
			goto fail;
		for (i = 0; i < p.len; i++) {
			if (load_child(p.seg, p.data + 8 * i, rem, &ch))
				goto fail;
			if (canon_emit(ds, obj + 8 * (size_t) i, ch, rem - 1, vs))
				goto fail;
		}
		break;

	case CAPN_BIT_LIST:
		nbytes = ((size_t) p.len + 7) / 8;
		add = pad8(nbytes);
		if (add == (size_t) -1)
			goto fail;
		obj = canon_alloc(ds, add);
		if (!obj && add != 0)
			goto fail;
		if (write_ptr_tag(ptr_loc, p, (int64_t) (obj - ptr_loc - 8)))
			goto fail;
		if (nbytes)
			memcpy(obj, p.data, nbytes);
		if (p.len & 7)
			obj[nbytes - 1] = (char) ((unsigned char) obj[nbytes - 1]
						  & ((1u << (p.len & 7)) - 1u));
		break;

	case CAPN_LIST:
		if (p.is_composite_list) {
			trunc_composite(p, &dw, &pw);
			if (!mul_size((size_t) p.len, (size_t) dw + (size_t) pw, &add))
				goto fail;
			if (!mul_size(add, 8, &add))
				goto fail;
			if (add > (size_t) -1 - 8)
				goto fail;
			add += 8;
			obj = canon_alloc(ds, add);
			if (!obj)
				goto fail;
			tag = p;
			tag.datasz = (unsigned) dw * 8u;
			tag.ptrs = (unsigned) pw;
			if (write_ptr_tag(ptr_loc, tag, (int64_t) (obj - ptr_loc - 8)))
				goto fail;
			hdr = STRUCT_PTR | (U64(p.len) << 2)
				| (U64(dw) << 32) | (U64(pw) << 48);
			*(uint64_t *) obj = capn_flip64(hdr);
			stride = (int) p.datasz + 8 * (int) p.ptrs;
			tstride = dw * 8 + pw * 8;
			for (i = 0; i < p.len; i++) {
				el = p.data + i * stride;
				dst_el = obj + 8 + (size_t) i * (size_t) tstride;
				if (dw)
					memcpy(dst_el, el, (size_t) dw * 8u);
			}
			for (i = 0; i < p.len; i++) {
				el = p.data + i * stride;
				dst_el = obj + 8 + (size_t) i * (size_t) tstride;
				for (j = 0; j < pw; j++) {
					if (load_child(p.seg, el + p.datasz + 8 * j, rem, &ch))
						goto fail;
					if (canon_emit(ds, dst_el + (size_t) dw * 8u
							    + 8 * (size_t) j,
						       ch, rem - 1, vs))
						goto fail;
				}
			}
		} else {
			if (p.datasz <= 0 || p.len <= 0) {
				add = 0;
			} else {
				if (!mul_size((size_t) p.len, (size_t) p.datasz, &nbytes))
					goto fail;
				add = pad8(nbytes);
				if (add == (size_t) -1)
					goto fail;
			}
			obj = canon_alloc(ds, add);
			if (!obj && add != 0)
				goto fail;
			if (write_ptr_tag(ptr_loc, p, (int64_t) (obj - ptr_loc - 8)))
				goto fail;
			if (add && p.datasz > 0 && p.len > 0)
				memcpy(obj, p.data, (size_t) p.len * (size_t) p.datasz);
		}
		break;

	default:
		goto fail;
	}

	vset_leave(vs, p);
	return 0;
fail:
	vset_leave(vs, p);
	return -1;
}

int capn_canonicalize(struct capn *src, struct capn *dst)
{
	size_t saved, limit, need;
	int rc = -1, nest, z;
	struct capn_vset vs;
	struct capn_segment *s;
	capn_ptr root, first;
	int64_t create_sz;

	if (!src || !dst || src == dst || !dst->create)
		return -1;
	if (dst->segnum != 0)
		return -1;

	saved = src->traversal_used;
	memset(&vs, 0, sizeof(vs));
	memset(&first, 0, sizeof(first));
	limit = src->traversal_limit ? src->traversal_limit : CAPN_TRAVERSAL_DEFAULT;
	nest = session_nesting(src);
	need = 8;

	if (!src->seglist) {
		first.type = CAPN_NULL;
		goto alloc;
	}

	root = capn_root(src);
	if (!root.seg || !root.data)
		goto done;
	z = ptr_word_null(root.seg, root.data);
	if (z < 0)
		goto done;
	if (z) {
		first.type = CAPN_NULL;
		goto alloc;
	}
	first = read_ptr(root.seg, root.data, nest);
	if (first.type == CAPN_NULL)
		goto done;

	if (vset_init(&vs))
		goto done;
	if (canon_measure(first, hop_remaining(first), &vs, &need, limit))
		goto done;
	vset_free(&vs);
	memset(&vs, 0, sizeof(vs));
	src->traversal_used = saved;

alloc:
	if (need < 8 || need > (size_t) INT_MAX)
		goto done;
	create_sz = (int64_t) need;
	s = dst->create(dst->user, 0, (int) create_sz);
	if (!s)
		goto done;
	capn_append_segment(dst, s);
	if (s->cap < need)
		goto done;
	s->len = 8;

	if (first.type == CAPN_NULL) {
		rc = 0;
		goto done;
	}

	if (vset_init(&vs))
		goto done;
	if (canon_emit(s, s->data, first, hop_remaining(first), &vs))
		goto done;
	rc = 0;
done:
	src->traversal_used = saved;
	vset_free(&vs);
	return rc;
}

capn_ptr capn_new_struct(struct capn_segment *seg, int datasz, int ptrs) {
	capn_ptr p = {CAPN_STRUCT};
	p.seg = seg;
	p.datasz = (datasz + 7) & ~7;
	p.ptrs = ptrs;
	new_object(&p, p.datasz + 8*p.ptrs);
	return p;
}

capn_ptr capn_new_interface(struct capn_segment *seg, int datasz, int ptrs) {
	/* Capability pointer: A=3, B=0, C=index. No object body.
	 * datasz/ptrs are accepted for ABI compatibility with
	 * capn_new_struct and are not stored. */
	capn_ptr p;
	memset(&p, 0, sizeof(p));
	(void) datasz;
	(void) ptrs;
	if (!seg)
		return p;
	p.type = CAPN_INTERFACE;
	p.seg = seg;
	return p;
}

capn_ptr capn_new_struct_list(struct capn_segment *seg, int sz, int datasz, int ptrs) {
	/* List(Struct) is always C=7 plus a tag, including empty lists
	 * and 0-pointer / 1-word / 0-word structs. List(Void) stays on
	 * capn_new_list(seg, n, 0, 0). */
	capn_ptr p = {CAPN_LIST};
	p.seg = seg;
	p.len = sz;
	p.is_composite_list = 1;
	p.datasz = (datasz + 7) & ~7;
	p.ptrs = ptrs;
	new_object(&p, p.len * (p.datasz + 8*p.ptrs) + 8);
	if (p.data) {
		uint64_t hdr = STRUCT_PTR | (U64(p.len) << 2) | (U64(p.datasz/8) << 32) | (U64(p.ptrs) << 48);
		*(uint64_t*) p.data = capn_flip64(hdr);
		p.data += 8;
	}
	return p;
}

capn_ptr capn_new_list(struct capn_segment *seg, int sz, int datasz, int ptrs) {
	capn_ptr p = {CAPN_LIST};
	p.seg = seg;
	p.len = sz;

	if (ptrs || datasz > 8) {
		return capn_new_struct_list(seg, sz, datasz, ptrs);
	} else if (datasz > 4) {
		p.datasz = 8;
		new_object(&p, p.len * 8);
	} else if (datasz > 2) {
		p.datasz = 4;
		new_object(&p, p.len * 4);
	} else {
		p.datasz = datasz;
		new_object(&p, p.len * datasz);
	}

	return p;
}

capn_list1 capn_new_list1(struct capn_segment *seg, int sz) {
	capn_list1 l = {{CAPN_BIT_LIST}};
	l.p.seg = seg;
	l.p.datasz = (sz+7)/8;
	l.p.len = sz;
	new_object(&l.p, l.p.datasz);
	return l;
}

capn_ptr capn_new_ptr_list(struct capn_segment *seg, int sz) {
	capn_ptr p = {CAPN_PTR_LIST};
	p.seg = seg;
	p.len = sz;
	p.ptrs = 0;
	p.datasz = 0;
	new_object(&p, sz*8);
	return p;
}

capn_ptr capn_new_string(struct capn_segment *seg, const char *str, ssize_t sz) {
	capn_ptr p = {CAPN_LIST};
	p.seg = seg;
	p.len = ((sz >= 0) ? (size_t)sz : strlen(str)) + 1;
	p.datasz = 1;
	new_object(&p, p.len);
	if (p.data) {
		memcpy(p.data, str, p.len - 1);
		p.data[p.len - 1] = '\0';
	}
	return p;
}

capn_text capn_get_text(capn_ptr p, int off, capn_text def) {
	capn_ptr m = capn_getp(p, off, 1);
	capn_text ret = def;
	if (m.type == CAPN_LIST && m.datasz == 1 && m.len && m.data[m.len - 1] == 0) {
		ret.seg = m.seg;
		ret.str = m.data;
		ret.len = m.len - 1;
	}
	return ret;
}

int capn_set_text(capn_ptr p, int off, capn_text tgt) {
	capn_ptr m = {CAPN_NULL};
	if (tgt.seg) {
		m.type = CAPN_LIST;
		m.seg = tgt.seg;
		m.data = (char*)tgt.str;
		m.len = tgt.len + 1;
		m.datasz = 1;
	} else if (tgt.str) {
		m = capn_new_string(p.seg, tgt.str, tgt.len);
	}
	return capn_setp(p, off, m);
}

capn_data capn_get_data(capn_ptr p, int off) {
	capn_data ret;
	ret.p = capn_getp(p, off, 1);
	if (ret.p.type != CAPN_LIST || ret.p.datasz != 1) {
		memset(&ret, 0, sizeof(ret));
	}
	return ret;
}

int capn_set_data(capn_ptr p, int off, capn_data tgt) {
	if (tgt.p.type != CAPN_LIST || tgt.p.datasz != 1) {
		memset(&tgt, 0, sizeof(tgt));
	}
	return capn_setp(p, off, tgt.p);
}

#define SZ 8
#include "capn-list.inc"
#undef SZ

#define SZ 16
#include "capn-list.inc"
#undef SZ

#define SZ 32
#include "capn-list.inc"
#undef SZ

#define SZ 64
#include "capn-list.inc"
#undef SZ
