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
	char        date_str[MAX_COUNT_STR];
	char        relative[MAX_COUNT_STR];
	long        timestamp;
} RecentResult;

static void recent_collect(const RepoEntry *entry, void *out)
{
	RecentResult *r = out;
	r->name         = entry->name;
	r->path         = entry->path;

	if (git_last_commit_date_into(entry->path, r->date_str, sizeof(r->date_str)) != 0) {
		strcpy(r->date_str, "unknown");
		strcpy(r->relative, "unknown");
		r->timestamp = 0;
	} else {
		r->timestamp = parse_date_to_timestamp(r->date_str);
		format_relative_time(r->relative, sizeof(r->relative), r->timestamp);
	}
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
	if (kb->timestamp > ka->timestamp)
		return 1;
	if (kb->timestamp < ka->timestamp)
		return -1;
	return 0;
}

static int cmd_recent(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_recent");
	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	CMD_RETURN_IF_EMPTY(cfg, config_path);

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group, indices, MAX_REPOS);

	/* Phase 1: parallel collection */
	RecentResult results[MAX_REPOS] = { 0 };
	if (parallel_collect(&cfg, indices, filtered, recent_collect, sizeof(RecentResult), results)
	    != 0) {
		LOG_ERROR("parallel collection failed");
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

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
		const char *headers[] = { "Name", "Path", "Last Commit Relative", "Last Commit" };
		Table      *t         = table_create(4, headers);
		table_set_color(t, CMD_COLOR());

		for (size_t i = 0; i < filtered; i++) {
			RecentResult *r = &results[keys[i].result_index];
			table_add_row(t, r->name, r->path, r->relative, r->date_str);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			RecentResult *r = &results[keys[i].result_index];
			fprintf(stdout, "%-20s %*s : %s\n", r->relative, NAME_COL_WIDTH, r->name, r->path);
		}
	}

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_recent(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "recent", "List repos sorted by last commit date", cmd_recent);
	const char *recent_aliases[] = { "r" };
	argparse_command_set_aliases(cmd, recent_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
