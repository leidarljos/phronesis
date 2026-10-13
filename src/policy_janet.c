/* SPDX-License-Identifier: MIT */
/*
 * Janet policy pack host.
 *
 * Shell: Cap'n ShellView → pack shell-check → Cap'n PolicyDecision.
 * Audio: Cap'n AudioCheck → pack audio-check → Cap'n PolicyDecision.
 * Pack authors reason text + code. Host may re-stamp agentId on audio.
 *
 * Multi-pack: PHRONESIS_JANET_PACK is a colon-separated list of pack
 * files and/or directories of top-level *.janet files. Each pack loads into
 * its own sealed env. checkShell / checkAudio run every pack that defines
 * the entry and compose fail-closed (deny > prompt > allow).
 * Pack bytes are read through phronesis_beneath_open on one pack-root fd
 * (install policy directory or PHRONESIS_PACK_ROOT; never the filesystem
 * root: "/", "//", "/.", "///").
 */
#include "policy_janet.h"
#include "internal.h"
#include "policy_trace.h"
#include "policy.capnp.h"

#include "janet.h"

#include "phronesis/supervisor.h"

#include <capnp_c.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHELL_HEAD_MAX 8192
#define SHELL_ARGV_MAX 256
#define SHELL_PROBE_MAX 32
#define PACK_PATH_MAX 4096
#define PACK_FILE_MAX (512 * 1024)
/* A pack image holds the compiled closures of every file in the pack. */
#define PACK_IMAGE_MAX (4 * 1024 * 1024)
#define PACK_LIB_MAX_FILES 32
#define PACK_MAX 16
/* Spec stores colon-joined resolved absolute/relative paths for reload/env. */
#define PACK_SPEC_MAX (PACK_PATH_MAX * PACK_MAX)
void capnp_janet_register(JanetTable *env);

typedef struct {
	JanetTable *env;
	/* Rooted Function values captured at load (local binding only). */
	Janet shell_fn;
	Janet audio_fn;
	int has_shell; /* shell-check Function bound locally on this pack */
	int has_audio; /* audio-check Function bound locally on this pack */
	char path[PACK_PATH_MAX];
} pack_slot_t;

static int janet_inited;
static int pack_loaded;
static int pack_failed;
static pack_slot_t packs[PACK_MAX];
static int npacks;
static int pack_rootfd = -1;
static char pack_root[PACK_PATH_MAX];
/* Spec string for default_pack / setenv (colon-joined). */
static char pack_spec_buf[PACK_SPEC_MAX];
static int pack_spec_set;

/*
 * Product default pack path (absolute). Meson sets this to
 * $prefix/share/phronesis/policy/shell.janet so installed seats load the
 * product pack with no env. Overridable at compile time.
 */
#ifndef PHRONESIS_DEFAULT_JANET_PACK
#define PHRONESIS_DEFAULT_JANET_PACK \
	"/usr/local/share/phronesis/policy/shell.janet"
#endif

static int path_is_file(const char *path);
static int path_is_absolute_file(const char *path);
static int path_is_absolute_dir(const char *path);

static int env_flag_on(const char *name)
{
	const char *e = getenv(name);

	if (!e || !e[0])
		return 0;
	return strcasecmp(e, "1") == 0 || strcasecmp(e, "true") == 0 ||
	       strcasecmp(e, "yes") == 0;
}

/* "/" after collapsing "/" and "." components (`/`, `//`, `/.`, `/./`). */
static int path_is_fs_root(const char *path)
{
	const char *p;

	if (!path || path[0] != '/')
		return 0;
	for (p = path; *p; p++) {
		if (*p == '/')
			continue;
		if (p[0] == '.' && (p[1] == '/' || p[1] == '\0'))
			continue;
		return 0;
	}
	return 1;
}

static int path_under_prefix(const char *path, const char *prefix)
{
	size_t n;

	if (!path || !prefix || path[0] != '/' || !prefix[0])
		return 0;
	n = strlen(prefix);
	while (n > 0 && prefix[n - 1] == '/')
		n--;
	if (n == 0)
		return 0;
	if (strncmp(path, prefix, n) != 0)
		return 0;
	return path[n] == '\0' || path[n] == '/';
}

static int pack_under_trusted_prefix(const char *path)
{
	static char prefix_root[PACK_PATH_MAX];
	const char *prefix;
	const char *def;
	const char *slash;
	const char *root;
	char dir[PACK_PATH_MAX];
	size_t n;

	if (path_under_prefix(path, "/usr/local/share/phronesis") ||
	    path_under_prefix(path, "/usr/share/phronesis"))
		return 1;
	prefix = getenv("PHRONESIS_PREFIX");
	if (prefix && prefix[0] &&
	    snprintf(prefix_root, sizeof(prefix_root), "%s/share/phronesis",
		     prefix) < (int)sizeof(prefix_root) &&
	    path_under_prefix(path, prefix_root))
		return 1;
	def = PHRONESIS_DEFAULT_JANET_PACK;
	slash = def ? strrchr(def, '/') : NULL;
	if (slash && slash > def) {
		n = (size_t)(slash - def);
		if (n < sizeof(dir)) {
			memcpy(dir, def, n);
			dir[n] = '\0';
			if (path_under_prefix(path, dir))
				return 1;
		}
	}
	root = getenv("PHRONESIS_PACK_ROOT");
	if (root && root[0] == '/' && !path_is_fs_root(root) &&
	    path_under_prefix(path, root))
		return 1;
	return 0;
}

/** File or directory segment allowed for reload (same prefixes as env). */
static int pack_reload_segment_allowed(const char *path)
{
	if (!path_is_absolute_file(path) && !path_is_absolute_dir(path))
		return 0;
	if (env_flag_on("PHRONESIS_DEV_PACK"))
		return 1;
	return pack_under_trusted_prefix(path);
}

