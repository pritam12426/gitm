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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static void print_branches(const char *name, const char *path, bool color)
{
	ProcessResult r = git_exec(path, "branch", "--list", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no branches)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no branches)\n", name);
		process_result_free(&r);
		return;
	}

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n", name);
	else
		fprintf(stderr, "\n%s\n", name);

	const char *p = r.stdout_buf;
	while (*p) {
		// Skip leading spaces
		while (*p == ' ')
			p++;

		const char *start = p;
		while (*p && *p != '\n')
			p++;

		size_t len = (size_t) (p - start);
		char   line[256];
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, start, len);
		line[len] = '\0';

		// Detect current branch (* prefix)
		if (line[0] == '*' && line[1] == ' ') {
			if (color)
				fprintf(stderr, "  \x1b[1m\x1b[32m* %s\x1b[0m\n", line + 2);
			else
				fprintf(stderr, "  * %s\n", line + 2);
		} else {
			fprintf(stderr, "    %s\n", line);
		}

		if (*p == '\n')
			p++;
	}

	process_result_free(&r);
}

int cmd_branch(const ArgParseResult *result)
{
	(void) result;

	bool color = log_use_color();

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, "No repositories registered.\n");
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	if (g_table_mode) {
		const char *headers[] = { "Repository", "Branch" };
		Table *t = table_create(2, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < cfg.count; i++) {
			if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
				continue;
			if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
				continue;

			ProcessResult r = git_exec(cfg.entries[i].path, "branch", "--list", NULL);

			if (r.exit_code != 0 || r.stdout_len == 0) {
				const char *cells[] = { cfg.entries[i].name, "-" };
				table_add_row_raw(t, cells, 2);
			} else {
				const char *p = r.stdout_buf;
				bool first = true;
				while (*p) {
					while (*p == ' ')
						p++;

					const char *start = p;
					while (*p && *p != '\n')
						p++;

					size_t len = (size_t) (p - start);
					char   line[256];
					if (len >= sizeof(line))
						len = sizeof(line) - 1;
					memcpy(line, start, len);
					line[len] = '\0';

					const char *repo_name = first ? cfg.entries[i].name : "";
					bool is_current = (line[0] == '*');

					if (color && is_current) {
						char colored[256];
						snprintf(colored, sizeof(colored), "\x1b[1m\x1b[32m* %s\x1b[0m", line + 2);
						const char *cells[] = { repo_name, colored };
						table_add_row_raw(t, cells, 2);
					} else {
						const char *display = is_current ? line + 2 : line;
						const char *cells[] = { repo_name, display };
						table_add_row_raw(t, cells, 2);
					}

					first = false;
					if (*p == '\n')
						p++;
				}
			}

			process_result_free(&r);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < cfg.count; i++) {
			if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
				continue;
			if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
				continue;

			LOG_TRACE("showing branches for %s", cfg.entries[i].name);
			print_branches(cfg.entries[i].name, cfg.entries[i].path, color);
		}
	}

	config_free(&cfg);
	free(config_path);
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
