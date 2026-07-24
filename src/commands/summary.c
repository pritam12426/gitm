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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static long get_dir_size(const char *path)
{
	ProcessResult r = git_exec(path, "count-objects", "-rvH", NULL);
	long          total = 0;

	if (r.exit_code == 0 && r.stdout_len > 0) {
		// Parse "N objects" and "N MiB" etc. — just get the size line
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
			// Skip to next line
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

int cmd_summary(const ArgParseResult *result)
{
	(void) result;

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

	int   total_branches = 0;
	long  total_size     = 0;
	size_t filtered_count = 0;

	LOG_DEBUG("computing summary for repos");

	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		total_branches += count_branches(cfg.entries[i].path);
		total_size     += get_dir_size(cfg.entries[i].path);
		filtered_count++;
	}

	char size_buf[32];
	format_size(size_buf, sizeof(size_buf), total_size);

	LOG_INFO("summary: %zu repos, %d branches, %s", filtered_count, total_branches, size_buf);

	fprintf(stdout, "Repos:    %zu\n", filtered_count);
	fprintf(stdout, "Branches: %d\n", total_branches);
	fprintf(stdout, "Size:     %s\n", size_buf);

	config_free(&cfg);
	free(config_path);
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
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &filter_group);
	(void) cmd;
}
