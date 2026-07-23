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
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_open(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm open <name>\n");
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

	RepoEntry *entry = config_find(&cfg, name);
	if (!entry) {
		fprintf(stderr, "Repository not found: %s\n", name);
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	const char *editor = getenv("EDITOR");
	if (!editor)
		editor = getenv("VISUAL");
	if (!editor) {
		LOG_ERROR("no $EDITOR or $VISUAL set");
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	fprintf(stderr, "Opening %s in %s\n", entry->path, editor);

	ProcessResult r  = process_exec(entry->path,
                                   (char *const *) (const char *[]) { editor, entry->path, NULL });
	int           rc = r.exit_code;
	process_result_free(&r);

	config_free(&cfg);
	free(config_path);
	return rc;
}

void cmd_register_open(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "open", "Open a repository in $EDITOR", cmd_open);
	argparse_add_positional(cmd, "name");
}