static int pack_reload_spec_allowed(const char *spec)
{
	char buf[PACK_SPEC_MAX];
	char *save = NULL;
	char *tok;
	int any = 0;

	if (!spec || !spec[0] || strlen(spec) >= sizeof(buf))
		return 0;
	memcpy(buf, spec, strlen(spec) + 1);
	for (tok = strtok_r(buf, ":", &save); tok;
	     tok = strtok_r(NULL, ":", &save)) {
		if (!tok[0] || !pack_reload_segment_allowed(tok))
			return 0;
		any = 1;
	}
	return any;
}

/**
 * First existing pack path among product defaults.
 * Env override (PHRONESIS_JANET_PACK) is a colon list of absolute files
 * and/or directories. Each segment must sit under an allowlisted prefix
 * (or PHRONESIS_DEV_PACK=1). CWD-relative policy/shell.janet is
 * only a candidate when that dev flag is set.
 */
static const char *default_pack_spec(void)
{
	static char prefix_buf[PACK_PATH_MAX];
	const char *prefix;
	const char *cands[8];
	int n = 0;
	int i;

	if (pack_spec_set && pack_spec_buf[0])
		return pack_spec_buf;
	{
		const char *e = getenv("PHRONESIS_JANET_PACK");

		if (e && e[0] && pack_reload_spec_allowed(e))
			return e;
	}

	cands[n++] = PHRONESIS_DEFAULT_JANET_PACK;
	prefix = getenv("PHRONESIS_PREFIX");
	if (prefix && prefix[0] &&
	    snprintf(prefix_buf, sizeof(prefix_buf),
		     "%s/share/phronesis/policy/shell.janet",
		     prefix) < (int)sizeof(prefix_buf))
		cands[n++] = prefix_buf;
	cands[n++] = "/usr/local/share/phronesis/policy/shell.janet";
	cands[n++] = "/usr/share/phronesis/policy/shell.janet";
	if (env_flag_on("PHRONESIS_DEV_PACK"))
		cands[n++] = "policy/shell.janet";

	for (i = 0; i < n; i++) {
		if (cands[i] && cands[i][0] && path_is_file(cands[i]))
			return cands[i];
	}
	/* Last resort: compile-time path (fail closed at load if missing). */
	return PHRONESIS_DEFAULT_JANET_PACK;
}

static void seal_pack_env(JanetTable *env)
{
	janet_table_remove(env, janet_csymbolv("os"));
	janet_table_remove(env, janet_csymbolv("net"));
	janet_table_remove(env, janet_csymbolv("spork"));
}

/**
 * Drop loaded pack state so the next load re-reads disk.
 * Prior Janet envs are abandoned for GC (reload is rare; no janet_deinit).
 */
static void unload_packs(void);

void phronesis_policy_pack_reset(void)
{
	unload_packs();
	pack_failed = 0;
	pack_spec_set = 0;
	pack_spec_buf[0] = '\0';
}

static void unload_packs(void)
{
	int i;

	pack_loaded = 0;
	pack_failed = 0;
	for (i = 0; i < npacks; i++) {
		if (packs[i].has_shell)
			janet_gcunroot(packs[i].shell_fn);
		if (packs[i].has_audio)
			janet_gcunroot(packs[i].audio_fn);
		packs[i].shell_fn = janet_wrap_nil();
		packs[i].audio_fn = janet_wrap_nil();
		packs[i].env = NULL;
		packs[i].has_shell = 0;
		packs[i].has_audio = 0;
		packs[i].path[0] = '\0';
	}
	npacks = 0;
}

static int path_has_dotdot(const char *path)
{
	const char *p;

	if (!path)
		return 1;
	p = path;
	while (*p) {
		if (p[0] == '/' && p[1] == '.' && p[2] == '.' &&
		    (p[3] == '/' || p[3] == '\0'))
			return 1;
		/* Relative leading ".." */
		if (p == path && p[0] == '.' && p[1] == '.' &&
		    (p[2] == '/' || p[2] == '\0'))
			return 1;
		p++;
	}
	return 0;
}

static int path_is_absolute_file(const char *path)
{
	struct stat st;

	if (!path || !path[0] || path[0] != '/')
		return 0;
	if (strlen(path) >= PACK_PATH_MAX)
		return 0;
	if (path_has_dotdot(path))
		return 0;
	if (stat(path, &st) != 0)
		return 0;
	return S_ISREG(st.st_mode) ? 1 : 0;
}

