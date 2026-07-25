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

#define _POSIX_C_SOURCE 200809L

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
	char        date_str[32]; /* inline — no heap */
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

static int cmd_recent(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_recent");
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

	/* Stack array — no heap */
	RepoDate repos[MAX_REPOS];
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
			// Copy into inline buffer — strip newline
			size_t len = r.stdout_len;
			if (len > 0 && r.stdout_buf[len - 1] == '\n')
				len--;
			if (len >= sizeof(repos[0].date_str))
				len = sizeof(repos[0].date_str) - 1;
			memcpy(repos[repo_count].date_str, r.stdout_buf, len);
			repos[repo_count].date_str[len] = '\0';

			repos[repo_count].timestamp = parse_date_to_timestamp(repos[repo_count].date_str);
		} else {
			strcpy(repos[repo_count].date_str, "unknown");
			repos[repo_count].timestamp = 0;
		}

		process_result_free(&r);
		repo_count++;
	}

	qsort(repos, repo_count, sizeof(RepoDate), cmp_repo_date);
	LOG_DEBUG("sorted %zu repos by commit date", repo_count);

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Name", "Path", "Last Commit" };
		Table *t = table_create(3, headers);
		table_set_color(t, CMD_COLOR());

		for (size_t i = 0; i < repo_count; i++)
			table_add_row(t, repos[i].name, repos[i].path, repos[i].date_str);

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < repo_count; i++)
			fprintf(stdout, "%-20s %-40s %s\n", repos[i].name, repos[i].path, repos[i].date_str);
	}

	cmd_cleanup(&cfg, config_path);
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
