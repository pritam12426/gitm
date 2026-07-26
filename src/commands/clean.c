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
#include "cmd_util.h"
#include "config.h"
#include "log.h"

static bool g_dry_run = false;

static int cmd_clean(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_clean");
	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	/* Find orphans */
	size_t orphans[MAX_REPOS];
	size_t orphan_count = config_find_orphans(&cfg, orphans, MAX_REPOS);

	LOG_DEBUG("found %zu orphaned repos", orphan_count);

	if (orphan_count == 0) {
		fprintf(stderr, "No orphaned repositories found.\n");
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	/* List orphans */
	fprintf(stderr, "Found %zu orphaned %s:\n", orphan_count,
	        PLURAL(orphan_count, "repository", "repositories"));
	for (size_t i = 0; i < orphan_count; i++) {
		size_t idx = orphans[i];
		fprintf(stderr, "  %s (%s)\n", cfg.entries[idx].name, cfg.entries[idx].path);
	}

	if (g_dry_run) {
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	/* Confirm removal */
	fprintf(stderr, "\nRemove %zu %s? [y/N] ", orphan_count,
	        PLURAL(orphan_count, "entry", "entries"));
	fflush(stderr);

	int ch = getchar();
	/* Consume rest of line to prevent stale input on next prompt */
	if (ch != EOF && ch != '\n') {
		int extra;
		while ((extra = getchar()) != EOF && extra != '\n')
			;
	}
	if (ch != 'y' && ch != 'Y') {
		fprintf(stderr, "Aborted.\n");
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	/* Remove orphans (indices are already in order from config_find_orphans) */
	if (config_remove_at_indices(&cfg, orphans, orphan_count) != 0) {
		LOG_ERROR("failed to remove orphans");
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	if (cmd_save_config(&cfg, config_path) != 0)
		return 1;

	fprintf(stderr, "Removed %zu %s.\n", orphan_count,
	        PLURAL(orphan_count, "entry", "entries"));

	LOG_INFO("removed %zu orphaned entries", orphan_count);

	cmd_cleanup(&cfg, config_path);
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