static int path_is_absolute_dir(const char *path)
{
	struct stat st;

	if (!path || !path[0] || path[0] != '/')
		return 0;
	if (strlen(path) >= PACK_PATH_MAX)
		return 0;
	if (path_has_dotdot(path))
		return 0;
	if (stat(path, &st) != 0)
		return 0;
	return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int path_is_file(const char *path)
{
	struct stat st;

	if (!path || !path[0] || strlen(path) >= PACK_PATH_MAX)
		return 0;
	if (path_has_dotdot(path))
		return 0;
	if (stat(path, &st) != 0)
		return 0;
	return S_ISREG(st.st_mode) ? 1 : 0;
}

static int path_is_dir(const char *path)
{
	struct stat st;

	if (!path || !path[0] || strlen(path) >= PACK_PATH_MAX)
		return 0;
	if (path_has_dotdot(path))
		return 0;
	if (stat(path, &st) != 0)
		return 0;
	return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int default_pack_dir(char *out, size_t n)
{
	const char *p = PHRONESIS_DEFAULT_JANET_PACK;
	const char *slash = strrchr(p, '/');
	size_t len;

	if (!out || n == 0 || !slash || slash == p)
		return -1;
	len = (size_t)(slash - p);
	if (len + 1 > n)
		return -1;
	memcpy(out, p, len);
	out[len] = '\0';
	return 0;
}

static int pack_root_path(char *out, size_t n)
{
	const char *e = getenv("PHRONESIS_PACK_ROOT");

	if (e && e[0]) {
		if (e[0] != '/' || path_is_fs_root(e) || path_has_dotdot(e))
			return -1;
		if (strlen(e) >= n)
			return -1;
		memcpy(out, e, strlen(e) + 1);
		return 0;
	}
	return default_pack_dir(out, n);
}

static int ensure_pack_root(void)
{
	char root[PACK_PATH_MAX];
	int fd;

	if (pack_root_path(root, sizeof(root)) != 0)
		return -1;
	if (pack_rootfd >= 0 && strcmp(pack_root, root) == 0)
		return 0;
	if (phronesis_beneath_dir(root, &fd) != 0)
		return -1;
	if (pack_rootfd >= 0)
		(void)close(pack_rootfd);
	pack_rootfd = fd;
	if (snprintf(pack_root, sizeof(pack_root), "%s", root) >=
	    (int)sizeof(pack_root)) {
		(void)close(pack_rootfd);
		pack_rootfd = -1;
		pack_root[0] = '\0';
		return -1;
	}
	return 0;
}

static int pack_on_root(const char *path)
{
	char rel[PACK_PATH_MAX];
	int fd;

	if (ensure_pack_root() != 0)
		return 0;
	if (phronesis_beneath_rel(pack_root, path, rel, sizeof(rel)) != 0)
		return 0;
	if (phronesis_beneath_open(pack_rootfd, rel, O_RDONLY, &fd) != 0)
		return 0;
	(void)close(fd);
	return 1;
}

static int cmp_strptr(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/**
 * Append one path to @a out if capacity remains.
 * Returns 0 on success, -1 on overflow / bad path.
 */
static int append_path(char out[][PACK_PATH_MAX], int *n, int max,
		       const char *path)
{
	if (!path || !path[0] || *n >= max)
		return -1;
	if (strlen(path) >= PACK_PATH_MAX)
		return -1;
	memcpy(out[*n], path, strlen(path) + 1);
	(*n)++;
	return 0;
}

/**
 * Expand a directory to sorted top-level *.janet files (not recursive, not
 * lib/). Missing dir or empty is an error when require_exists is set.
 */
static int expand_dir_packs(const char *dir, char out[][PACK_PATH_MAX], int *n,
			    int max)
{
	char path[PACK_PATH_MAX];
	char *names[PACK_LIB_MAX_FILES];
	int nnames = 0;
	DIR *d;
	struct dirent *ent;
	int i, rc = 0;

	d = opendir(dir);
	if (!d)
		return -1;

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
			rc = -1;
			goto free_names;
		}
		nnames++;
	}
	closedir(d);

	if (nnames == 0) {
		rc = -1;
		goto free_names;
	}
	if (nnames > 1)
		qsort(names, (size_t)nnames, sizeof(names[0]), cmp_strptr);

	for (i = 0; i < nnames; i++) {
		if (snprintf(path, sizeof(path), "%s/%s", dir, names[i]) >=
		    (int)sizeof(path)) {
			rc = -1;
			break;
		}
		if (append_path(out, n, max, path) != 0) {
			rc = -1;
			break;
		}
	}
free_names:
	for (i = 0; i < nnames; i++)
		free(names[i]);
	return rc;
}

/**
 * Parse colon-separated pack spec into concrete .janet file paths.
 * Each segment is a file or a directory of top-level *.janet packs.
 * @a require_absolute: reload path (each segment must be absolute).
 * Returns 0 and *n_out > 0 on success.
 */
static int parse_pack_spec(const char *spec, char out[][PACK_PATH_MAX],
			   int *n_out, int max, int require_absolute)
{
	char buf[PACK_SPEC_MAX];
	char *save = NULL;
	char *tok;
	int n = 0;

	if (!spec || !spec[0] || !n_out || max <= 0)
		return -1;
	if (strlen(spec) >= sizeof(buf))
		return -1;
	memcpy(buf, spec, strlen(spec) + 1);
	*n_out = 0;

	for (tok = strtok_r(buf, ":", &save); tok;
	     tok = strtok_r(NULL, ":", &save)) {
		if (!tok[0])
			continue;
		if (require_absolute && tok[0] != '/')
			return -1;
		if (require_absolute && !pack_on_root(tok))
			return -1;
		if (path_is_dir(tok) ||
		    (require_absolute && path_is_absolute_dir(tok))) {
			int n_before = n;
			int i;

			if (expand_dir_packs(tok, out, &n, max) != 0)
				return -1;
			if (require_absolute) {
				for (i = n_before; i < n; i++) {
					if (!pack_on_root(out[i]))
						return -1;
				}
			}
			continue;
		}
		if (require_absolute) {
			if (!path_is_absolute_file(tok))
				return -1;
		} else if (!path_is_file(tok)) {
			return -1;
		}
		if (append_path(out, &n, max, tok) != 0)
			return -1;
	}
	if (n <= 0)
		return -1;
	*n_out = n;
	return 0;
}

/**
 * Read @a path, opened under the pack root, into a malloc buffer.
 * Returns 0 on success, -1 on I/O or size error.
 */
static int read_pack_file(const char *path, char **buf_out, size_t *len_out)
{
	char rel[PACK_PATH_MAX];
	char *buf;
	off_t sz;
	int fd;

	if (ensure_pack_root() != 0)
		return -1;
	if (phronesis_beneath_rel(pack_root, path, rel, sizeof(rel)) != 0)
		return -1;
	if (phronesis_beneath_open(pack_rootfd, rel, O_RDONLY, &fd) != 0)
		return -1;
	sz = lseek(fd, 0, SEEK_END);
	if (sz <= 0 || sz > PACK_IMAGE_MAX || lseek(fd, 0, SEEK_SET) != 0) {
		(void)close(fd);
		return -1;
	}
	buf = malloc((size_t)sz);
	if (!buf || read(fd, buf, (size_t)sz) != (ssize_t)sz) {
		free(buf);
		(void)close(fd);
		return -1;
	}
	(void)close(fd);
	*buf_out = buf;
	*len_out = (size_t)sz;
	return 0;
}

/**
 * Read @a path into the pack env via janet_dobytes.
 * Returns 0 on success, -1 on I/O/size error, -2 on Janet load error.
 */
static int dobytes_file(JanetTable *env, const char *path)
{
	char *buf;
	size_t sz;
	Janet out;
	int rc;

	if (read_pack_file(path, &buf, &sz) != 0)
		return -1;
	if (sz > PACK_FILE_MAX) {
		free(buf);
		return -1;
	}
	rc = janet_dobytes(env, (const uint8_t *)buf, (int32_t)sz, path, &out);
	free(buf);
	return rc == 0 ? 0 : -2;
}

/**
 * Sorted .janet helper names in the lib/ directory beside @a pack_path.
 * Writes that directory to @a libdir and strdup'd names to @a names.
 * A pack with no lib/ has none. Returns 0 on success, -1 on failure.
 */
static int pack_lib_names(const char *pack_path, char *libdir, size_t libdir_n,
			  char **names, int *n_out)
{
	char dir[PACK_PATH_MAX];
	int nnames = 0;
	DIR *d;
	struct dirent *ent;
	const char *slash;
	size_t dlen;

	*n_out = 0;
	libdir[0] = '\0';
	slash = strrchr(pack_path, '/');
	if (!slash || slash == pack_path)
		return 0;
	dlen = (size_t)(slash - pack_path);
	if (dlen + 1 >= sizeof(dir))
		return -1;
	memcpy(dir, pack_path, dlen);
	dir[dlen] = '\0';
	if (snprintf(libdir, libdir_n, "%s/lib", dir) >= (int)libdir_n)
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
			while (nnames > 0)
				free(names[--nnames]);
			return -1;
		}
		nnames++;
	}
	closedir(d);

	if (nnames > 1)
		qsort(names, (size_t)nnames, sizeof(names[0]), cmp_strptr);
	*n_out = nnames;
	return 0;
}

