/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Shell content pack host.
 *
 * Cap'n ShellView (c-capnproto) → Janet pack (capnp-janet) → Cap'n
 * PolicyDecision bytes (passthrough). Pack authors reason Text + code.
 */
#include "policy_janet.h"
#include "internal.h"
#include "policy.capnp.h"

#include "janet.h"

#include "grok-policyd/supervisor.h"

#include <capnp_c.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SHELL_HEAD_MAX 8192
#define PACK_PATH_MAX 4096
#define PACK_FILE_MAX (512 * 1024)
#define PACK_LIB_MAX_FILES 32
void capnp_janet_register(JanetTable *env);

static int janet_inited;
static int pack_loaded;
static int pack_failed;
static JanetTable *pack_env;
/* Absolute path of the pack currently loaded (or last reload attempt). */
static char pack_path_buf[PACK_PATH_MAX];
static int pack_path_set;

static const char *default_pack(void)
{
	if (pack_path_set && pack_path_buf[0])
		return pack_path_buf;
	{
		const char *e = getenv("GROKOS_POLICYD_JANET_PACK");

		if (e && e[0])
			return e;
	}
	return "policy/shell.janet";
}

static void seal_pack_env(JanetTable *env)
{
	janet_table_remove(env, janet_csymbolv("os"));
	janet_table_remove(env, janet_csymbolv("net"));
	janet_table_remove(env, janet_csymbolv("spork"));
}

/**
 * Drop loaded pack state so the next load re-reads disk.
 * Prior Janet env is abandoned for GC (reload is rare; no janet_deinit).
 */
static void unload_pack(void)
{
	pack_loaded = 0;
	pack_failed = 0;
	pack_env = NULL;
}

static int path_is_absolute_file(const char *path)
{
	struct stat st;

	if (!path || !path[0] || path[0] != '/')
		return 0;
	if (strlen(path) >= PACK_PATH_MAX)
		return 0;
	/* Reject ".." segments (lexical only; no realpath). */
	{
		const char *p = path;

		while (*p) {
			if (p[0] == '/' && p[1] == '.' && p[2] == '.' &&
			    (p[3] == '/' || p[3] == '\0'))
				return 0;
			p++;
		}
	}
	if (stat(path, &st) != 0)
		return 0;
	return S_ISREG(st.st_mode) ? 1 : 0;
}

/**
 * Read @a path into the pack env via janet_dobytes.
 * Returns 0 on success, -1 on I/O/size error, -2 on Janet load error.
 */
static int dobytes_file(JanetTable *env, const char *path)
{
	FILE *f;
	char *buf;
	long sz;
	Janet out;
	int rc;

	f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0 ||
	    sz > PACK_FILE_MAX || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	buf = malloc((size_t)sz);
	if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	rc = janet_dobytes(env, (const uint8_t *)buf, (int32_t)sz, path, &out);
	free(buf);
	return rc == 0 ? 0 : -2;
}

