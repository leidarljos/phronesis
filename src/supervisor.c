/* SPDX-License-Identifier: MIT */
#include "internal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PHRONESIS_MAX_AGENTS 16

typedef struct {
	int in_use;
	char id[PHRONESIS_ID_MAX];
	phronesis_agent_state_t state;
	pid_t pid;
	pid_t pgid;
	int exit_status;
	char mode[PHRONESIS_MODE_MAX];
	char workspace[PHRONESIS_PATH_MAX];
	char cgroup_path[PHRONESIS_PATH_MAX];
} agent_slot_t;

struct phronesis_supervisor {
	char state_dir[PHRONESIS_PATH_MAX];
	char runtime_dir[PHRONESIS_PATH_MAX];
	char action_log[PHRONESIS_PATH_MAX];
	char agents_dir[PHRONESIS_PATH_MAX];
	agent_slot_t agents[PHRONESIS_MAX_AGENTS];
};

static int valid_id(const char *id)
{
	size_t i;

	if (!id || !id[0] || strlen(id) >= PHRONESIS_ID_MAX)
		return 0;
	for (i = 0; id[i]; i++) {
		char c = id[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '-' || c == '_')
			continue;
		return 0;
	}
	return 1;
}

static int valid_token(const char *s, size_t maxlen)
{
	size_t i;

	if (!s || !s[0] || strlen(s) >= maxlen)
		return 0;
	for (i = 0; s[i]; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c <= 0x20 || c == 0x7f)
			return 0;
	}
	return 1;
}

static int slot_path(const phronesis_supervisor_t *s, const char *id, char *out, size_t n)
{
	int r = snprintf(out, n, "%s/%s.slot", s->agents_dir, id);

	if (r < 0 || (size_t)r >= n)
		return PHRONESIS_ERR_INVAL;
	return PHRONESIS_OK;
}

static int write_slot(const phronesis_supervisor_t *s, const agent_slot_t *a)
{
	char path[PHRONESIS_PATH_MAX];
	char tmp[PHRONESIS_PATH_MAX];
	FILE *f;

	if (slot_path(s, a->id, path, sizeof(path)) != PHRONESIS_OK)
		return PHRONESIS_ERR_INVAL;
	if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
		return PHRONESIS_ERR_INVAL;
	f = fopen(tmp, "w");
	if (!f)
		return PHRONESIS_ERR_IO;
	if (fprintf(f, "%s %d %d %d %d %s %s\n",
		    a->id, (int)a->state, (int)a->pid, (int)a->pgid, a->exit_status,
		    a->mode[0] ? a->mode : "-",
		    a->workspace[0] ? a->workspace : "-") < 0) {
		fclose(f);
		unlink(tmp);
		return PHRONESIS_ERR_IO;
	}
	if (fclose(f) != 0) {
		unlink(tmp);
		return PHRONESIS_ERR_IO;
	}
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return PHRONESIS_ERR_IO;
	}
	return PHRONESIS_OK;
}

static int read_slot_file(const phronesis_supervisor_t *s, const char *id, agent_slot_t *a)
{
	char path[PHRONESIS_PATH_MAX];
	FILE *f;
	int state, pid, pgid, exit_status;
	char mode[PHRONESIS_MODE_MAX];
	char workspace[PHRONESIS_PATH_MAX];
	char got_id[PHRONESIS_ID_MAX];

	if (slot_path(s, id, path, sizeof(path)) != PHRONESIS_OK)
		return PHRONESIS_ERR_INVAL;
	f = fopen(path, "r");
	if (!f)
		return PHRONESIS_ERR_NOTFOUND;
	memset(a, 0, sizeof(*a));
	if (fscanf(f, "%63s %d %d %d %d %31s %511s",
		   got_id, &state, &pid, &pgid, &exit_status, mode, workspace) != 7) {
		fclose(f);
		return PHRONESIS_ERR_IO;
	}
	fclose(f);
	if (strcmp(got_id, id) != 0)
		return PHRONESIS_ERR_IO;
	a->in_use = 1;
	snprintf(a->id, sizeof(a->id), "%s", got_id);
	a->state = (phronesis_agent_state_t)state;
	a->pid = (pid_t)pid;
	a->pgid = (pid_t)pgid;
	a->exit_status = exit_status;
	if (strcmp(mode, "-") != 0)
		snprintf(a->mode, sizeof(a->mode), "%s", mode);
	if (strcmp(workspace, "-") != 0)
		snprintf(a->workspace, sizeof(a->workspace), "%s", workspace);
	return PHRONESIS_OK;
}

