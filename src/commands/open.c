/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * open.c — `gitm open` command
 *
 * Opens a registered repository in $EDITOR or file manager.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"

static int cmd_open(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm open <name>\n");
		return 1;
	}

	LOG_TRACE("cmd_open");
	const char *name = result->positionals[0];
	LOG_DEBUG("opening repo: %s", name);

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	RepoEntry *entry = config_find(&cfg, name);
	if (!entry) {
		fprintf(stderr, "Repository not found: %s\n", name);
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	const char *editor = getenv("EDITOR");
	if (!editor)
		editor = getenv("VISUAL");
	if (!editor) {
		LOG_ERROR("no $EDITOR or $VISUAL set");
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	fprintf(stderr, "Opening %s in %s\n", entry->path, editor);
	LOG_INFO("opening %s in %s", entry->path, editor);

	ProcessResult r  = process_exec(entry->path,
                                   (char *const *) (const char *[]) { editor, entry->path, NULL });
	int           rc = r.exit_code;
	process_result_free(&r);

	cmd_cleanup(&cfg, config_path);
	return rc;
}

void cmd_register_open(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "open", "Open a repository in $EDITOR", cmd_open);
	const char *open_aliases[] = { "o" };
	argparse_command_set_aliases(cmd, open_aliases, 1);
	argparse_add_positional(cmd, "name");
}
