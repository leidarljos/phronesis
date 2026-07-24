/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int json_escape(const char *in, char *out, size_t outlen)
{
	size_t j = 0;

	if (!in)
		in = "";
	for (size_t i = 0; in[i]; i++) {
		unsigned char c = (unsigned char)in[i];
		if (c == '"' || c == '\\') {
			if (j + 2 >= outlen)
				return -1;
			out[j++] = '\\';
			out[j++] = (char)c;
		} else if (c < 0x20) {
			if (j + 6 >= outlen)
				return -1;
			j += (size_t)snprintf(out + j, outlen - j, "\\u%04x", c);
		} else {
			if (j + 1 >= outlen)
				return -1;
			out[j++] = (char)c;
		}
	}
	if (j >= outlen)
		return -1;
	out[j] = '\0';
	return 0;
}

int grok_action_log_append(const char *path,
			   const char *agent_id,
			   const char *kind,
			   const char *detail)
{
	FILE *f;
	char ek[GROK_DETAIL_MAX * 2];
	char ed[GROK_DETAIL_MAX * 2];
	char ea[GROK_ID_MAX * 2];
	time_t now;

	if (!path || !kind || !kind[0])
		return GROK_ERR_INVAL;
	if (json_escape(kind, ek, sizeof(ek)) != 0)
		return GROK_ERR_INVAL;
	if (json_escape(detail ? detail : "", ed, sizeof(ed)) != 0)
		return GROK_ERR_INVAL;
	if (json_escape(agent_id ? agent_id : "", ea, sizeof(ea)) != 0)
		return GROK_ERR_INVAL;

	now = time(NULL);
	f = fopen(path, "a");
	if (!f)
		return GROK_ERR_IO;
	if (fprintf(f,
		    "{\"ts\":%lld,\"agent\":\"%s\",\"kind\":\"%s\",\"detail\":\"%s\"}\n",
		    (long long)now, ea, ek, ed) < 0) {
		fclose(f);
		return GROK_ERR_IO;
	}
	if (fclose(f) != 0)
		return GROK_ERR_IO;
	return GROK_OK;
}

int grok_action_log_last(const char *path, char *buf, size_t buflen)
{
	FILE *f;
	char line[2048];
	char last[2048];
	size_t n;

	if (!path || !buf || buflen == 0)
		return GROK_ERR_INVAL;
	buf[0] = '\0';
	last[0] = '\0';
	f = fopen(path, "r");
	if (!f)
		return GROK_ERR_IO;
	while (fgets(line, sizeof(line), f)) {
		n = strlen(line);
		while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = '\0';
		if (n > 0)
			snprintf(last, sizeof(last), "%s", line);
	}
	fclose(f);
	if (!last[0])
		return GROK_ERR_NOTFOUND;
	if (snprintf(buf, buflen, "%s", last) >= (int)buflen)
		return GROK_ERR_INVAL;
	return GROK_OK;
}
