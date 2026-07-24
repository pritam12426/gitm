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
#include "cmd_util.h"
#include "config.h"
#include "log.h"
#include "table.h"

static const char *list_filter_tag   = NULL;
static const char *list_filter_group = NULL;

int cmd_list(const ArgParseResult *result)
{
	(void) result;

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, "No repositories registered.\n");
		fprintf(stderr, "Use 'gitm add <path> [name]' to register one.\n");
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	size_t *indices = calloc(cfg.count, sizeof(size_t));
	size_t  filtered = cmd_filter_entries(&cfg, list_filter_tag, list_filter_group,
	                                     indices, cfg.count);

	if (filtered == 0) {
		fprintf(stderr, "No repos match the given filters.\n");
		free(indices);
		config_free(&cfg);
		free(config_path);
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
	free(config_path);
	return 0;
}

void cmd_register_list(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "list", "List registered repositories", cmd_list);
	const char *list_aliases[] = { "ls" };
	argparse_command_set_aliases(cmd, list_aliases, 1);
	cmd_register_filter_flags(cmd, &list_filter_tag, &list_filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
