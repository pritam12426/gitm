/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * rename.c — `gitm rename` command
 *
 * Renames a repository alias.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "log.h"

static int cmd_rename(const ArgParseResult *result)
{
	if (result->positional_count < 2) {
		fprintf(stderr, "Usage: gitm rename <old-name> <new-name>\n");
		return 1;
	}

	LOG_TRACE("cmd_rename");
	const char *old_name = result->positionals[0];
	const char *new_name = result->positionals[1];
	LOG_DEBUG("renaming %s -> %s", old_name, new_name);

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (config_rename(&cfg, old_name, new_name) != 0) {
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	if (cmd_save_config(&cfg, config_path) != 0)
		return 1;

	fprintf(stderr, "Renamed %s -> %s\n", old_name, new_name);
	LOG_INFO("renamed %s -> %s", old_name, new_name);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_rename(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "rename",
	                                       "Rename a repository alias",
	                                       cmd_rename);
	argparse_add_positional(cmd, "old-name");
	argparse_add_positional(cmd, "new-name");
}