static agent_slot_t *find_mem(phronesis_supervisor_t *s, const char *id)
{
	int i;

	for (i = 0; i < PHRONESIS_MAX_AGENTS; i++) {
		if (s->agents[i].in_use && strcmp(s->agents[i].id, id) == 0)
			return &s->agents[i];
	}
	return NULL;
}

static agent_slot_t *alloc_mem(phronesis_supervisor_t *s)
{
	int i;

	for (i = 0; i < PHRONESIS_MAX_AGENTS; i++) {
		if (!s->agents[i].in_use) {
			memset(&s->agents[i], 0, sizeof(s->agents[i]));
			s->agents[i].in_use = 1;
			s->agents[i].exit_status = -1;
			return &s->agents[i];
		}
	}
	return NULL;
}

static int load_agent(phronesis_supervisor_t *s, const char *id, agent_slot_t **out)
{
	agent_slot_t *a = find_mem(s, id);
	agent_slot_t disk;
	int rc;

	if (a) {
		*out = a;
		return PHRONESIS_OK;
	}
	rc = read_slot_file(s, id, &disk);
	if (rc != PHRONESIS_OK)
		return rc;
	a = alloc_mem(s);
	if (!a)
		return PHRONESIS_ERR_STATE;
	*a = disk;
	*out = a;
	return PHRONESIS_OK;
}

static void reap_slot(agent_slot_t *a)
{
	int st;
	pid_t r;

	if (!a || a->state != PHRONESIS_AGENT_RUNNING || a->pid <= 0)
		return;
	if (kill(a->pid, 0) != 0 && errno == ESRCH) {
		r = waitpid(a->pid, &st, WNOHANG);
		a->exit_status = (r == a->pid) ? st : -1;
		a->state = PHRONESIS_AGENT_STOPPED;
		a->pid = 0;
		return;
	}
	r = waitpid(a->pid, &st, WNOHANG);
	if (r == a->pid) {
		a->exit_status = st;
		a->state = PHRONESIS_AGENT_STOPPED;
		a->pid = 0;
	}
}

static int kill_tree(pid_t pgid)
{
	int st;
	int tries;

	if (pgid <= 1)
		return PHRONESIS_ERR_INVAL;

	(void)kill(-pgid, SIGTERM);
	for (tries = 0; tries < 50; tries++) {
		if (kill(-pgid, 0) != 0 && errno == ESRCH)
			break;
		(void)waitpid(-pgid, &st, WNOHANG);
		usleep(10 * 1000);
	}
	if (kill(-pgid, 0) == 0) {
		(void)kill(-pgid, SIGKILL);
		for (tries = 0; tries < 50; tries++) {
			if (kill(-pgid, 0) != 0 && errno == ESRCH)
				break;
			(void)waitpid(-pgid, &st, WNOHANG);
			usleep(10 * 1000);
		}
	}
	while (waitpid(-pgid, &st, WNOHANG) > 0) {
	}
	return PHRONESIS_OK;
}

int phronesis_supervisor_open(phronesis_supervisor_t **out,
			 const char *state_dir,
			 const char *runtime_dir)
{
	phronesis_supervisor_t *s;
	int rc;

	if (!out)
		return PHRONESIS_ERR_INVAL;
	*out = NULL;
	s = calloc(1, sizeof(*s));
	if (!s)
		return PHRONESIS_ERR_IO;
	rc = phronesis_paths_resolve(s->state_dir, sizeof(s->state_dir),
				s->runtime_dir, sizeof(s->runtime_dir),
				s->action_log, sizeof(s->action_log),
				state_dir, runtime_dir);
	if (rc != PHRONESIS_OK) {
		free(s);
		return rc;
	}
	if (snprintf(s->agents_dir, sizeof(s->agents_dir), "%s/agents",
		     s->runtime_dir) >= (int)sizeof(s->agents_dir)) {
		free(s);
		return PHRONESIS_ERR_INVAL;
	}
	if (phronesis_paths_ensure_dir(s->agents_dir, 0700) != PHRONESIS_OK) {
		free(s);
		return PHRONESIS_ERR_IO;
	}
	*out = s;
	return PHRONESIS_OK;
}

