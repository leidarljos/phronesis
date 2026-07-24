/* SPDX-License-Identifier: Apache-2.0 */
#ifndef GROK_POLICYD_SUPERVISOR_H
#define GROK_POLICYD_SUPERVISOR_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GROK_OK            0
#define GROK_ERR_INVAL    (-1)
#define GROK_ERR_EXISTS   (-2)
#define GROK_ERR_NOTFOUND (-3)
#define GROK_ERR_IO       (-4)
#define GROK_ERR_SPAWN    (-5)
#define GROK_ERR_STATE    (-6)
#define GROK_ERR_DENIED   (-7)

#define GROK_ID_MAX       64
#define GROK_MODE_MAX     32
#define GROK_PATH_MAX     512
#define GROK_DETAIL_MAX   512
#define GROK_TOOL_MAX     64
#define GROK_ACTION_MAX   64
#define GROK_REASON_MAX   128

typedef enum {
	GROK_AGENT_STOPPED = 0,
	GROK_AGENT_RUNNING = 1,
	GROK_AGENT_FAILED = 2
} grok_agent_state_t;

typedef enum {
	GROK_DECISION_DENY = 0,
	GROK_DECISION_ALLOW = 1,
	GROK_DECISION_PROMPT = 2
} grok_decision_t;

typedef struct {
	char id[GROK_ID_MAX];
	grok_agent_state_t state;
	pid_t pid;
	pid_t pgid;
	int exit_status;
	char mode[GROK_MODE_MAX];
	char workspace[GROK_PATH_MAX];
	/* 1 if agent was placed in a cgroup for stop; 0 = process-group only */
	int has_cgroup;
} grok_agent_status_t;

typedef struct {
	grok_decision_t decision;
	char reason[GROK_REASON_MAX];
} grok_policy_result_t;

typedef struct grok_supervisor grok_supervisor_t;

int grok_supervisor_open(grok_supervisor_t **out,
			 const char *state_dir,
			 const char *runtime_dir);
void grok_supervisor_close(grok_supervisor_t *s);

const char *grok_supervisor_action_log_path(const grok_supervisor_t *s);
const char *grok_supervisor_state_dir(const grok_supervisor_t *s);
const char *grok_supervisor_runtime_dir(const grok_supervisor_t *s);

int grok_supervisor_start(grok_supervisor_t *s,
			  const char *agent_id,
			  const char *mode,
			  const char *workspace,
			  char *const argv[]);
int grok_supervisor_status(grok_supervisor_t *s,
			   const char *agent_id,
			   grok_agent_status_t *out);
int grok_supervisor_stop(grok_supervisor_t *s, const char *agent_id);
int grok_supervisor_log(grok_supervisor_t *s,
			const char *agent_id,
			const char *kind,
			const char *detail);
int grok_supervisor_log_last(const grok_supervisor_t *s,
			     char *buf,
			     size_t buflen);

/*
 * Tools default deny. High-risk actions → prompt. Path ops under the agent's
 * workspace root may allow. Logged as non-trivial policy decisions.
 */
int grok_policy_check(grok_supervisor_t *s,
		      const char *agent_id,
		      const char *tool,
		      const char *action,
		      const char *path,
		      grok_policy_result_t *out);

#ifdef __cplusplus
}
#endif

#endif
