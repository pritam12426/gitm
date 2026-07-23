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

	for (size_t i = 0; i < cfg.count; i++) {
		fprintf(stdout, "%s\t%s\n", cfg.entries[i].name, cfg.entries[i].path);
	}

	config_free(&cfg);
	free(path);
	return 0;
}

void cmd_register_list(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "list", "List registered repositories", cmd_list);
	(void) cmd;
}