static int cmp_strptr(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/**
 * Load sorted .janet helpers from a sibling lib/ directory, if present.
 * Missing lib/ is OK (single-file packs / allow-all test packs).
 * Returns 0 on success (or no lib), -1 on failure.
 */
static int load_pack_libs(JanetTable *env, const char *pack_path)
{
	char dir[PACK_PATH_MAX];
	char libdir[PACK_PATH_MAX];
	char path[PACK_PATH_MAX];
	char *names[PACK_LIB_MAX_FILES];
	int nnames = 0;
	DIR *d;
	struct dirent *ent;
	const char *slash;
	size_t dlen;
	int i, rc;

	slash = strrchr(pack_path, '/');
	if (!slash || slash == pack_path)
		return 0;
	dlen = (size_t)(slash - pack_path);
	if (dlen + 1 >= sizeof(dir))
		return -1;
	memcpy(dir, pack_path, dlen);
	dir[dlen] = '\0';
	if (snprintf(libdir, sizeof(libdir), "%s/lib", dir) >=
	    (int)sizeof(libdir))
		return -1;

	d = opendir(libdir);
	if (!d)
		return 0; /* optional */

	while ((ent = readdir(d)) != NULL && nnames < PACK_LIB_MAX_FILES) {
		size_t len = strlen(ent->d_name);

		if (len < 7)
			continue;
		if (strcmp(ent->d_name + len - 6, ".janet") != 0)
			continue;
		if (ent->d_name[0] == '.')
			continue;
		names[nnames] = strdup(ent->d_name);
		if (!names[nnames]) {
			closedir(d);
			goto fail_names;
		}
		nnames++;
	}
	closedir(d);

	if (nnames > 1)
		qsort(names, (size_t)nnames, sizeof(names[0]), cmp_strptr);

	for (i = 0; i < nnames; i++) {
		if (snprintf(path, sizeof(path), "%s/%s", libdir, names[i]) >=
		    (int)sizeof(path)) {
			rc = -1;
			goto done;
		}
		rc = dobytes_file(env, path);
		if (rc != 0)
			goto done;
	}
	rc = 0;
done:
	for (i = 0; i < nnames; i++)
		free(names[i]);
	return rc;
fail_names:
	for (i = 0; i < nnames; i++)
		free(names[i]);
	return -1;
}

static int load_pack_from_path(const char *path)
{
	int rc;

	if (!path || !path[0])
		return -1;
	if (!janet_inited) {
		janet_init();
		janet_inited = 1;
	}
	pack_env = janet_core_env(NULL);
	if (!pack_env) {
		pack_failed = 1;
		return -1;
	}
	capnp_janet_register(pack_env);
	seal_pack_env(pack_env);

	/* Shared pure helpers first (sibling lib/), then the pack entry. */
	if (load_pack_libs(pack_env, path) != 0) {
		pack_failed = 1;
		pack_env = NULL;
		return -1;
	}
	rc = dobytes_file(pack_env, path);
	if (rc != 0) {
		pack_failed = 1;
		pack_env = NULL;
		return -1;
	}
	pack_loaded = 1;
	pack_failed = 0;
	return 0;
}

static int load_pack_once(void)
{
	const char *path;

	if (pack_failed)
		return -1;
	if (pack_loaded)
		return 0;
	path = default_pack();
	return load_pack_from_path(path);
}

int grok_policy_shell_pack_reload_internal(const char *path)
{
	if (!path_is_absolute_file(path))
		return -1;
	unload_pack();
	if (snprintf(pack_path_buf, sizeof(pack_path_buf), "%s", path) >=
	    (int)sizeof(pack_path_buf)) {
		pack_path_buf[0] = '\0';
		pack_path_set = 0;
		return -1;
	}
	pack_path_set = 1;
	/* Keep env in sync for subprocesses / diagnostics. */
	if (setenv("GROKOS_POLICYD_JANET_PACK", pack_path_buf, 1) != 0) {
		/* Non-fatal: pack_path_buf is source of truth for this process. */
	}
	if (load_pack_from_path(pack_path_buf) != 0)
		return -2;
	return 0;
}

/* Public ABI (declared in supervisor.h). */
int grok_policy_shell_pack_reload(const char *path)
{
	int rc = grok_policy_shell_pack_reload_internal(path);

	if (rc == 0)
		return GROK_OK;
	if (rc == -1)
		return GROK_ERR_INVAL;
	return GROK_ERR_IO;
}

static int path_under_workspace(const char *workspace, const char *path)
{
	size_t wl;

	if (!workspace || !workspace[0] || !path || !path[0])
		return 0;
	if (path[0] != '/')
		return 0;
	wl = strlen(workspace);
	while (wl > 1 && workspace[wl - 1] == '/')
		wl--;
	if (strncmp(path, workspace, wl) != 0)
		return 0;
	return path[wl] == '\0' || path[wl] == '/';
}

static capn_text ctext(const char *s)
{
	capn_text t;
	size_t len;

	if (!s)
		s = "";
	len = strlen(s);
	if (len > (size_t)INT_MAX)
		len = (size_t)INT_MAX;
	t.len = (int)len;
	t.str = s;
	t.seg = NULL;
	return t;
}

static int write_flat(struct capn *c, uint8_t **out, size_t *out_len)
{
	uint8_t *buf = NULL;
	size_t cap = 8192U;
	int64_t n;

	if (!out || !out_len)
		return -1;
	*out = NULL;
	*out_len = 0;
	for (;;) {
		buf = malloc(cap);
		if (!buf)
			return -1;
		n = capn_write_mem(c, buf, cap, 0);
		if (n >= 0)
			break;
		free(buf);
		if (cap > 512U * 1024U)
			return -1;
		cap *= 2U;
	}
	*out = buf;
	*out_len = (size_t)n;
	return 0;
}

/* Host-only Cap'n PolicyDecision (code set; reason empty — pack owns reasons). */
static int build_policy_decision_code(uint16_t decision, uint16_t code,
				      uint8_t **out, size_t *out_len)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr dp;

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&d, 0, sizeof(d));
	d.decision = (enum Decision)decision;
	d.reason = ctext("");
	d.code = (enum PolicyReason)code;
	dp = new_PolicyDecision(capn_root(&c).seg);
	write_PolicyDecision(&d, dp);
	if (capn_setp(capn_root(&c), 0, dp.p) != 0) {
		capn_free(&c);
		return -1;
	}
	if (write_flat(&c, out, out_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

static int build_shell_view(const char *workspace, const char *cwd,
			    capn_ptr argv, uint8_t **flat_out, size_t *flat_len)
{
	struct capn c;
	struct ShellView view;
	ShellView_ptr root;
	int under;
	int n, i, pcount = 0;
	capn_text empty = { 0, "", NULL };
	char abs[GROK_PATH_MAX];
	struct {
		char arg[GROK_PATH_MAX];
		char resolved[GROK_PATH_MAX];
		int exists;
		char head[SHELL_HEAD_MAX];
	} probes[32];
	char argv_store[256][GROK_PATH_MAX];
	int argv_n = 0;

	if (!flat_out || !flat_len)
		return -1;
	*flat_out = NULL;
	*flat_len = 0;
	memset(probes, 0, sizeof(probes));

	under = cwd && cwd[0] && path_under_workspace(workspace, cwd);

	/* argv may arrive as an unresolved far pointer from generated readers. */
	capn_resolve(&argv);
	if (argv.type != CAPN_NULL && argv.len > 0) {
		n = argv.len;
		if (n > 256)
			n = 256;
		for (i = 0; i < n; i++) {
			capn_text t = capn_get_text(argv, i, empty);

			if (t.len > 0 && t.str && (size_t)t.len < GROK_PATH_MAX) {
				memcpy(argv_store[argv_n], t.str, (size_t)t.len);
				argv_store[argv_n][t.len] = '\0';
			} else {
				argv_store[argv_n][0] = '\0';
			}
			argv_n++;
		}
	}

	for (i = 0; i < argv_n && pcount < 32; i++) {
		const char *tok = argv_store[i];
		FILE *f;
		size_t nr;

		if (!tok[0] || tok[0] == '-')
			continue;
		if (!strchr(tok, '/') && !strchr(tok, '.'))
			continue;
		if (grok_policy_resolve_script(cwd, tok, abs, sizeof(abs)) != 0)
			continue;
		if (!path_under_workspace(workspace, abs))
			continue;
		memcpy(probes[pcount].arg, tok, strlen(tok) + 1);
		memcpy(probes[pcount].resolved, abs, strlen(abs) + 1);
		f = fopen(abs, "rb");
		if (!f) {
			probes[pcount].exists = 0;
			probes[pcount].head[0] = '\0';
			pcount++;
			continue;
		}
		probes[pcount].exists = 1;
		nr = fread(probes[pcount].head, 1, SHELL_HEAD_MAX - 1, f);
		probes[pcount].head[nr] = '\0';
		fclose(f);
		pcount++;
	}

	memset(&c, 0, sizeof(c));
	capn_init_malloc(&c);
	memset(&view, 0, sizeof(view));
	view.underWorkspace = under ? 1 : 0;
	view.cwd = ctext(cwd ? cwd : "");
	/* List(Text) is a pointer list (CAPN_PTR_LIST), not composite. */
	view.argv = capn_new_ptr_list(capn_root(&c).seg, argv_n);
	for (i = 0; i < argv_n; i++)
		capn_set_text(view.argv, i, ctext(argv_store[i]));

	view.pathProbes = new_PathProbe_list(capn_root(&c).seg, pcount);
	for (i = 0; i < pcount; i++) {
		struct PathProbe pp;

		memset(&pp, 0, sizeof(pp));
		pp.arg = ctext(probes[i].arg);
		pp.resolved = ctext(probes[i].resolved);
		pp.exists = probes[i].exists ? 1 : 0;
		pp.head = ctext(probes[i].head);
		set_PathProbe(&pp, view.pathProbes, i);
	}

	root = new_ShellView(capn_root(&c).seg);
	write_ShellView(&view, root);
	if (capn_setp(capn_root(&c), 0, root.p) != 0) {
		capn_free(&c);
		return -1;
	}
	if (write_flat(&c, flat_out, flat_len) != 0) {
		capn_free(&c);
		return -1;
	}
	capn_free(&c);
	return 0;
}

void grok_policy_shell_pack(const char *workspace, const char *cwd,
			    capn_ptr argv, uint8_t **out, size_t *out_len)
{
	uint8_t *view_flat = NULL;
	size_t view_len = 0;
	JanetBuffer *in_buf;
	Janet fnv, res, args[1];
	JanetFunction *fn;
	JanetFiber *fiber = NULL;
	JanetSignal sig;
	JanetBuffer *out_buf;
	uint8_t *copy;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;

	if (load_pack_once() != 0) {
		(void)build_policy_decision_code(0, GROK_REASON_PACK_MISSING,
						 out, out_len);
		return;
	}

	if (build_shell_view(workspace, cwd, argv, &view_flat, &view_len) != 0 ||
	    !view_flat) {
		(void)build_policy_decision_code(
			0, GROK_REASON_SHELL_VIEW_BUILD_FAILED, out, out_len);
		return;
	}

	in_buf = janet_buffer((int32_t)view_len);
	janet_buffer_push_bytes(in_buf, view_flat, (int32_t)view_len);
	free(view_flat);

	/*
	 * dobytes defines into the env proto chain; resolve, don't table_get.
	 * defn binds a Function (not a CFunction).
	 */
	{
		Janet resolved = janet_wrap_nil();
		JanetBindingType bt =
			janet_resolve(pack_env, janet_csymbol("shell-check"),
				      &resolved);

		if (bt == JANET_BINDING_NONE ||
		    !janet_checktype(resolved, JANET_FUNCTION)) {
			(void)build_policy_decision_code(
				0, GROK_REASON_PACK_MISSING, out, out_len);
			return;
		}
		fnv = resolved;
	}
	fn = janet_unwrap_function(fnv);
	args[0] = janet_wrap_buffer(in_buf);
	sig = janet_pcall(fn, 1, args, &res, &fiber);
	if (sig != JANET_SIGNAL_OK) {
		(void)build_policy_decision_code(
			0, GROK_REASON_PACK_RUNTIME_ERROR, out, out_len);
		return;
	}
	if (!janet_checktype(res, JANET_BUFFER)) {
		(void)build_policy_decision_code(0, GROK_REASON_PACK_BAD_RESULT,
						 out, out_len);
		return;
	}
	out_buf = janet_unwrap_buffer(res);
	if (out_buf->count <= 0) {
		(void)build_policy_decision_code(0, GROK_REASON_PACK_BAD_RESULT,
						 out, out_len);
		return;
	}
	copy = malloc((size_t)out_buf->count);
	if (!copy) {
		(void)build_policy_decision_code(0, GROK_REASON_PACK_BAD_RESULT,
						 out, out_len);
		return;
	}
	memcpy(copy, out_buf->data, (size_t)out_buf->count);
	*out = copy;
	*out_len = (size_t)out_buf->count;
}
