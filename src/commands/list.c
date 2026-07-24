/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * list.c — `gitm list` command
 *
 * Prints all registered repositories.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "config.h"
#include "log.h"

static const char *list_filter_tag   = NULL;
static const char *list_filter_group = NULL;

int cmd_list(const ArgParseResult *result)
{
	(void) result;

	char *path = config_default_path();
	if (!path) {
		LOG_ERROR("could not determine config path");
		return 1;
	}

	GitConfig cfg = { 0 };
	if (config_load(path, &cfg) != 0) {
		LOG_ERROR("could not load config");
		free(path);
		return 1;
	}

	if (cfg.count == 0) {
		fprintf(stderr, "No repositories registered.\n");
		fprintf(stderr, "Use 'gitm add <path> [name]' to register one.\n");
		config_free(&cfg);
		free(path);
		return 0;
	}

	int shown = 0;
	for (size_t i = 0; i < cfg.count; i++) {
		if (list_filter_tag && !config_entry_has_tag(&cfg.entries[i], list_filter_tag))
			continue;
		if (list_filter_group && !config_entry_has_group(&cfg.entries[i], list_filter_group))
			continue;

		LOG_TRACE("listing: %s (%s)", cfg.entries[i].name, cfg.entries[i].path);
		fprintf(stdout, "%s\t%s\n", cfg.entries[i].name, cfg.entries[i].path);
		shown++;
	}

	if (shown == 0)
		fprintf(stderr, "No repos match the given filters.\n");

	config_free(&cfg);
	free(path);
	return 0;
}

void cmd_register_list(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "list", "List registered repositories", cmd_list);
	const char *list_aliases[] = { "ls" };
	argparse_command_set_aliases(cmd, list_aliases, 1);
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &list_filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &list_filter_group);
	(void) cmd;
}