/**
 * Load sorted .janet helpers from a sibling lib/ directory, if present.
 * Missing lib/ is OK (single-file packs / allow-all test packs).
 * Returns 0 on success (or no lib), -1 on failure.
 */
static int load_pack_libs(JanetTable *env, const char *pack_path)
{
	char libdir[PACK_PATH_MAX];
	char path[PACK_PATH_MAX];
	char *names[PACK_LIB_MAX_FILES];
	int nnames = 0;
	int i, rc;

	if (pack_lib_names(pack_path, libdir, sizeof(libdir), names, &nnames) != 0)
		return -1;
	rc = 0;
	for (i = 0; i < nnames && rc == 0; i++) {
		if (snprintf(path, sizeof(path), "%s/%s", libdir, names[i]) >=
		    (int)sizeof(path))
			rc = -1;
		else
			rc = dobytes_file(env, path);
	}
	for (i = 0; i < nnames; i++)
		free(names[i]);
	return rc;
}

/**
 * Resolve a Function binding (walks proto). Used only around a single pack
 * load so "defined by this file" = appears or changes vs pre-entry snapshot.
 */
static int resolve_fn(JanetTable *env, const char *name, Janet *out_fn)
{
	Janet resolved = janet_wrap_nil();
	JanetBindingType bt;

	if (!env || !name)
		return 0;
	bt = janet_resolve(env, janet_csymbol(name), &resolved);
	if (bt == JANET_BINDING_NONE ||
	    !janet_checktype(resolved, JANET_FUNCTION))
		return 0;
	if (out_fn)
		*out_fn = resolved;
	return 1;
}

/**
 * Compile one .janet pack into a new sealed env.
 * Sets the entry Functions this file defines (shell-check, audio-check);
 * a file that defines neither is not a pack and sets both flags to 0.
 * Returns 0 on success, -1 on a load error.
 */
static int compile_pack(const char *path, Janet *shell_fn, int *has_shell,
			Janet *audio_fn, int *has_audio)
{
	JanetTable *env;
	int rc;
	Janet shell_before = janet_wrap_nil();
	Janet audio_before = janet_wrap_nil();
	int had_shell_before;
	int had_audio_before;

	*shell_fn = janet_wrap_nil();
	*audio_fn = janet_wrap_nil();
	*has_shell = 0;
	*has_audio = 0;
	env = janet_core_env(NULL);
	if (!env)
		return -1;
	capnp_janet_register(env);
	seal_pack_env(env);
	/* Optional TRACE Cfuns after seal so packs stay pure Cap'n-free. */
	PD_TRACE_REGISTER_JANET(env);

	PD_TRACE_EVENT(PD_TRACE_LAYER_PACK, PD_TRACE_PHASE_ENTER, "pack-load",
		       path, -1, NULL, 0);
	if (load_pack_libs(env, path) != 0) {
		PD_TRACE_EVENT(PD_TRACE_LAYER_PACK, PD_TRACE_PHASE_ERROR,
			       "pack-load/lib", path, -1, NULL, 0);
		return -1;
	}

	/*
	 * Snapshot after libs (shared pure helpers may not define entries) and
	 * before the pack file so we only count bindings this entry introduces.
	 */
	had_shell_before = resolve_fn(env, "shell-check", &shell_before);
	had_audio_before = resolve_fn(env, "audio-check", &audio_before);

	rc = dobytes_file(env, path);
	if (rc != 0) {
		PD_TRACE_EVENT(PD_TRACE_LAYER_PACK, PD_TRACE_PHASE_ERROR,
			       "pack-load/entry", path, -1, NULL, 0);
		return -1;
	}

	if (resolve_fn(env, "shell-check", shell_fn)) {
		if (!had_shell_before ||
		    !janet_equals(shell_before, *shell_fn))
			*has_shell = 1;
	}
	if (resolve_fn(env, "audio-check", audio_fn)) {
		if (!had_audio_before ||
		    !janet_equals(audio_before, *audio_fn))
			*has_audio = 1;
	}
	if (!*has_shell)
		*shell_fn = janet_wrap_nil();
	if (!*has_audio)
		*audio_fn = janet_wrap_nil();
	return 0;
}

