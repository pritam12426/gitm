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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

// ANSI codes
#define C_RESET  "\x1b[0m"
#define C_BOLD   "\x1b[1m"
#define C_RED    "\x1b[31m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_CYAN   "\x1b[36m"
#define C_DIM    "\x1b[2m"

static void print_header(const char *name, const char *path, bool color)
{
	if (color)
		fprintf(stderr, "\n%s%s%s%s %s(%s)%s\n", C_BOLD, C_CYAN, name, C_RESET, C_DIM, path, C_RESET);
	else
		fprintf(stderr, "\n%s (%s)\n", name, path);
}

static void print_clean(bool color)
{
	if (color)
		fprintf(stderr, "  %sgreen%s\n", C_GREEN, C_RESET);
	else
		fprintf(stderr, "  clean\n");
}

static void print_error(bool color)
{
	if (color)
		fprintf(stderr, "  %serror: could not get status%s\n", C_RED, C_RESET);
	else
		fprintf(stderr, "  error: could not get status\n");
}

// Colour the status line based on the XY code
static void print_status_line(const char *line, bool color)
{
	if (!color) {
		fprintf(stderr, "  %s\n", line);
		return;
	}

	// Parse XY prefix (first two chars)
	char x = line[0];
	char y = line[1];

	// Determine colour from the more "severe" status
	if (x == '?' || y == '?')
		fprintf(stderr, "  %s%s%s\n", C_RED, line, C_RESET);
	else if (x == 'D' || y == 'D')
		fprintf(stderr, "  %s%s%s\n", C_RED, line, C_RESET);
	else if (x == 'A' || y == 'A')
		fprintf(stderr, "  %s%s%s\n", C_GREEN, line, C_RESET);
	else if (x == 'M' || y == 'M')
		fprintf(stderr, "  %s%s%s\n", C_YELLOW, line, C_RESET);
	else if (x == 'R' || y == 'R')
		fprintf(stderr, "  %s%s%s\n", C_CYAN, line, C_RESET);
	else
		fprintf(stderr, "  %s\n", line);
}

int cmd_status(const ArgParseResult *result)
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

	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		print_header(cfg.entries[i].name, cfg.entries[i].path, color);

		ProcessResult r = git_exec(cfg.entries[i].path, "status", "--porcelain", "--branch", NULL);

		if (r.exit_code != 0) {
			print_error(color);
		} else if (r.stdout_len == 0) {
			print_clean(color);
		} else {
			// Print each status line with colour
			const char *p = r.stdout_buf;
			while (*p) {
				const char *start = p;
				while (*p && *p != '\n')
					p++;

				// Print the line with colour
				char line[512];
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

		process_result_free(&r);
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_status(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "status",
	                                       "Show status of all registered repos",
	                                       cmd_status);
	const char *status_aliases[] = { "st", "s" };
	argparse_command_set_aliases(cmd, status_aliases, 2);
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &filter_group);
	(void) cmd;
}
