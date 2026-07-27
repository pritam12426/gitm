/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * info.c — `gitm info` command
 *
 * Shows metadata about a registered repository.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"

static int cmd_info(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm info <name>\n");
		return 1;
	}

	LOG_TRACE("cmd_info");
	const char *name = result->positionals[0];
	LOG_DEBUG("showing info for: %s", name);

	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	RepoEntry *entry = cmd_find_repo_or_fail(&cfg, config_path, name);
	if (!entry)
		return 1;

	fprintf(stderr, "Name:      %s\n", entry->name);
	fprintf(stderr, "Path:      %s\n", entry->path);
	if (entry->tags[0])
		fprintf(stderr, "Tags:      %s\n", entry->tags);
	if (entry->groups[0])
		fprintf(stderr, "Groups:    %s\n", entry->groups);

	/* Branch */
	char *branch = git_current_branch(entry->path);
	if (branch) {
		fprintf(stderr, "Branch:    %s\n", branch);
		free(branch);
	}

	/* Remotes */
	ProcessResult r = git_exec(entry->path, "remote", "-v", NULL);
	if (r.exit_code == 0 && r.stdout_len > 0) {
		fprintf(stderr, "Remotes:\n");
		fwrite(r.stdout_buf, 1, r.stdout_len, stderr);
	}
	process_result_free(&r);

	/* Last commit */
	ProcessResult r2 = git_exec(entry->path, "log", "-1", "--format=%h %s (%ar)", NULL);
	if (r2.exit_code == 0 && r2.stdout_len > 0) {
		fprintf(stderr, "Last:      %s", r2.stdout_buf);
	}
	process_result_free(&r2);

	cmd_cleanup(&cfg, config_path);
	return 0;
}

void cmd_register_info(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "info", "Show repository metadata", cmd_info);
	const char *info_aliases[] = { "i" };
	argparse_command_set_aliases(cmd, info_aliases, 1);
	argparse_add_positional(cmd, "name");
}