/*
 * Pack image: the entry Functions of one pack, compiled and marshalled, so
 * a fresh process skips the boot image and the Janet compiler. Layout:
 * the magic line, the Janet version line, the 16-hex FNV-1a hash of the
 * pack's source files and a newline, then janet_marshal of
 * [shell-check audio-check] (nil when the file lacks one). The image sits
 * beside the pack as <name>.jimage and is opened under the pack root like
 * the sources. A missing image, a version or hash mismatch, or a failed
 * unmarshal falls back to compiling the sources.
 */
#define PACK_IMAGE_MAGIC "phronesis-pack-image 1\n"

static uint64_t fnv1a_feed(uint64_t h, const void *p, size_t n)
{
	const unsigned char *b = p;
	size_t i;

	for (i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

/** <pack>.jimage for <pack>.janet. Returns 0, or -1 when the name is not a pack. */
static int pack_image_path(const char *path, char *out, size_t n)
{
	size_t len = strlen(path);

	if (len < 7 || strcmp(path + len - 6, ".janet") != 0)
		return -1;
	if (len - 6 + sizeof(".jimage") > n)
		return -1;
	memcpy(out, path, len - 6);
	memcpy(out + len - 6, ".jimage", sizeof(".jimage"));
	return 0;
}

/**
 * Hash every source the pack compiles from: each lib/ helper in load
 * order, then the entry file, each as its name, a NUL and its bytes.
 * Returns 0 on success, -1 when a source cannot be read.
 */
static int pack_source_hash(const char *path, uint64_t *out)
{
	char libdir[PACK_PATH_MAX];
	char file[PACK_PATH_MAX];
	char *names[PACK_LIB_MAX_FILES];
	int nnames = 0;
	uint64_t h = 0xcbf29ce484222325ULL;
	int i, rc = 0;

	if (pack_lib_names(path, libdir, sizeof(libdir), names, &nnames) != 0)
		return -1;
	for (i = 0; i <= nnames && rc == 0; i++) {
		const char *name;
		char *buf;
		size_t len;

		if (i < nnames) {
			if (snprintf(file, sizeof(file), "%s/%s", libdir,
				     names[i]) >= (int)sizeof(file)) {
				rc = -1;
				break;
			}
			name = names[i];
		} else {
			if (snprintf(file, sizeof(file), "%s", path) >=
			    (int)sizeof(file)) {
				rc = -1;
				break;
			}
			name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
		}
		if (read_pack_file(file, &buf, &len) != 0) {
			rc = -1;
			break;
		}
		h = fnv1a_feed(h, name, strlen(name) + 1);
		h = fnv1a_feed(h, buf, len);
		free(buf);
	}
	for (i = 0; i < nnames; i++)
		free(names[i]);
	if (rc == 0)
		*out = h;
	return rc;
}

/** The Cap'n (and TRACE) Cfuns a pack env carries, as an env of bindings. */
static JanetTable *pack_cfun_env(void)
{
	JanetTable *t = janet_table(0);

	capnp_janet_register(t);
	PD_TRACE_REGISTER_JANET(t);
	return t;
}

/** Symbol to value for every Cfun an image may name. */
static JanetTable *pack_image_forward(void)
{
	JanetTable *fwd = janet_core_lookup_table(NULL);

	janet_env_lookup_into(fwd, pack_cfun_env(), NULL, 0);
	return fwd;
}

/** Value to symbol: the inverse of pack_image_forward. */
static JanetTable *pack_image_reverse(void)
{
	JanetTable *fwd = pack_image_forward();
	JanetTable *rev = janet_table(fwd->count);
	int32_t i;

	for (i = 0; i < fwd->capacity; i++) {
		JanetKV kv = fwd->data[i];

		if (!janet_checktype(kv.key, JANET_NIL))
			janet_table_put(rev, kv.value, kv.key);
	}
	return rev;
}

/** janet_unmarshal that returns -1 instead of panicking on a bad image. */
static int unmarshal_guarded(const uint8_t *bytes, size_t len, Janet *out)
{
	JanetTryState st;
	volatile int rc = -1;

	if (!janet_try(&st)) {
		*out = janet_unmarshal(bytes, len, 0, pack_image_forward(), NULL);
		rc = 0;
	}
	janet_restore(&st);
	return rc;
}

/** janet_marshal that returns -1 instead of panicking on an unnamed Cfun. */
static int marshal_guarded(JanetBuffer *b, Janet x)
{
	JanetTryState st;
	volatile int rc = -1;

	if (!janet_try(&st)) {
		janet_marshal(b, x, pack_image_reverse(), 0);
		rc = 0;
	}
	janet_restore(&st);
	return rc;
}

/** The header an image of the pack at @a path must start with. */
static int pack_image_header(const char *path, char *out, size_t n)
{
	uint64_t h;

	if (pack_source_hash(path, &h) != 0)
		return -1;
	if (snprintf(out, n, "%s%s\n%016llx\n", PACK_IMAGE_MAGIC, JANET_VERSION,
		     (unsigned long long)h) >= (int)n)
		return -1;
	return 0;
}

/**
 * Load the entry Functions from the image beside @a path.
 * Returns 0 on success, -1 when there is no usable image.
 */
static int load_pack_image(const char *path, Janet *shell_fn, int *has_shell,
			   Janet *audio_fn, int *has_audio)
{
	char ipath[PACK_PATH_MAX];
	char header[256];
	char *buf;
	size_t len, hl;
	Janet v = janet_wrap_nil();
	int ok;

	if (pack_image_path(path, ipath, sizeof(ipath)) != 0)
		return -1;
	if (!pack_on_root(ipath))
		return -1;
	if (pack_image_header(path, header, sizeof(header)) != 0)
		return -1;
	if (read_pack_file(ipath, &buf, &len) != 0)
		return -1;
	hl = strlen(header);
	if (len <= hl || memcmp(buf, header, hl) != 0) {
		free(buf);
		return -1;
	}
	ok = unmarshal_guarded((const uint8_t *)buf + hl, len - hl, &v) == 0;
	free(buf);
	if (!ok || !janet_checktype(v, JANET_TUPLE) ||
	    janet_tuple_length(janet_unwrap_tuple(v)) != 2)
		return -1;
	*shell_fn = janet_unwrap_tuple(v)[0];
	*audio_fn = janet_unwrap_tuple(v)[1];
	*has_shell = janet_checktype(*shell_fn, JANET_FUNCTION);
	*has_audio = janet_checktype(*audio_fn, JANET_FUNCTION);
	if (!*has_shell && !janet_checktype(*shell_fn, JANET_NIL))
		return -1;
	if (!*has_audio && !janet_checktype(*audio_fn, JANET_NIL))
		return -1;
	return (*has_shell || *has_audio) ? 0 : -1;
}

int phronesis_pack_image_write(const char *path, const char *out_path)
{
	char header[256];
	Janet shell_fn, audio_fn, pair[2];
	int has_shell, has_audio;
	JanetBuffer *b;
	FILE *f;

	if (!path || !out_path)
		return -1;
	if (!janet_inited) {
		janet_init();
		janet_inited = 1;
	}
	if (compile_pack(path, &shell_fn, &has_shell, &audio_fn, &has_audio) != 0)
		return -1;
	if (!has_shell && !has_audio)
		return -1;
	if (pack_image_header(path, header, sizeof(header)) != 0)
		return -1;
	pair[0] = shell_fn;
	pair[1] = audio_fn;
	b = janet_buffer(0);
	janet_buffer_push_cstring(b, header);
	if (marshal_guarded(b, janet_wrap_tuple(janet_tuple_n(pair, 2))) != 0)
		return -1;
	f = fopen(out_path, "wb");
	if (!f)
		return -1;
	if (fwrite(b->data, 1, (size_t)b->count, f) != (size_t)b->count) {
		fclose(f);
		return -1;
	}
	return fclose(f) == 0 ? 0 : -1;
}

/**
 * Load one .janet pack and register a slot: from its image when one
 * matches the sources, else by compiling it into a new sealed env.
 * Files without a new local shell-check/audio-check are skipped (0) so packs.d
 * can hold non-entry helpers/docs. Entry Functions are GC-rooted at capture so
 * a later pack that redefines the same name cannot change earlier slots.
 * Does not set pack_loaded.
 */
static int load_one_pack(const char *path)
{
	Janet shell_fn = janet_wrap_nil();
	Janet audio_fn = janet_wrap_nil();
	int has_shell = 0;
	int has_audio = 0;

	if (!path || !path[0] || npacks >= PACK_MAX)
		return -1;
	if (!janet_inited) {
		janet_init();
		janet_inited = 1;
	}
	if (load_pack_image(path, &shell_fn, &has_shell, &audio_fn,
			    &has_audio) != 0 &&
	    compile_pack(path, &shell_fn, &has_shell, &audio_fn, &has_audio) != 0)
		return -1;
	/* Non-entry .janet (docs / layout tables) is not a pack. */
	if (!has_shell && !has_audio)
		return 0;

	packs[npacks].env = NULL;
	packs[npacks].shell_fn = shell_fn;
	packs[npacks].audio_fn = audio_fn;
	packs[npacks].has_shell = has_shell;
	packs[npacks].has_audio = has_audio;
	if (has_shell)
		janet_gcroot(packs[npacks].shell_fn);
	if (has_audio)
		janet_gcroot(packs[npacks].audio_fn);
	if (snprintf(packs[npacks].path, sizeof(packs[npacks].path), "%s",
		     path) >= (int)sizeof(packs[npacks].path)) {
		if (has_shell)
			janet_gcunroot(packs[npacks].shell_fn);
		if (has_audio)
			janet_gcunroot(packs[npacks].audio_fn);
		return -1;
	}
	npacks++;
	PD_TRACE_EVENT(PD_TRACE_LAYER_PACK, PD_TRACE_PHASE_GATE, "pack-load",
		       path, -1, NULL, 0);
	return 0;
}

/**
 * Load all packs from a colon-separated spec. Replaces any prior load.
 * @a require_absolute: 1 for reload/Cap'n path; 0 for env/default (relative OK).
 * Returns 0 on success, -1 invalid, -2 load failed.
 */
static int load_packs_from_spec(const char *spec, int require_absolute)
{
	char paths[PACK_MAX][PACK_PATH_MAX];
	int n = 0;
	int i;
	size_t spec_len = 0;

	if (!spec || !spec[0])
		return -1;
	/* Accept the spec before teardown so a rejected path cannot drop law. */
	if (parse_pack_spec(spec, paths, &n, PACK_MAX, require_absolute) != 0)
		return -1;
	unload_packs();
	PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_ENTER, "multi-pack-load",
		       spec, n, NULL, 0);
	for (i = 0; i < n; i++) {
		if (load_one_pack(paths[i]) != 0) {
			unload_packs();
			pack_failed = 1;
			return -2;
		}
	}
	if (npacks <= 0) {
		unload_packs();
		pack_failed = 1;
		return -2;
	}
	/* Join resolved file list into pack_spec_buf for diagnostics / setenv. */
	pack_spec_buf[0] = '\0';
	for (i = 0; i < npacks; i++) {
		size_t pl = strlen(packs[i].path);
		size_t need = pl + (i > 0 ? 1 : 0);

		if (spec_len + need + 1 >= sizeof(pack_spec_buf)) {
			unload_packs();
			pack_failed = 1;
			return -2;
		}
		if (i > 0)
			pack_spec_buf[spec_len++] = ':';
		memcpy(pack_spec_buf + spec_len, packs[i].path, pl);
		spec_len += pl;
		pack_spec_buf[spec_len] = '\0';
	}
	pack_spec_set = 1;
	pack_loaded = 1;
	pack_failed = 0;
	PD_TRACE_EVENT(PD_TRACE_LAYER_HOST, PD_TRACE_PHASE_GATE, "multi-pack-load",
		       pack_spec_buf, npacks, NULL, 0);
	return 0;
}

static int load_pack_once(void)
{
	const char *spec;

	if (pack_failed)
		return -1;
	if (pack_loaded)
		return 0;
	spec = default_pack_spec();
	/* Relative only when PHRONESIS_DEV_PACK selected the cwd candidate. */
	return load_packs_from_spec(spec, 0) == 0 ? 0 : -1;
}

int phronesis_shell_pack_reload_internal(const char *path)
{
	int rc;

	/* path is a colon-separated list of absolute files and/or directories. */
	if (!path || !path[0])
		return -1;
	/* Reject before unload so a failed attack cannot drop the loaded pack. */
	if (!pack_reload_spec_allowed(path))
		return -1;
	rc = load_packs_from_spec(path, 1);
	if (rc != 0) {
		/* -1: spec rejected before unload. Live packs stay. */
		if (rc == -2) {
			pack_spec_buf[0] = '\0';
			pack_spec_set = 0;
		}
		return rc;
	}
	/* Keep env in sync for subprocesses / diagnostics. */
	if (setenv("PHRONESIS_JANET_PACK", pack_spec_buf, 1) != 0) {
		/* Non-fatal: pack_spec_buf is source of truth for this process. */
	}
	return 0;
}

/* Public ABI (declared in supervisor.h). */
int phronesis_shell_pack_reload(const char *path)
{
	int rc = phronesis_shell_pack_reload_internal(path);

	if (rc == 0)
		return PHRONESIS_OK;
	if (rc == -1)
		return PHRONESIS_ERR_INVAL;
	return PHRONESIS_ERR_IO;
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

struct shell_path_probe {
	char arg[PHRONESIS_PATH_MAX];
	char resolved[PHRONESIS_PATH_MAX];
	int exists;
	char head[SHELL_HEAD_MAX];
};

int phronesis_build_shell_view(const char *workspace, const char *cwd,
			       capn_ptr argv, uint8_t **flat_out,
			       size_t *flat_len)
{
	struct capn c;
	struct ShellView view;
	ShellView_ptr root;
	int under;
	int n, i, pcount = 0;
	int probe_cap = 0;
	int rc = -1;
	int capn_live = 0;
	capn_text empty = { 0, "", NULL };
	char abs[PHRONESIS_PATH_MAX];
	/* Heap: musl default thread stacks are 128 KB. */
	struct shell_path_probe *probes = NULL;
	char (*argv_store)[PHRONESIS_PATH_MAX] = NULL;
	int argv_n = 0;

	if (!flat_out || !flat_len)
		return -1;
	*flat_out = NULL;
	*flat_len = 0;

	under = cwd && cwd[0] && path_under_workspace(workspace, cwd);

	/* argv may arrive as an unresolved far pointer from generated readers. */
	capn_resolve(&argv);
	if (argv.type != CAPN_NULL && argv.len > 0) {
		n = argv.len;
		if (n > SHELL_ARGV_MAX) {
			rc = -2;
			goto out;
		}
		argv_store = calloc((size_t)n, sizeof(*argv_store));
		if (!argv_store)
			goto out;
		for (i = 0; i < n; i++) {
			capn_text t = capn_get_text(argv, i, empty);

			if ((size_t)t.len >= PHRONESIS_PATH_MAX) {
				rc = -2;
				goto out;
			}
			if (t.len > 0 && t.str) {
				memcpy(argv_store[i], t.str, (size_t)t.len);
				argv_store[i][t.len] = '\0';
			}
			argv_n++;
		}
	}

	if (argv_n > 0) {
		probe_cap = argv_n < SHELL_PROBE_MAX ? argv_n : SHELL_PROBE_MAX;
		probes = calloc((size_t)probe_cap, sizeof(*probes));
		if (!probes)
			goto out;
	}

	for (i = 0; i < argv_n && pcount < probe_cap; i++) {
		const char *tok = argv_store[i];
		FILE *f;
		size_t nr;

		if (!tok[0] || tok[0] == '-')
			continue;
		if (!strchr(tok, '/') && !strchr(tok, '.'))
			continue;
		if (phronesis_resolve_script(cwd, tok, abs, sizeof(abs)) != 0)
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
	capn_live = 1;
	memset(&view, 0, sizeof(view));
	view.underWorkspace = under ? 1 : 0;
	view.cwd = ctext(cwd ? cwd : "");
	{
		capn_ptr *argv = phronesis_argv_slot(view.argv);

		*argv = capn_new_ptr_list(capn_root(&c).seg, argv_n);
		for (i = 0; i < argv_n; i++)
			capn_set_text(*argv, i, ctext(argv_store[i]));
	}

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
	if (capn_setp(capn_root(&c), 0, root.p) != 0)
		goto out;
	if (write_flat(&c, flat_out, flat_len) != 0)
		goto out;
	rc = 0;
out:
	if (capn_live)
		capn_free(&c);
	free(probes);
	free(argv_store);
	return rc;
}

/**
 * Rank for fail-closed composition: higher wins.
 * deny (0) > prompt (2) > allow (1). Unknown treated as deny.
 */
static int decision_rank(int decision)
{
	if (decision == (int)PHRONESIS_DECISION_DENY)
		return 3;
	if (decision == (int)PHRONESIS_DECISION_PROMPT)
		return 2;
	if (decision == (int)PHRONESIS_DECISION_ALLOW)
		return 1;
	return 3;
}

static int read_decision_fields(const uint8_t *buf, size_t len, int *dec_out,
				int *code_out)
{
	struct capn c;
	struct PolicyDecision d;
	PolicyDecision_ptr root;

	if (!buf || len == 0 || !dec_out || !code_out)
		return -1;
	if (capn_init_mem(&c, buf, len, 0) != 0)
		return -1;
	root.p = capn_getp(capn_root(&c), 0, 1);
	read_PolicyDecision(&d, root);
	*dec_out = (int)d.decision;
	*code_out = (int)d.code;
	capn_free(&c);
	return 0;
}

/**
 * Invoke a rooted pack entry Function with Cap'n bytes in a Janet buffer.
 * On success writes malloc'd Cap'n PolicyDecision to *out.
 * Returns 0 on success; -1 missing binding; -2 runtime; -3 bad result.
 */
static int pack_pcall_fn(Janet fnv, const uint8_t *in, size_t in_len,
			 uint8_t **out, size_t *out_len)
{
	JanetBuffer *in_buf;
	Janet res, args[1];
	JanetFunction *fn;
	JanetFiber *fiber = NULL;
	JanetSignal sig;
	JanetBuffer *out_buf;
	uint8_t *copy;

	*out = NULL;
	*out_len = 0;
	if (!janet_checktype(fnv, JANET_FUNCTION) || !in || in_len == 0)
		return -1;

	fn = janet_unwrap_function(fnv);
	in_buf = janet_buffer((int32_t)in_len);
	janet_buffer_push_bytes(in_buf, in, (int32_t)in_len);
	args[0] = janet_wrap_buffer(in_buf);
	sig = janet_pcall(fn, 1, args, &res, &fiber);
	if (sig != JANET_SIGNAL_OK)
		return -2;
	if (!janet_checktype(res, JANET_BUFFER))
		return -3;
	out_buf = janet_unwrap_buffer(res);
	if (out_buf->count <= 0)
		return -3;
	copy = malloc((size_t)out_buf->count);
	if (!copy)
		return -3;
	memcpy(copy, out_buf->data, (size_t)out_buf->count);
	*out = copy;
	*out_len = (size_t)out_buf->count;
	return 0;
}

/**
 * Run every loaded pack that defines @a entry; compose fail-closed.
 * deny short-circuits. Otherwise keep highest-rank decision (prompt > allow).
 */
static void compose_pack_entry(const char *entry, int need_flag_shell,
			       const uint8_t *in, size_t in_len, uint8_t **out,
			       size_t *out_len)
{
	uint8_t *best = NULL;
	size_t best_len = 0;
	int best_rank = 0;
	int i;
	int any = 0;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;

	if (load_pack_once() != 0) {
		(void)build_policy_decision_code(0, PHRONESIS_REASON_PACK_MISSING,
						 out, out_len);
		return;
	}

	(void)entry; /* entry name fixed by need_flag_shell (shell vs audio) */

	for (i = 0; i < npacks; i++) {
		uint8_t *one = NULL;
		size_t one_len = 0;
		int rc;
		int dec = 0;
		int code = 0;
		int rank;
		Janet fnv;

		if (need_flag_shell) {
			if (!packs[i].has_shell)
				continue;
			fnv = packs[i].shell_fn;
		} else {
			if (!packs[i].has_audio)
				continue;
			fnv = packs[i].audio_fn;
		}
		any = 1;
		rc = pack_pcall_fn(fnv, in, in_len, &one, &one_len);
		if (rc == -1) {
			free(one);
			free(best);
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_PACK_MISSING, out, out_len);
			return;
		}
		if (rc == -2) {
			free(one);
			free(best);
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_PACK_RUNTIME_ERROR, out,
				out_len);
			return;
		}
		if (rc != 0 || !one) {
			free(one);
			free(best);
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_PACK_BAD_RESULT, out, out_len);
			return;
		}
		if (read_decision_fields(one, one_len, &dec, &code) != 0) {
			free(one);
			free(best);
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_PACK_BAD_RESULT, out, out_len);
			return;
		}
		rank = decision_rank(dec);
		/* Deny short-circuit: first deny wins. */
		if (dec == (int)PHRONESIS_DECISION_DENY) {
			free(best);
			*out = one;
			*out_len = one_len;
			return;
		}
		if (rank > best_rank) {
			free(best);
			best = one;
			best_len = one_len;
			best_rank = rank;
		} else {
			free(one);
		}
	}

	if (!any || !best) {
		free(best);
		(void)build_policy_decision_code(0, PHRONESIS_REASON_PACK_MISSING,
						 out, out_len);
		return;
	}
	*out = best;
	*out_len = best_len;
}

