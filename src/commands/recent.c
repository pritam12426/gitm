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
#include "parallel.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

typedef struct {
	const char *name;
	const char *path;
	char        date_str[32];
	long        timestamp;
} RecentResult;

static long parse_date_to_timestamp(const char *date_str)
{
	int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

	if (sscanf(date_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6)
		return 0;

	long days = (long) (year - 1970) * 365 + (long) ((year - 1968) / 4);
	static const int days_in_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	for (int m = 1; m < month; m++)
		days += days_in_month[m];
	if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
		days++;
	days += day - 1;

	return days * 86400 + hour * 3600 + min * 60 + sec;
}

static void recent_collect(const RepoEntry *entry, void *out)
{
	RecentResult *r = out;
	r->name = entry->name;
	r->path = entry->path;

	ProcessResult pr = git_exec(entry->path, "log", "-1", "--format=%ci", "HEAD", NULL);

	if (pr.exit_code == 0 && pr.stdout_len > 0) {
		size_t len = pr.stdout_len;
		if (len > 0 && pr.stdout_buf[len - 1] == '\n')
			len--;
		if (len >= sizeof(r->date_str))
			len = sizeof(r->date_str) - 1;
		memcpy(r->date_str, pr.stdout_buf, len);
		r->date_str[len] = '\0';
		r->timestamp = parse_date_to_timestamp(r->date_str);
	} else {
		strcpy(r->date_str, "unknown");
		r->timestamp = 0;
	}

	process_result_free(&pr);
}

/* Sort key for decorating results before qsort */
typedef struct {
	size_t result_index;
	long   timestamp;
} SortKey;

static int cmp_sort_key(const void *a, const void *b)
{
	const SortKey *ka = a;
	const SortKey *kb = b;
	if (kb->timestamp > ka->timestamp) return  1;
	if (kb->timestamp < ka->timestamp) return -1;
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

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group,
	                                     indices, cfg.count);

	/* Phase 1: parallel collection */
	RecentResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered,
	                 recent_collect, sizeof(RecentResult), results);

	/* Phase 2: sort by timestamp (most recent first) */
	SortKey keys[MAX_REPOS];
	for (size_t i = 0; i < filtered; i++) {
		keys[i].result_index = i;
		keys[i].timestamp    = results[i].timestamp;
	}
	qsort(keys, filtered, sizeof(SortKey), cmp_sort_key);
	LOG_DEBUG("sorted %zu repos by commit date", filtered);

	/* Phase 3: display in sorted order */
	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Name", "Path", "Last Commit" };
		Table *t = table_create(3, headers);
		table_set_color(t, CMD_COLOR());

		for (size_t i = 0; i < filtered; i++) {
			RecentResult *r = &results[keys[i].result_index];
			table_add_row(t, r->name, r->path, r->date_str);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			RecentResult *r = &results[keys[i].result_index];
			fprintf(stdout, "%-20s %-40s %s\n", r->name, r->path, r->date_str);
		}
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
