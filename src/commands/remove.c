/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * remove.c — `gitm remove` command
 *
 * Unregisters a repository by name.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "log.h"

static int cmd_remove(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm remove <name>\n");
		return 1;
	}

	LOG_TRACE("cmd_remove");
	const char *name = result->positionals[0];
	LOG_DEBUG("removing repo: %s", name);

	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (config_remove(&cfg, name) != 0) {
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	if (cmd_save_config(&cfg, config_path) != 0)
		return 1;

	fprintf(stderr, "Removed %s\n", name);
	LOG_INFO("removed %s", name);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_remove(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "remove", "Unregister a repository", cmd_remove);
	const char *remove_aliases[] = { "rm" };
	argparse_command_set_aliases(cmd, remove_aliases, 1);
	argparse_add_positional(cmd, "name");
}
