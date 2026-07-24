/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef __linux__

int grok_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len)
{
	(void)runtime_dir;
	(void)agent_id;
	if (path_out && path_len)
		path_out[0] = '\0';
	return GROK_OK;
}

int grok_cgroup_attach(const char *cgroup_path, pid_t pid)
{
	(void)cgroup_path;
	(void)pid;
	return GROK_ERR_STATE;
}

int grok_cgroup_kill(const char *cgroup_path)
{
	(void)cgroup_path;
	return GROK_ERR_STATE;
}

void grok_cgroup_remove(const char *cgroup_path)
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
	char line[GROK_PATH_MAX];

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

int grok_cgroup_create(const char *runtime_dir, const char *agent_id,
		       char *path_out, size_t path_len)
{
	char rel[GROK_PATH_MAX];
	char base[GROK_PATH_MAX];
	char path[GROK_PATH_MAX];
	char controllers[GROK_PATH_MAX];

	if (path_out && path_len)
		path_out[0] = '\0';
	if (!runtime_dir || !agent_id || !path_out || path_len == 0)
		return GROK_ERR_INVAL;
	if (self_cgroup_rel(rel, sizeof(rel)) != 0)
		return GROK_OK;

	/* Prefer a child under our current cgroup (needs subtree control / write). */
	if (rel[0] == '\0' || strcmp(rel, "/") == 0)
		snprintf(base, sizeof(base), "/sys/fs/cgroup");
	else
		snprintf(base, sizeof(base), "/sys/fs/cgroup%s", rel);

	if (snprintf(path, sizeof(path), "%s/grok-%s", base, agent_id) >= (int)sizeof(path))
		return GROK_OK;

	/* Best-effort enable controllers on parent (ignore failure). */
	if (snprintf(controllers, sizeof(controllers), "%s/cgroup.subtree_control", base) <
	    (int)sizeof(controllers)) {
		(void)write_str(controllers, "+pids +memory");
	}

	if (grok_paths_ensure_dir(path, 0755) != GROK_OK) {
		/*
		 * Fallback: mirror under runtime (still needs move into a real
		 * hierarchy — only works if runtime is on cgroupfs; usually not).
		 * Leave empty → caller uses process-group kill.
		 */
		return GROK_OK;
	}

	if (snprintf(path_out, path_len, "%s", path) >= (int)path_len) {
		path_out[0] = '\0';
		return GROK_OK;
	}
	return GROK_OK;
}

int grok_cgroup_attach(const char *cgroup_path, pid_t pid)
{
	char procs[GROK_PATH_MAX];
	char buf[32];

	if (!cgroup_path || !cgroup_path[0] || pid <= 0)
		return GROK_ERR_INVAL;
	if (snprintf(procs, sizeof(procs), "%s/cgroup.procs", cgroup_path) >= (int)sizeof(procs))
		return GROK_ERR_INVAL;
	if (snprintf(buf, sizeof(buf), "%d", (int)pid) >= (int)sizeof(buf))
		return GROK_ERR_INVAL;
	if (write_str(procs, buf) != 0)
		return GROK_ERR_IO;
	return GROK_OK;
}

int grok_cgroup_kill(const char *cgroup_path)
{
	char killp[GROK_PATH_MAX];

	if (!cgroup_path || !cgroup_path[0])
		return GROK_ERR_INVAL;
	if (snprintf(killp, sizeof(killp), "%s/cgroup.kill", cgroup_path) >= (int)sizeof(killp))
		return GROK_ERR_INVAL;
	if (write_str(killp, "1") != 0)
		return GROK_ERR_IO;
	return GROK_OK;
}

void grok_cgroup_remove(const char *cgroup_path)
{
	if (cgroup_path && cgroup_path[0])
		(void)rmdir(cgroup_path);
}

#endif /* __linux__ */
