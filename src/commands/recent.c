/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * recent.c — `gitm recent` command
 *
 * Lists repos sorted by last commit date (most recent first).
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

typedef struct {
	const char *name;
	const char *path;
	char       *date_str;
	long        timestamp;
} RepoDate;

static long parse_date_to_timestamp(const char *date_str)
{
	// Parse "YYYY-MM-DD HH:MM:SS +/-HHMM" from git log --format=%ci
	int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

	if (sscanf(date_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6)
		return 0;

	// Simple days-to-seconds conversion
	long days = (long) (year - 1970) * 365 + (long) ((year - 1968) / 4);
	static const int days_in_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	for (int m = 1; m < month; m++)
		days += days_in_month[m];
	if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
		days++;
	days += day - 1;

	return days * 86400 + hour * 3600 + min * 60 + sec;
}

static int cmp_repo_date(const void *a, const void *b)
{
	const RepoDate *ra = (const RepoDate *) a;
	const RepoDate *rb = (const RepoDate *) b;
	// Most recent first
	if (rb->timestamp > ra->timestamp)
		return 1;
	if (rb->timestamp < ra->timestamp)
		return -1;
	return 0;
}

int cmd_recent(const ArgParseResult *result)
{
	(void) result;

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

	RepoDate *repos = calloc(cfg.count, sizeof(RepoDate));
	if (!repos) {
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	size_t repo_count = 0;
	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		LOG_TRACE("fetching date for %s", cfg.entries[i].name);
		repos[repo_count].name = cfg.entries[i].name;
		repos[repo_count].path = cfg.entries[i].path;

		// Get last commit date
		ProcessResult r = git_exec(cfg.entries[i].path,
		                           "log", "-1", "--format=%ci", "HEAD", NULL);

		if (r.exit_code == 0 && r.stdout_len > 0) {
			// Strip newline
			char *date = strdup(r.stdout_buf);
			size_t len = strlen(date);
			if (len > 0 && date[len - 1] == '\n')
				date[len - 1] = '\0';

			repos[repo_count].date_str  = date;
			repos[repo_count].timestamp = parse_date_to_timestamp(date);
		} else {
			repos[repo_count].date_str  = strdup("unknown");
			repos[repo_count].timestamp = 0;
		}

		process_result_free(&r);
		repo_count++;
	}

	qsort(repos, repo_count, sizeof(RepoDate), cmp_repo_date);

	if (g_table_mode) {
		const char *headers[] = { "Name", "Path", "Last Commit" };
		Table *t = table_create(3, headers);
		table_set_color(t, log_use_color());

		for (size_t i = 0; i < repo_count; i++) {
			table_add_row(t, repos[i].name, repos[i].path, repos[i].date_str);
			free((char *) repos[i].date_str);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < repo_count; i++) {
			fprintf(stdout, "%-20s %-40s %s\n", repos[i].name, repos[i].path, repos[i].date_str);
			free((char *) repos[i].date_str);
		}
	}

	free(repos);
	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_recent(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "recent",
	                                       "List repos sorted by last commit date",
	                                       cmd_recent);
	const char *recent_aliases[] = { "r" };
	argparse_command_set_aliases(cmd, recent_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
