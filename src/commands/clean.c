/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * clean.c — `gitm clean` command
 *
 * Finds and removes repos that no longer exist on disk.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "config.h"
#include "log.h"

static bool g_dry_run = false;

int cmd_clean(const ArgParseResult *result)
{
	(void) result;

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

	/* Find orphans */
	size_t orphans[256];
	size_t orphan_count = config_find_orphans(&cfg, orphans, 256);

	if (orphan_count == 0) {
		fprintf(stderr, "No orphaned repositories found.\n");
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	/* List orphans */
	fprintf(stderr, "Found %zu orphaned %s:\n", orphan_count,
	        orphan_count == 1 ? "repository" : "repositories");
	for (size_t i = 0; i < orphan_count; i++) {
		size_t idx = orphans[i];
		fprintf(stderr, "  %s (%s)\n", cfg.entries[idx].name, cfg.entries[idx].path);
	}

	if (g_dry_run) {
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	/* Confirm removal */
	fprintf(stderr, "\nRemove %zu %s? [y/N] ", orphan_count,
	        orphan_count == 1 ? "entry" : "entries");
	fflush(stderr);

	int ch = getchar();
	if (ch != 'y' && ch != 'Y') {
		fprintf(stderr, "Aborted.\n");
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	/* Remove orphans (indices are already in order from config_find_orphans) */
	if (config_remove_at_indices(&cfg, orphans, orphan_count) != 0) {
		LOG_ERROR("failed to remove orphans");
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

	fprintf(stderr, "Removed %zu %s.\n", orphan_count,
	        orphan_count == 1 ? "entry" : "entries");

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_clean(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "clean",
	                                       "Remove repos that no longer exist on disk",
	                                       cmd_clean);
	const char *clean_aliases[] = { "prune" };
	argparse_command_set_aliases(cmd, clean_aliases, 1);
	argparse_add_option(cmd, "dry-run", 'n', ARG_TYPE_NONE, NULL,
	                    "Show orphans without removing", &g_dry_run);
	(void) cmd;
}