void phronesis_supervisor_close(phronesis_supervisor_t *s)
{
	free(s);
}

const char *phronesis_supervisor_action_log_path(const phronesis_supervisor_t *s)
{
	return s ? s->action_log : NULL;
}

const char *phronesis_supervisor_state_dir(const phronesis_supervisor_t *s)
{
	return s ? s->state_dir : NULL;
}

const char *phronesis_supervisor_runtime_dir(const phronesis_supervisor_t *s)
{
	return s ? s->runtime_dir : NULL;
}

int phronesis_supervisor_start(phronesis_supervisor_t *s,
			  const char *agent_id,
			  const char *mode,
			  const char *workspace,
			  char *const argv[])
{
	agent_slot_t *a;
	pid_t pid;
	char detail[PHRONESIS_DETAIL_MAX];
	int rc;

	if (!s || !argv || !argv[0] || !valid_id(agent_id))
		return PHRONESIS_ERR_INVAL;
	if (mode && mode[0] && !valid_token(mode, PHRONESIS_MODE_MAX))
		return PHRONESIS_ERR_INVAL;
	if (workspace && workspace[0] && !valid_token(workspace, PHRONESIS_PATH_MAX))
		return PHRONESIS_ERR_INVAL;

	rc = load_agent(s, agent_id, &a);
	if (rc == PHRONESIS_OK) {
		reap_slot(a);
		if (a->state == PHRONESIS_AGENT_RUNNING)
			return PHRONESIS_ERR_EXISTS;
	} else if (rc == PHRONESIS_ERR_NOTFOUND) {
		a = alloc_mem(s);
		if (!a)
			return PHRONESIS_ERR_STATE;
		snprintf(a->id, sizeof(a->id), "%s", agent_id);
	} else {
		return rc;
	}

	pid = fork();
	if (pid < 0)
		return PHRONESIS_ERR_SPAWN;
	if (pid == 0) {
		if (setpgid(0, 0) != 0)
			_exit(127);
		execvp(argv[0], argv);
		_exit(127);
	}
	(void)setpgid(pid, pid);

	a->pid = pid;
	a->pgid = pid;
	a->state = PHRONESIS_AGENT_RUNNING;
	a->exit_status = -1;
	a->cgroup_path[0] = '\0';
	if (mode && mode[0])
		snprintf(a->mode, sizeof(a->mode), "%s", mode);
	else
		snprintf(a->mode, sizeof(a->mode), "develop");
	if (workspace && workspace[0])
		snprintf(a->workspace, sizeof(a->workspace), "%s", workspace);
	else
		a->workspace[0] = '\0';

	/* Best-effort cgroup placement; process-group kill remains the baseline. */
	if (phronesis_cgroup_create(s->runtime_dir, agent_id, a->cgroup_path,
			       sizeof(a->cgroup_path)) == PHRONESIS_OK &&
	    a->cgroup_path[0]) {
		if (phronesis_cgroup_attach(a->cgroup_path, pid) != PHRONESIS_OK)
			a->cgroup_path[0] = '\0';
	}

	if (write_slot(s, a) != PHRONESIS_OK) {
		(void)kill_tree(pid);
		if (a->cgroup_path[0])
			phronesis_cgroup_remove(a->cgroup_path);
		a->state = PHRONESIS_AGENT_FAILED;
		a->pid = 0;
		a->cgroup_path[0] = '\0';
		(void)write_slot(s, a);
		return PHRONESIS_ERR_IO;
	}

	{
		const char *cmd = argv[0] ? argv[0] : "?";
		const char *cg = a->cgroup_path[0] ? a->cgroup_path : "none";
		/* Bounded detail for action log (avoid -Wformat-truncation). */
		snprintf(detail, sizeof(detail), "pid=%d pgid=%d cmd=%.64s cgroup=%.128s",
			 (int)pid, (int)pid, cmd, cg);
	}
	(void)phronesis_action_log_append(s->action_log, agent_id, "start", detail);
	return PHRONESIS_OK;
}

