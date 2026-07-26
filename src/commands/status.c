/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * status.c — `gitm status` command
 *
 * Shows git status for all registered repos with colour.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
	char  *stdout_buf;
	size_t stdout_len;
	char   branch[MAX_NAME_LEN];
	char   status_buf[MAX_NAME_LEN];
	int    exit_code;
} StatusResult;

static void status_collect(const RepoEntry *entry, void *out)
{
	StatusResult *r = out;
	memset(r, 0, sizeof(*r));

	ProcessResult pr = git_exec(entry->path, "status", "--porcelain", "--branch", NULL);
	r->exit_code     = pr.exit_code;

	/* Steal stdout — prevents double-free */
	r->stdout_buf = pr.stdout_buf;
	r->stdout_len = pr.stdout_len;
	pr.stdout_buf = NULL;
	process_result_free(&pr);

	if (r->exit_code != 0 || r->stdout_len == 0)
		return;

	/* Parse branch from first line: "## branch-name...[ahead N]" */
	const char *p = r->stdout_buf;
	if (p[0] == '#' && p[1] == '#') {
		p += 2;
		while (*p == ' ')
			p++;
		const char *end = p;
		while (*end && *end != '\n' && *end != '.' && *end != '[')
			end++;
		size_t len = (size_t) (end - p);
		if (len >= sizeof(r->branch))
			len = sizeof(r->branch) - 1;
		memcpy(r->branch, p, len);
		r->branch[len] = '\0';
	}
	if (r->branch[0] == '\0')
		snprintf(r->branch, sizeof(r->branch), "-");

	/* Parse status from porcelain lines (skip first branch line) */
	p = r->stdout_buf;
	while (*p && *p != '\n')
		p++;
	if (*p == '\n')
		p++;

	int modified = 0, added = 0, deleted = 0, untracked = 0, other = 0;
	while (*p) {
		char x = *p;
		char y = (p[1] && p[1] != '\n') ? p[1] : ' ';
		if (x == '?' || y == '?')
			untracked++;
		else if (x == 'M' || y == 'M')
			modified++;
		else if (x == 'A' || y == 'A')
			added++;
		else if (x == 'D' || y == 'D')
			deleted++;
		else
			other++;
		while (*p && *p != '\n')
			p++;
		if (*p == '\n')
			p++;
	}

	char  *sp        = r->status_buf;
	size_t remaining = sizeof(r->status_buf);
	int n = 0;
	if (modified > 0 && (size_t) n < remaining)  n += snprintf(sp + n, remaining - (size_t) n, "%dm ", modified);
	if (added > 0 && (size_t) n < remaining)     n += snprintf(sp + n, remaining - (size_t) n, "%da ", added);
	if (deleted > 0 && (size_t) n < remaining)   n += snprintf(sp + n, remaining - (size_t) n, "%dd ", deleted);
	if (untracked > 0 && (size_t) n < remaining) n += snprintf(sp + n, remaining - (size_t) n, "%du ", untracked);
	if (other > 0 && (size_t) n < remaining)     n += snprintf(sp + n, remaining - (size_t) n, "%do ", other);
	if (n > 0)
		r->status_buf[n - 1] = '\0';
	else
		snprintf(r->status_buf, sizeof(r->status_buf), "clean");
}

static void status_result_free(StatusResult *r)
{
	free(r->stdout_buf);
	r->stdout_buf = NULL;
}

static void print_header(const char *name, const char *path, bool color)
{
	if (color)
		fprintf(stderr,
		        "\n%s%s%s%s %s(%s)%s\n",
		        ANSI_BOLD,
		        ANSI_FG_CYAN,
		        name,
		        ANSI_RESET,
		        ANSI_DIM,
		        path,
		        ANSI_RESET);
	else
		fprintf(stderr, "\n%s (%s)\n", name, path);
}

static void print_clean(bool color)
{
	if (color)
		fprintf(stderr, "  %sgreen%s\n", ANSI_FG_GREEN, ANSI_RESET);
	else
		fprintf(stderr, "  clean\n");
}

static void print_error(bool color)
{
	if (color)
		fprintf(stderr, "  %serror: could not get status%s\n", ANSI_FG_RED, ANSI_RESET);
	else
		fprintf(stderr, "  error: could not get status\n");
}

static void print_status_line(const char *line, bool color)
{
	if (!color) {
		fprintf(stderr, "  %s\n", line);
		return;
	}

	char x = line[0];
	char y = line[1];

	if (x == '?' || y == '?')
		fprintf(stderr, "  %s%s%s\n", ANSI_FG_RED, line, ANSI_RESET);
	else if (x == 'D' || y == 'D')
		fprintf(stderr, "  %s%s%s\n", ANSI_FG_RED, line, ANSI_RESET);
	else if (x == 'A' || y == 'A')
		fprintf(stderr, "  %s%s%s\n", ANSI_FG_GREEN, line, ANSI_RESET);
	else if (x == 'M' || y == 'M')
		fprintf(stderr, "  %s%s%s\n", ANSI_FG_YELLOW, line, ANSI_RESET);
	else if (x == 'R' || y == 'R')
		fprintf(stderr, "  %s%s%s\n", ANSI_FG_CYAN, line, ANSI_RESET);
	else
		fprintf(stderr, "  %s\n", line);
}

static int cmd_status(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_status");
	bool color = CMD_COLOR();

	GitConfig cfg         = { 0 };
	char     *config_path = NULL;

	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group, indices, cfg.count);

	LOG_DEBUG("filtered to %zu repos", filtered);

	/* Phase 1: parallel collection (1 git call per repo instead of 2) */
	StatusResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered, status_collect, sizeof(StatusResult), results);

	/* Phase 2: display sequentially */
	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Name", "Status", "Branch" };
		Table      *t         = table_create(3, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++) {
			RepoEntry    *e = &cfg.entries[indices[i]];
			StatusResult *r = &results[i];

			const char *status_str = r->exit_code != 0 ? "error" : r->status_buf;

			if (color) {
				const char *code = ANSI_FG_YELLOW;
				if (strcmp(status_str, "clean") == 0)
					code = ANSI_FG_GREEN;
				else if (strcmp(status_str, "error") == 0)
					code = ANSI_FG_RED;
				char colored[MAX_NAME_LEN];
				ansi_colorize(colored, sizeof(colored), status_str, code);
				const char *cells[] = { e->name, colored, r->branch };
				table_add_row_raw(t, cells, 3);
			} else {
				table_add_row(t, e->name, status_str, r->branch);
			}
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			RepoEntry    *e = &cfg.entries[indices[i]];
			StatusResult *r = &results[i];

			print_header(e->name, e->path, color);

			if (r->exit_code != 0) {
				print_error(color);
			} else if (r->stdout_len == 0) {
				print_clean(color);
			} else {
				const char *p = r->stdout_buf;
				/* Skip branch line */
				while (*p && *p != '\n')
					p++;
				if (*p == '\n')
					p++;

				while (*p) {
					const char *start = p;
					while (*p && *p != '\n')
						p++;

					char   line[MAX_PATH_LEN];
					size_t len = (size_t) (p - start);
					if (len >= sizeof(line))
						len = sizeof(line) - 1;
					memcpy(line, start, len);
					line[len] = '\0';

					print_status_line(line, color);

					if (*p == '\n')
						p++;
				}
			}
		}
	}

	/* Phase 3: cleanup */
	for (size_t i = 0; i < filtered; i++)
		status_result_free(&results[i]);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_status(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "status", "Show status of all registered repos", cmd_status);
	const char *status_aliases[] = { "st", "s" };
	argparse_command_set_aliases(cmd, status_aliases, 2);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
