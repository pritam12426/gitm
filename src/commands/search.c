/*
 * search.c — `gitm search PATTERN` command
 *
 * Searches repos by name or path using substring matching.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cmd.h"
#include "config.h"
#include "log.h"

int cmd_search(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm search PATTERN\n");
		return 1;
	}

	const char *pattern = result->positionals[0];

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

	int found = 0;
	for (size_t i = 0; i < cfg.count; i++) {
		if (strcasestr(cfg.entries[i].name, pattern) != NULL ||
		    strcasestr(cfg.entries[i].path, pattern) != NULL) {
			fprintf(stdout, "%s\t%s\n", cfg.entries[i].name, cfg.entries[i].path);
			found++;
		}
	}

	if (found == 0)
		fprintf(stderr, "No repos matching '%s'\n", pattern);

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_search(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "search",
	                                       "Search repos by name or path pattern",
	                                       cmd_search);
	(void) cmd;
}
