/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd_util.c — Shared command utilities implementation
 */

#include "cmd_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ansi_color.h"
#include "cmd.h"
#include "git.h"
#include "log.h"
#include "share.h"


int cmd_load_config(GitConfig *cfg, char **path_out)
{
	LOG_TRACE("cmd_load_config");
	*path_out = config_default_path();
	if (!*path_out) {
		LOG_ERROR(MSG_CFG_PATH_ERR);
		return -1;
	}

	if (config_load(*path_out, cfg) != 0) {
		LOG_ERROR(MSG_CFG_LOAD_ERR);
		free(*path_out);
		*path_out = NULL;
		config_free(cfg);
		return -1;
	}

	LOG_DEBUG("loaded config with %zu entries", cfg->count);
	return 0;
}

size_t cmd_filter_entries(const GitConfig *cfg,
                          const char      *filter_tag,
                          const char      *filter_group,
                          size_t          *out_indices,
                          size_t           max)
{
	size_t count = 0;

	LOG_TRACE("cmd_filter_entries(tag=%s, group=%s)",
	          filter_tag ? filter_tag : "-",
	          filter_group ? filter_group : "-");
	for (size_t i = 0; i < cfg->count && count < max; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg->entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg->entries[i], filter_group))
			continue;
		out_indices[count++] = i;
	}

	return count;
}

void cmd_register_filter_flags(ArgCommand *cmd, const char **out_tag, const char **out_group)
{
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG", "Filter by tag", out_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP", "Filter by group", out_group);
}

void cmd_print_name_path(FILE *stream, const char *name, const char *path)
{
	fprintf(stream, "%*s : %s\n", NAME_COL_WIDTH, name ? name : "(null)", path ? path : "(null)");
}

void cmd_display_plain_result(int         exit_code,
                              const char *stdout_buf,
                              size_t      stdout_len,
                              const char *name,
                              const char *empty_msg,
                              bool        color)
{
	if (exit_code != 0 || stdout_len == 0) {
		if (empty_msg)
			ansi_print_repo_empty(name, empty_msg, color);
		return;
	}

	ansi_print_repo_header(name, color);
	fputs(stdout_buf, stderr);
}

const char *cmd_resolve_editor(void)
{
	const char *editor = getenv("EDITOR");
	if (!editor)
		editor = getenv("VISUAL");
	return editor;
}

int cmd_repo_commit_age(const char *path, char *date_out, size_t date_len, long *days_out)
{
	if (git_last_commit_date_into(path, date_out, date_len) != 0)
		return -1;

	long ts  = parse_date_to_timestamp(date_out);
	long now = (long) time(NULL);
	*days_out = (now - ts) / 86400;
	return 0;
}

RepoEntry *cmd_find_repo_or_fail(GitConfig *cfg, char *config_path, const char *name)
{
	RepoEntry *entry = config_find(cfg, name);
	if (!entry) {
		fprintf(stderr, MSG_REPO_NOT_FOUND, name);
		cmd_cleanup(cfg, config_path);
	}
	return entry;
}

size_t copy_next_line(const char **pp, char *buf, size_t buflen)
{
	const char *p = *pp;
	if (!*p)
		return 0;

	const char *start = p;
	while (*p && *p != '\n')
		p++;

	size_t len = (size_t) (p - start);
	if (len >= buflen)
		len = buflen - 1;
	memcpy(buf, start, len);
	buf[len] = '\0';

	if (*p == '\n')
		p++;
	*pp = p;
	return len;
}

RepoHealth repo_check_health(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return REPO_HEALTH_MISSING;
	if (!S_ISDIR(st.st_mode))
		return REPO_HEALTH_NOT_DIR;
	if (!git_is_repo(path))
		return REPO_HEALTH_NOT_GIT;
	return REPO_HEALTH_OK;
}
