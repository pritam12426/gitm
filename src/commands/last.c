/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * last.c — `gitm last` command
 *
 * Shows the last commit log for each registered repo.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ansi_color.h"
#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "parallel.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static void last_collect(const RepoEntry *entry, void *out)
{
	CmdGitResult *r = out;
	ProcessResult pr;

	if (g_table_mode) {
		pr = git_exec(entry->path, "log", "-1", "--format=%h|%an|%ar|%s", "HEAD", NULL);
	} else {
		bool        color = CMD_COLOR();
		const char *fmt   = color ? "%C(yellow)%h%Creset %C(cyan)%an%Creset %Cgreen%ar%Creset %s"
		                          : "%h %an %ar %s";
		char        pretty_arg[MAX_PATH_LEN];
		snprintf(pretty_arg, sizeof(pretty_arg), "--pretty=tformat:%s", fmt);
		pr = git_exec_smart(entry->path, color, "log", "-1", pretty_arg, "HEAD", NULL);
	}

	process_steal_stdout(r, &pr);
}

static void last_display_plain(const CmdGitResult *r, const char *name, bool color)
{
	if (r->exit_code != 0 || r->stdout_len == 0) {
		ansi_print_repo_empty(name, "(no commits)", color);
		return;
	}

	if (color)
		fprintf(stderr, "%s%s%*s : %s", ANSI_BOLD, ANSI_FG_CYAN, NAME_COL_WIDTH, name, ANSI_RESET);
	else
		fprintf(stderr, "%*s :", NAME_COL_WIDTH, name);

	fputs(r->stdout_buf, stderr);
}

static void last_display_table(Table *t, const CmdGitResult *r, const char *repo_name)
{
	if (r->exit_code == 0 && r->stdout_len > 0) {
		char   line[PROCESS_BUF_SIZE];
		size_t len = r->stdout_len;
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, r->stdout_buf, len);
		line[len] = '\0';
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		char *save   = NULL;
		char *hash   = strtok_r(line, "|", &save);
		char *author = strtok_r(NULL, "|", &save);
		char *date   = strtok_r(NULL, "|", &save);
		char *msg    = strtok_r(NULL, "|", &save);

		const char *cells[] = {
			repo_name, hash ? hash : "-", author ? author : "-", date ? date : "-", msg ? msg : "-"
		};
		table_add_row_raw(t, cells, 5);
	} else {
		const char *cells[] = { repo_name, "-", "-", "-", "(no commits)" };
		table_add_row_raw(t, cells, 5);
	}
}

static int cmd_last(const ArgParseResult *result)
{
	(void) result;

	bool color = CMD_COLOR();

	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	CMD_RETURN_IF_EMPTY(cfg, config_path);

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group, indices, MAX_REPOS);

	if (filtered == 0) {
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	CmdGitResult results[MAX_REPOS] = { 0 };
	if (parallel_collect(&cfg, indices, filtered, last_collect, sizeof(CmdGitResult), results) != 0) {
		LOG_ERROR("parallel collection failed");
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Name", "Hash", "Author", "Date", "Message" };
		Table      *t         = table_create(5, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++)
			last_display_table(t, &results[i], cfg.entries[indices[i]].name);

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			LOG_TRACE("showing last log for %s", cfg.entries[indices[i]].name);
			last_display_plain(&results[i], cfg.entries[indices[i]].name, color);
		}
	}

	for (size_t i = 0; i < filtered; i++)
		free(results[i].stdout_buf);

	LOG_DEBUG("last: done");
	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_last(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "last", "Show last commit log for each repo", cmd_last);
	const char *last_aliases[] = { "log", "l" };
	argparse_command_set_aliases(cmd, last_aliases, 2);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
