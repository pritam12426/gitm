/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * branch.c — `gitm branch` command
 *
 * Shows local branches for all registered repos.
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

static void branch_collect(const RepoEntry *entry, void *out)
{
	CmdGitResult   *r = out;
	ProcessResult pr;

	bool color = g_table_mode ? false : CMD_COLOR();
	pr = git_exec_smart(entry->path, color, "branch", "--list", NULL);

	process_steal_stdout(r, &pr);
}

static void branch_display_table(Table *t, const CmdGitResult *r, const char *repo_name, bool color)
{
	if (r->exit_code != 0 || r->stdout_len == 0) {
		const char *cells[] = { repo_name, "-" };
		table_add_row_raw(t, cells, 2);
		return;
	}

	const char *p = r->stdout_buf;
	bool first = true;
	while (*p) {
		while (*p == ' ')
			p++;

		const char *start = p;
		while (*p && *p != '\n')
			p++;

		size_t len = (size_t) (p - start);
		char   line[MAX_NAME_LEN];
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, start, len);
		line[len] = '\0';

		const char *repo_disp = first ? repo_name : "";
		bool is_current = (line[0] == '*');

		if (color && is_current) {
			char colored[MAX_NAME_LEN];
			snprintf(colored, sizeof(colored), "%s%s* %s%s", ANSI_BOLD, ANSI_FG_GREEN, line + 2, ANSI_RESET);
			const char *cells[] = { repo_disp, colored };
			table_add_row_raw(t, cells, 2);
		} else {
			const char *display = is_current ? line + 2 : line;
			const char *cells[] = { repo_disp, display };
			table_add_row_raw(t, cells, 2);
		}

		first = false;
		if (*p == '\n')
			p++;
	}
}

static int cmd_branch(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_branch");
	bool color = CMD_COLOR();

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group, indices, MAX_REPOS);

	if (filtered == 0) {
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	CmdGitResult results[MAX_REPOS] = { 0 };
	if (parallel_collect(&cfg, indices, filtered, branch_collect, sizeof(CmdGitResult), results) != 0) {
		LOG_ERROR("parallel collection failed");
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Repository", "Branch" };
		Table *t = table_create(2, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++)
			branch_display_table(t, &results[i], cfg.entries[indices[i]].name, color);

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			LOG_TRACE("showing branches for %s", cfg.entries[indices[i]].name);
			cmd_display_plain_result(results[i].exit_code, results[i].stdout_buf,
			                         results[i].stdout_len, cfg.entries[indices[i]].name,
			                         "(no branches)", color);
		}
	}

	for (size_t i = 0; i < filtered; i++)
		free(results[i].stdout_buf);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_branch(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "branch",
	                                       "Show local branches",
	                                       cmd_branch);
	const char *branch_aliases[] = { "br", "b" };
	argparse_command_set_aliases(cmd, branch_aliases, 2);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
