/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * summary.c — `gitm summary` command
 *
 * Shows total repos, total branches, and total size.
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

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

typedef struct {
	int   branch_count;
	long  dir_size;
} SummaryResult;

static long get_dir_size(const char *path)
{
	ProcessResult r = git_exec(path, "count-objects", "-rvH", NULL);
	long          total = 0;

	if (r.exit_code == 0 && r.stdout_len > 0) {
		const char *p = r.stdout_buf;
		while (*p) {
			long   val = 0;
			char   unit[8] = { 0 };
			if (sscanf(p, "%ld %7s", &val, unit) == 2) {
				if (strstr(unit, "MiB") || strstr(unit, "MB"))
					total += val * 1024 * 1024;
				else if (strstr(unit, "GiB") || strstr(unit, "GB"))
					total += val * 1024 * 1024 * 1024;
				else if (strstr(unit, "KiB") || strstr(unit, "KB"))
					total += val * 1024;
				else
					total += val;
			}
			while (*p && *p != '\n')
				p++;
			if (*p == '\n')
				p++;
		}
	}

	process_result_free(&r);
	return total;
}

static int count_branches(const char *path)
{
	ProcessResult r    = git_exec(path, "branch", "--list", NULL);
	int           count = 0;

	if (r.exit_code == 0 && r.stdout_len > 0) {
		const char *p = r.stdout_buf;
		while (*p) {
			if (*p == '\n')
				count++;
			p++;
		}
	}

	process_result_free(&r);
	return count;
}

static void format_size(char *buf, size_t buflen, long bytes)
{
	if (bytes >= 1024L * 1024 * 1024)
		snprintf(buf, buflen, "%.1f GiB", (double) bytes / (1024.0 * 1024 * 1024));
	else if (bytes >= 1024L * 1024)
		snprintf(buf, buflen, "%.1f MiB", (double) bytes / (1024.0 * 1024));
	else if (bytes >= 1024L)
		snprintf(buf, buflen, "%.1f KiB", (double) bytes / 1024.0);
	else
		snprintf(buf, buflen, "%ld B", bytes);
}

static void summary_collect(const RepoEntry *entry, void *out)
{
	SummaryResult *r = out;
	r->branch_count = count_branches(entry->path);
	r->dir_size     = get_dir_size(entry->path);
}

static int cmd_summary(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_summary");
	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group,
	                                     indices, cfg.count);

	LOG_DEBUG("computing summary for %zu repos", filtered);

	/* Phase 1: parallel collection */
	SummaryResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered,
	                 summary_collect, sizeof(SummaryResult), results);

	/* Phase 2: aggregate sequentially */
	int   total_branches = 0;
	long  total_size     = 0;

	for (size_t i = 0; i < filtered; i++) {
		total_branches += results[i].branch_count;
		total_size     += results[i].dir_size;
	}

	char size_buf[32];
	format_size(size_buf, sizeof(size_buf), total_size);

	LOG_INFO("summary: %zu repos, %d branches, %s", filtered, total_branches, size_buf);

	fprintf(stdout, "Repos:    %zu\n", filtered);
	fprintf(stdout, "Branches: %d\n", total_branches);
	fprintf(stdout, "Size:     %s\n", size_buf);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_summary(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "summary",
	                                       "Total repos, branches, and size",
	                                       cmd_summary);
	const char *summary_aliases[] = { "sum" };
	argparse_command_set_aliases(cmd, summary_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	(void) cmd;
}
