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

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "log.h"
#include "table.h"

static const char *list_filter_tag   = NULL;
static const char *list_filter_group = NULL;

static int cmd_list(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_list");
	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		fprintf(stderr, "Use 'gitm add <path> [name]' to register one.\n");
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	size_t indices[MAX_REPOS];
	size_t filtered
	    = cmd_filter_entries(&cfg, list_filter_tag, list_filter_group, indices, MAX_REPOS);

	LOG_DEBUG("filtered to %zu repos (tag=%s, group=%s)",
	          filtered,
	          list_filter_tag ? list_filter_tag : "-",
	          list_filter_group ? list_filter_group : "-");

	if (filtered == 0) {
		fprintf(stderr, "No repos match the given filters.\n");
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Name", "Path", "Tags", "Groups" };
		Table      *t         = table_create(4, headers);
		table_set_color(t, CMD_COLOR());

		for (size_t i = 0; i < filtered; i++) {
			RepoEntry *e = &cfg.entries[indices[i]];
			table_add_row(
			    t, e->name, e->path, e->tags[0] ? e->tags : "-", e->groups[0] ? e->groups : "-");
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			RepoEntry *e = &cfg.entries[indices[i]];
			cmd_print_name_path(stdout, e->name, e->path);
		}
	}

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_list(ArgParser *parser)
{
	ArgCommand *cmd
	    = argparse_add_command(parser, "list", "List registered repositories", cmd_list);
	const char *list_aliases[] = { "ls" };
	argparse_command_set_aliases(cmd, list_aliases, 1);
	cmd_register_filter_flags(cmd, &list_filter_tag, &list_filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
