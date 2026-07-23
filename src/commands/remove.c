/*
 * remove.c — `gitm remove` command
 *
 * Unregisters a repository by name.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "config.h"
#include "log.h"

int cmd_remove(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm remove <name>\n");
		return 1;
	}

	const char *name = result->positionals[0];

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

	if (config_remove(&cfg, name) != 0) {
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

	fprintf(stderr, "Removed %s\n", name);

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_remove(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "remove", "Unregister a repository", cmd_remove);
	argparse_add_positional(cmd, "name");
}
