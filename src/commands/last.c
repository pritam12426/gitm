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

static void print_last(const char *name, const char *path, bool color)
{
	// Format: hash author date subject
	ProcessResult r = git_exec(path,
	                           "log", "-1",
	                           "--format=%h %an %ar %s",
	                           "HEAD", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no commits)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no commits)\n", name);
		process_result_free(&r);
		return;
	}

	// Strip newline
	char *line = strdup(r.stdout_buf);
	size_t len = strlen(line);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';

	// Parse: hash author date subject
	char *p = line;

	// Hash (first word)
	char *hash = p;
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';

	// Author (until we hit a date pattern like "2 hours ago")
	char *author = p;
	while (*p && !((*p >= '0' && *p <= '9') && *(p + 1) == ' ' &&
	               (*(p + 2) == 'm' || *(p + 2) == 'h' || *(p + 2) == 'd' ||
	                *(p + 2) == 'w' || *(p + 2) == 'y' || *(p + 2) == 's')))
		p++;

	// Find the date part
	char *date = p;
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';
	// Skip "ago"
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';

	char *subject = p;

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n"
		                "  \x1b[33m%s\x1b[0m %s \x1b[2m%s\x1b[0m %s\n",
		        name, hash, author, date, subject);
	else
		fprintf(stderr, "\n%s\n  %s %s %s %s\n",
		        name, hash, author, date, subject);

	free(line);
	process_result_free(&r);
}

int cmd_last(const ArgParseResult *result)
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
		const char *headers[] = { "Name", "Hash", "Author", "Date", "Message" };
		Table *t = table_create(5, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < cfg.count; i++) {
			if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
				continue;
			if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
				continue;

			ProcessResult r = git_exec(cfg.entries[i].path,
			                           "log", "-1",
			                           "--format=%h|%an|%ar|%s",
			                           "HEAD", NULL);

			if (r.exit_code != 0 || r.stdout_len == 0) {
				const char *cells[] = { cfg.entries[i].name, "-", "-", "-", "(no commits)" };
				table_add_row_raw(t, cells, 5);
			} else {
				char *line = strdup(r.stdout_buf);
				size_t len = strlen(line);
				if (len > 0 && line[len - 1] == '\n')
					line[len - 1] = '\0';

				/* Split by | */
				char *hash   = strtok(line, "|");
				char *author = strtok(NULL, "|");
				char *date   = strtok(NULL, "|");
				char *msg    = strtok(NULL, "|");

				const char *cells[] = {
					cfg.entries[i].name,
					hash ? hash : "-",
					author ? author : "-",
					date ? date : "-",
					msg ? msg : "-"
				};
				table_add_row_raw(t, cells, 5);

				free(line);
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

			LOG_TRACE("showing last log for %s", cfg.entries[i].name);
			print_last(cfg.entries[i].name, cfg.entries[i].path, color);
		}
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_last(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "last",
	                                       "Show last commit log for each repo",
	                                       cmd_last);
	const char *last_aliases[] = { "log", "l" };
	argparse_command_set_aliases(cmd, last_aliases, 2);
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