int phronesis_supervisor_bind(phronesis_supervisor_t *s,
			      const char *agent_id,
			      const char *mode,
			      const char *workspace,
			      pid_t pid)
{
	agent_slot_t *a;
	char detail[PHRONESIS_DETAIL_MAX];
	int rc;

	if (!s || !valid_id(agent_id))
		return PHRONESIS_ERR_INVAL;
	if (mode && mode[0] && !valid_token(mode, PHRONESIS_MODE_MAX))
		return PHRONESIS_ERR_INVAL;
	if (workspace && workspace[0] && !valid_token(workspace, PHRONESIS_PATH_MAX))
		return PHRONESIS_ERR_INVAL;
	if (pid < 0)
		return PHRONESIS_ERR_INVAL;

	rc = load_agent(s, agent_id, &a);
	if (rc == PHRONESIS_OK) {
		reap_slot(a);
		if (a->state == PHRONESIS_AGENT_RUNNING) {
			/*
			 * Empty workspace is not a committed root. Sessiond
			 * often admits first with workspace "". The seated
			 * agent then bind-fills cwd. A non-empty workspace
			 * is sticky: refuse replace.
			 */
			if (a->workspace[0] == '\0' && workspace &&
			    workspace[0]) {
				snprintf(a->workspace, sizeof(a->workspace),
					 "%s", workspace);
				if (pid > 0)
					a->pid = pid;
				if (pid > 1)
					a->pgid = pid;
				if (write_slot(s, a) != PHRONESIS_OK)
					return PHRONESIS_ERR_IO;
				snprintf(detail, sizeof(detail),
					 "bind fill-workspace pid=%d",
					 (int)a->pid);
				(void)phronesis_action_log_append(s->action_log,
							     agent_id, "bind",
							     detail);
				return PHRONESIS_OK;
			}
			return PHRONESIS_ERR_EXISTS;
		}
	} else if (rc == PHRONESIS_ERR_NOTFOUND) {
		a = alloc_mem(s);
		if (!a)
			return PHRONESIS_ERR_STATE;
		snprintf(a->id, sizeof(a->id), "%s", agent_id);
	} else {
		return rc;
	}

	a->pid = pid;
	a->pgid = pid > 1 ? pid : 0;
	a->state = PHRONESIS_AGENT_RUNNING;
	a->exit_status = -1;
	a->cgroup_path[0] = '\0';
	if (mode && mode[0])
		snprintf(a->mode, sizeof(a->mode), "%s", mode);
	else
		snprintf(a->mode, sizeof(a->mode), "develop");
	if (workspace && workspace[0])
		snprintf(a->workspace, sizeof(a->workspace), "%s", workspace);
	/* Empty incoming must not wipe a committed root. Sessiond
	 * reportAgent often binds with workspace "". */

	if (write_slot(s, a) != PHRONESIS_OK)
		return PHRONESIS_ERR_IO;
	snprintf(detail, sizeof(detail), "bind pid=%d", (int)pid);
	(void)phronesis_action_log_append(s->action_log, agent_id, "bind", detail);
	return PHRONESIS_OK;
}

int phronesis_supervisor_status(phronesis_supervisor_t *s,
			   const char *agent_id,
			   phronesis_agent_status_t *out)
{
	agent_slot_t *a;
	int rc;

	if (!s || !out || !valid_id(agent_id))
		return PHRONESIS_ERR_INVAL;
	rc = load_agent(s, agent_id, &a);
	if (rc != PHRONESIS_OK)
		return rc;
	reap_slot(a);
	(void)write_slot(s, a);
	memset(out, 0, sizeof(*out));
	snprintf(out->id, sizeof(out->id), "%s", a->id);
	out->state = a->state;
	out->pid = a->pid;
	out->pgid = a->pgid;
	out->exit_status = a->exit_status;
	snprintf(out->mode, sizeof(out->mode), "%s", a->mode);
	snprintf(out->workspace, sizeof(out->workspace), "%s", a->workspace);
	out->has_cgroup = a->cgroup_path[0] ? 1 : 0;
	return PHRONESIS_OK;
}

