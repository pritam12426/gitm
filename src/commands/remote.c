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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static void print_remotes(const char *name, const char *path, bool color)
{
	ProcessResult r = git_exec(path, "remote", "-v", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no remotes)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no remotes)\n", name);
		process_result_free(&r);
		return;
	}

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n", name);
	else
		fprintf(stderr, "\n%s\n", name);

	const char *p = r.stdout_buf;
	while (*p) {
		const char *start = p;
		while (*p && *p != '\n')
			p++;

		size_t len = (size_t) (p - start);
		char   line[512];
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, start, len);
		line[len] = '\0';

		// Color the remote name (first word)
		char *space = strchr(line, ' ');
		if (space && color) {
			*space = '\0';
			fprintf(stderr, "  \x1b[33m%-10s\x1b[0m %s\n", line, space + 1);
		} else {
			fprintf(stderr, "  %s\n", line);
		}

		if (*p == '\n')
			p++;
	}

	process_result_free(&r);
}

int cmd_remote(const ArgParseResult *result)
{
	(void) result;

	bool color = log_use_color();

	char *config_path = config_default_path();
	if (!config_path) {
		LOG_ERROR("could not determine config path");
		return 1;
	}

	GitConfig cfg = { 0 };
	if (config_load(config_path, &cfg) != 0) {
		LOG_ERROR("could not load config");
		free(config_path);
		return 1;
	}

	if (cfg.count == 0) {
		fprintf(stderr, "No repositories registered.\n");
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	if (g_table_mode) {
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
					char   line[512];
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

	config_free(&cfg);
	free(config_path);
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
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
