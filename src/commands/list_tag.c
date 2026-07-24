/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * list_tag.c — `gitm list-tag` command
 *
 * Shows tags with their messages for all registered repos.
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

static void print_tags(const char *name, const char *path, bool color)
{
	ProcessResult r = git_exec(path, "tag", "-n1", "--sort=-version:refname", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
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

		// Split tag and message
		char *space = strchr(line, ' ');
		if (space) {
			*space = '\0';
			const char *tag     = line;
			const char *message = space + 1;
			if (color)
				fprintf(stderr, "  \x1b[33m%-20s\x1b[0m %s\n", tag, message);
			else
				fprintf(stderr, "  %-20s %s\n", tag, message);
		} else {
			if (color)
				fprintf(stderr, "  \x1b[33m%s\x1b[0m\n", line);
			else
				fprintf(stderr, "  %s\n", line);
		}

		if (*p == '\n')
			p++;
	}

	process_result_free(&r);
}

int cmd_list_tag(const ArgParseResult *result)
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
		const char *headers[] = { "Repository", "Tag", "Message" };
		Table *t = table_create(3, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < cfg.count; i++) {
			if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
				continue;
			if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
				continue;

			ProcessResult r = git_exec(cfg.entries[i].path, "tag", "-n1", "--sort=-version:refname", NULL);

			if (r.exit_code != 0 || r.stdout_len == 0) {
				const char *cells[] = { cfg.entries[i].name, "-", "-" };
				table_add_row_raw(t, cells, 3);
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

					char *space = strchr(line, ' ');
					const char *repo_name = first ? cfg.entries[i].name : "";
					if (space) {
						*space = '\0';
						const char *cells[] = { repo_name, line, space + 1 };
						table_add_row_raw(t, cells, 3);
					} else {
						const char *cells[] = { repo_name, line, "-" };
						table_add_row_raw(t, cells, 3);
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

			LOG_TRACE("listing tags for %s", cfg.entries[i].name);
			print_tags(cfg.entries[i].name, cfg.entries[i].path, color);
		}
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_list_tag(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "list-tag",
	                                       "Show tags with messages",
	                                       cmd_list_tag);
	const char *list_tag_aliases[] = { "tags", "lt" };
	argparse_command_set_aliases(cmd, list_tag_aliases, 2);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