void phronesis_shell_pack(const char *workspace, const char *cwd,
			    capn_ptr argv, uint8_t **out, size_t *out_len)
{
	uint8_t *view_flat = NULL;
	size_t view_len = 0;

	if (!out || !out_len)
		return;
	*out = NULL;
	*out_len = 0;

	if (load_pack_once() != 0) {
		(void)build_policy_decision_code(0, PHRONESIS_REASON_PACK_MISSING,
						 out, out_len);
		return;
	}

	{
		int view_rc = phronesis_build_shell_view(
			workspace, cwd, argv, &view_flat, &view_len);

		if (view_rc == -2) {
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_FIELD_TOO_LONG, out, out_len);
			return;
		}
		if (view_rc != 0 || !view_flat) {
			(void)build_policy_decision_code(
				0, PHRONESIS_REASON_SHELL_VIEW_BUILD_FAILED, out,
				out_len);
			return;
		}
	}

	compose_pack_entry("shell-check", 1, view_flat, view_len, out, out_len);
	free(view_flat);
}

void phronesis_audio_pack(const uint8_t *in, size_t in_len, uint8_t **out,
			    size_t *out_len)
{
	/* Cap'n AudioCheck → every pack with audio-check → compose. */
	compose_pack_entry("audio-check", 0, in, in_len, out, out_len);
}
