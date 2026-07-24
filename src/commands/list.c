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
#include "table.h"

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

	/* Collect filtered entries */
	size_t *indices = calloc(cfg.count, sizeof(size_t));
	size_t  filtered = 0;

	for (size_t i = 0; i < cfg.count; i++) {
		if (list_filter_tag && !config_entry_has_tag(&cfg.entries[i], list_filter_tag))
			continue;
		if (list_filter_group && !config_entry_has_group(&cfg.entries[i], list_filter_group))
			continue;
		indices[filtered++] = i;
	}

	if (filtered == 0) {
		fprintf(stderr, "No repos match the given filters.\n");
		free(indices);
		config_free(&cfg);
		free(path);
		return 0;
	}

	if (g_table_mode) {
		const char *headers[] = { "Name", "Path", "Tags", "Groups" };
		Table *t = table_create(4, headers);
		table_set_color(t, log_use_color());

		for (size_t i = 0; i < filtered; i++) {
			RepoEntry *e = &cfg.entries[indices[i]];
			table_add_row(t,
			              e->name,
			              e->path,
			              e->tags ? e->tags : "-",
			              e->groups ? e->groups : "-");
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			RepoEntry *e = &cfg.entries[indices[i]];
			fprintf(stdout, "%s\t%s\n", e->name, e->path);
		}
	}

	free(indices);
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
	cmd_register_table_flag(cmd);
	(void) cmd;
}
