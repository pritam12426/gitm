/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * remote.c — `gitm remote` command
 *
 * Shows remote settings for all registered repos.
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
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static void print_remotes(const char *name, const char *path, bool color)
{
	ProcessResult r = color
	    ? git_exec_color(path, "remote", "-v", NULL)
	    : git_exec(path, "remote", "-v", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		ansi_print_repo_empty(name, "(no remotes)", color);
		process_result_free(&r);
		return;
	}

	ansi_print_repo_header(name, color);

	/* Git output already has colours (FORCE_COLOR=1) — pass through */
	fputs(r.stdout_buf, stderr);

	process_result_free(&r);
}

static int cmd_remote(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_remote");
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

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Repository", "Remote", "URL", "Type" };
		Table *t = table_create(4, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < cfg.count; i++) {
			if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
				continue;
			if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
				continue;

			ProcessResult r = git_exec(cfg.entries[i].path, "remote", "-v", NULL);

			if (r.exit_code != 0 || r.stdout_len == 0) {
				const char *cells[] = { cfg.entries[i].name, "-", "-", "-" };
				table_add_row_raw(t, cells, 4);
			} else {
				const char *p = r.stdout_buf;
				bool first = true;
				while (*p) {
					const char *start = p;
					while (*p && *p != '\n')
						p++;

				size_t len = (size_t) (p - start);
				char   line[MAX_PATH_LEN];
					if (len >= sizeof(line))
						len = sizeof(line) - 1;
					memcpy(line, start, len);
					line[len] = '\0';

					/* Parse: "name\turl (type)" */
					char *tab = strchr(line, '\t');
					if (tab) {
						*tab = '\0';
						char *name = line;
						char *rest = tab + 1;

						/* Find "(fetch)" or "(push)" */
						char *paren = strchr(rest, ' ');
						char *url = rest;
						char *type = "-";
						if (paren) {
							*paren = '\0';
							type = paren + 1;
							/* Strip parens from type */
							if (*type == '(') {
								type++;
								char *close = strchr(type, ')');
								if (close)
									*close = '\0';
							}
						}

						const char *repo_name = first ? cfg.entries[i].name : "";
						const char *cells[] = { repo_name, name, url, type };
						table_add_row_raw(t, cells, 4);
						first = false;
					}

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

			LOG_TRACE("showing remotes for %s", cfg.entries[i].name);
			print_remotes(cfg.entries[i].name, cfg.entries[i].path, color);
		}
	}

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_remote(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "remote",
	                                       "Show remote settings",
	                                       cmd_remote);
	const char *remote_aliases[] = { "rem" };
	argparse_command_set_aliases(cmd, remote_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
