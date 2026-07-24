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
#include "config.h"
#include "log.h"

int cmd_rename(const ArgParseResult *result)
{
	if (result->positional_count < 2) {
		fprintf(stderr, "Usage: gitm rename <old-name> <new-name>\n");
		return 1;
	}

	const char *old_name = result->positionals[0];
	const char *new_name = result->positionals[1];

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

	if (config_rename(&cfg, old_name, new_name) != 0) {
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	if (config_save(config_path, &cfg) != 0) {
		LOG_ERROR("could not save config");
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	fprintf(stderr, "Renamed %s -> %s\n", old_name, new_name);

	config_free(&cfg);
	free(config_path);
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
