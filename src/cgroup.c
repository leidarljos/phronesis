/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef __linux__

int phronesis_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len)
{
	(void)runtime_dir;
	(void)agent_id;
	if (path_out && path_len)
		path_out[0] = '\0';
	return PHRONESIS_OK;
}

int phronesis_cgroup_attach(const char *cgroup_path, pid_t pid)
{
	(void)cgroup_path;
	(void)pid;
	return PHRONESIS_ERR_STATE;
}

int phronesis_cgroup_kill(const char *cgroup_path)
{
	(void)cgroup_path;
	return PHRONESIS_ERR_STATE;
}

void phronesis_cgroup_remove(const char *cgroup_path)
{
	(void)cgroup_path;
}

#else /* __linux__ */

static int write_str(const char *path, const char *data)
{
	FILE *f = fopen(path, "w");

	if (!f)
		return -1;
	if (fputs(data, f) < 0) {
		fclose(f);
		return -1;
	}
	if (fclose(f) != 0)
		return -1;
	return 0;
}

static int self_cgroup_rel(char *out, size_t n)
{
	FILE *f;
	char line[PHRONESIS_PATH_MAX];

	f = fopen("/proc/self/cgroup", "r");
	if (!f)
		return -1;
	/* cgroup v2 unified: 0::/path */
	while (fgets(line, sizeof(line), f)) {
		char *p = strstr(line, "::");
		if (!p)
			continue;
		p += 2;
		/* strip newline */
		{
			size_t L = strlen(p);
			while (L > 0 && (p[L - 1] == '\n' || p[L - 1] == '\r'))
				p[--L] = '\0';
		}
		if (snprintf(out, n, "%s", p) >= (int)n) {
			fclose(f);
			return -1;
		}
		fclose(f);
		return 0;
	}
	fclose(f);
	return -1;
}

int phronesis_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len)
{
	char rel[PHRONESIS_PATH_MAX];
	char base[PHRONESIS_PATH_MAX];
	char path[PHRONESIS_PATH_MAX];
	char controllers[PHRONESIS_PATH_MAX];

	if (path_out && path_len)
		path_out[0] = '\0';
	if (!runtime_dir || !agent_id || !path_out || path_len == 0)
		return PHRONESIS_ERR_INVAL;
	if (self_cgroup_rel(rel, sizeof(rel)) != 0)
		return PHRONESIS_OK;

	/* Prefer a child under our current cgroup (needs subtree control / write). */
	if (rel[0] == '\0' || strcmp(rel, "/") == 0) {
		if (snprintf(base, sizeof(base), "/sys/fs/cgroup") >= (int)sizeof(base))
			return PHRONESIS_OK;
	} else {
		/* Bound: prefix + rel must fit base without format-truncation Werror. */
		if (strlen(rel) + sizeof("/sys/fs/cgroup") > sizeof(base))
			return PHRONESIS_OK;
		if (snprintf(base, sizeof(base), "/sys/fs/cgroup%s", rel) >= (int)sizeof(base))
			return PHRONESIS_OK;
	}

	if (strlen(base) + 1 + strlen(agent_id) + sizeof("/phronesis-") > sizeof(path))
		return PHRONESIS_OK;
	if (snprintf(path, sizeof(path), "%s/phronesis-%s", base, agent_id) >= (int)sizeof(path))
		return PHRONESIS_OK;

	/* Best-effort enable controllers on parent (ignore failure). */
	if (snprintf(controllers, sizeof(controllers), "%s/cgroup.subtree_control", base) <
	    (int)sizeof(controllers)) {
		(void)write_str(controllers, "+pids +memory");
	}

	if (phronesis_paths_ensure_dir(path, 0755) != PHRONESIS_OK) {
		/*
		 * Fallback: mirror under runtime (still needs move into a real
		 * hierarchy — only works if runtime is on cgroupfs; usually not).
		 * Leave empty → caller uses process-group kill.
		 */
		return PHRONESIS_OK;
	}

	if (snprintf(path_out, path_len, "%s", path) >= (int)path_len) {
		path_out[0] = '\0';
		return PHRONESIS_OK;
	}
	return PHRONESIS_OK;
}

int phronesis_cgroup_attach(const char *cgroup_path, pid_t pid)
{
	char procs[PHRONESIS_PATH_MAX];
	char buf[32];

	if (!cgroup_path || !cgroup_path[0] || pid <= 0)
		return PHRONESIS_ERR_INVAL;
	if (snprintf(procs, sizeof(procs), "%s/cgroup.procs", cgroup_path) >= (int)sizeof(procs))
		return PHRONESIS_ERR_INVAL;
	if (snprintf(buf, sizeof(buf), "%d", (int)pid) >= (int)sizeof(buf))
		return PHRONESIS_ERR_INVAL;
	if (write_str(procs, buf) != 0)
		return PHRONESIS_ERR_IO;
	return PHRONESIS_OK;
}

int phronesis_cgroup_kill(const char *cgroup_path)
{
	char killp[PHRONESIS_PATH_MAX];

	if (!cgroup_path || !cgroup_path[0])
		return PHRONESIS_ERR_INVAL;
	if (snprintf(killp, sizeof(killp), "%s/cgroup.kill", cgroup_path) >= (int)sizeof(killp))
		return PHRONESIS_ERR_INVAL;
	if (write_str(killp, "1") != 0)
		return PHRONESIS_ERR_IO;
	return PHRONESIS_OK;
}

void phronesis_cgroup_remove(const char *cgroup_path)
{
	if (cgroup_path && cgroup_path[0])
		(void)rmdir(cgroup_path);
}

#endif /* __linux__ */