int phronesis_supervisor_stop(phronesis_supervisor_t *s, const char *agent_id)
{
	agent_slot_t *a;
	char detail[PHRONESIS_DETAIL_MAX];
	pid_t pgid;
	int rc;

	if (!s || !valid_id(agent_id))
		return PHRONESIS_ERR_INVAL;
	rc = load_agent(s, agent_id, &a);
	if (rc != PHRONESIS_OK)
		return rc;

	reap_slot(a);
	if (a->state != PHRONESIS_AGENT_RUNNING) {
		(void)write_slot(s, a);
		(void)phronesis_action_log_append(s->action_log, agent_id, "stop", "idempotent");
		return PHRONESIS_OK;
	}

	pgid = a->pgid > 0 ? a->pgid : a->pid;
	if (pgid <= 1) {
		a->state = PHRONESIS_AGENT_STOPPED;
		a->pid = 0;
		a->pgid = 0;
		(void)write_slot(s, a);
		(void)phronesis_action_log_append(s->action_log, agent_id, "stop",
						  "bind-only");
		return PHRONESIS_OK;
	}
	if (a->cgroup_path[0] && phronesis_cgroup_kill(a->cgroup_path) == PHRONESIS_OK) {
		snprintf(detail, sizeof(detail), "cgroup.kill path=%.200s pgid=%d",
			 a->cgroup_path, (int)pgid);
	} else {
		snprintf(detail, sizeof(detail), "process-group pgid=%d", (int)pgid);
	}
	/* Always process-group kill as safety net (orphans, attach failure, non-Linux). */
	(void)kill_tree(pgid);
	if (a->cgroup_path[0]) {
		phronesis_cgroup_remove(a->cgroup_path);
		a->cgroup_path[0] = '\0';
	}
	a->state = PHRONESIS_AGENT_STOPPED;
	a->pid = 0;
	a->exit_status = -1;
	(void)write_slot(s, a);
	(void)phronesis_action_log_append(s->action_log, agent_id, "stop", detail);
	return PHRONESIS_OK;
}

int phronesis_supervisor_log(phronesis_supervisor_t *s,
			const char *agent_id,
			const char *kind,
			const char *detail)
{
	if (!s)
		return PHRONESIS_ERR_INVAL;
	if (agent_id && agent_id[0] && !valid_id(agent_id))
		return PHRONESIS_ERR_INVAL;
	return phronesis_action_log_append(s->action_log, agent_id, kind, detail);
}

int phronesis_supervisor_log_last(const phronesis_supervisor_t *s, char *buf, size_t buflen)
{
	if (!s)
		return PHRONESIS_ERR_INVAL;
	return phronesis_action_log_last(s->action_log, buf, buflen);
}

int phronesis_policy_check(phronesis_supervisor_t *s,
		      const char *agent_id,
		      const char *tool,
		      const char *action,
		      const char *path,
		      phronesis_policy_result_t *out)
{
	agent_slot_t *a = NULL;
	const char *ws = NULL;
	char detail[PHRONESIS_DETAIL_MAX];
	int rc;

	if (!s || !out)
		return PHRONESIS_ERR_INVAL;
	if (agent_id && agent_id[0]) {
		if (!valid_id(agent_id))
			return PHRONESIS_ERR_INVAL;
		rc = load_agent(s, agent_id, &a);
		if (rc == PHRONESIS_OK) {
			reap_slot(a);
			(void)write_slot(s, a);
			ws = a->workspace[0] ? a->workspace : NULL;
		} else if (rc != PHRONESIS_ERR_NOTFOUND)
			return rc;
		/* unknown agent still evaluates with empty workspace (deny paths) */
	}
	rc = phronesis_policy_eval(ws, tool, action, path, out);
	if (rc != PHRONESIS_OK)
		return rc;
	/* Seat / model ALLOW requires a running slot; null and missing ids deny. */
	if (out->decision == PHRONESIS_DECISION_ALLOW && tool &&
	    (strcmp(tool, "seat") == 0 || strcmp(tool, "model") == 0)) {
		if (!agent_id || !agent_id[0])
			phronesis_policy_result_set(out, PHRONESIS_DECISION_DENY,
					       PHRONESIS_REASON_INVALID_MESSAGE);
		else if (!a || a->state != PHRONESIS_AGENT_RUNNING)
			phronesis_policy_result_set(out, PHRONESIS_DECISION_DENY,
					       PHRONESIS_REASON_TOOLS_DEFAULT_DENY);
	}
	snprintf(detail, sizeof(detail), "tool=%s action=%s decision=%d %s",
		 tool ? tool : "", action ? action : "",
		 (int)out->decision, out->reason);
	(void)phronesis_action_log_append(s->action_log, agent_id, "policy", detail);
	return PHRONESIS_OK;
}
