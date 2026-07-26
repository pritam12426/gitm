/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * stale.c — `gitm stale` command
 *
 * Shows repos with no commits in the last N days.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
static int         g_days       = 30;

typedef struct {
	const char *name;
	const char *path;
	char        date_str[MAX_COUNT_STR];
	long        days_ago;
	bool        is_stale;
} StaleResult;

static void stale_collect(const RepoEntry *entry, void *out)
{
	StaleResult *r = out;
	r->name = entry->name;
	r->path = entry->path;

	char *date = git_last_commit_date(entry->path);
	if (date) {
		size_t len = strlen(date);
		if (len >= sizeof(r->date_str))
			len = sizeof(r->date_str) - 1;
		memcpy(r->date_str, date, len);
		r->date_str[len] = '\0';

		long ts  = parse_date_to_timestamp(r->date_str);
		long now = (long) time(NULL);
		r->days_ago = (now - ts) / 86400;
		r->is_stale = (r->days_ago >= g_days);
		free(date);
	} else {
		strcpy(r->date_str, "unknown");
		r->days_ago = -1;
		r->is_stale = true;
	}
}

static int cmd_stale(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_stale");

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

	LOG_DEBUG("checking %zu repos for staleness (>= %d days)", filtered, g_days);

	StaleResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered, stale_collect, sizeof(StaleResult), results);

	/* Count stale repos */
	size_t stale_count = 0;
	for (size_t i = 0; i < filtered; i++) {
		if (results[i].is_stale)
			stale_count++;
	}

	if (stale_count == 0) {
		fprintf(stdout, "no repos stale (>= %d days)\n", g_days);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	bool color = CMD_COLOR();

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Repository", "Last Commit", "Days Ago" };
		Table *t = table_create(3, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++) {
			if (!results[i].is_stale)
				continue;

			const char *days_str;
			char days_buf[MAX_COUNT_STR];
			if (results[i].days_ago < 0) {
				days_str = "no commits";
			} else {
				snprintf(days_buf, sizeof(days_buf), "%ld", results[i].days_ago);
				days_str = days_buf;
			}

			const char *cells[] = { results[i].name, results[i].date_str, days_str };
			table_add_row_raw(t, cells, 3);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			if (!results[i].is_stale)
				continue;

			if (results[i].days_ago < 0) {
				if (color)
					fprintf(stderr, "%s%-20s%s %s(no commits)%s\n",
					        ANSI_BOLD, results[i].name, ANSI_RESET,
					        ANSI_FG_YELLOW, ANSI_RESET);
				else
					fprintf(stderr, "%-20s (no commits)\n",
					        results[i].name);
			} else {
				if (color)
					fprintf(stderr, "%s%-20s%s %s(%ld days ago)%s\n",
					        ANSI_BOLD, results[i].name, ANSI_RESET,
					        ANSI_FG_YELLOW, results[i].days_ago, ANSI_RESET);
				else
					fprintf(stderr, "%-20s (%ld days ago)\n",
					        results[i].name, results[i].days_ago);
			}
		}
	}

	fprintf(stderr, "\n%zu/%zu repos stale (>= %d days)\n", stale_count, filtered, g_days);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_stale(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "stale",
	                                       "Show repos with no commits in N days",
	                                       cmd_stale);
	const char *stale_aliases[] = { "old" };
	argparse_command_set_aliases(cmd, stale_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);

	argparse_add_option(cmd, "days", 'd', ARG_TYPE_INT, "N",
	                    "Staleness threshold in days (default: 30)", &g_days);
	(void) cmd;
}
