/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * exec.c — `gitm exec` command
 *
 * Runs a git command on a registered repository.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_exec(const ArgParseResult *result)
{
	if (result->positional_count < 2) {
		fprintf(stderr, "Usage: gitm exec <name> <git-command> [args...]\n");
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

	/* Build git command: "git" + remaining positionals */
	const char *git_argv[64];
	int         git_argc = 0;
	git_argv[git_argc++] = "git";

	for (int i = 1; i < result->positional_count && git_argc < 62; i++) {
		git_argv[git_argc++] = result->positionals[i];
	}
	git_argv[git_argc] = NULL;

	ProcessResult r = process_exec(entry->path, (char *const *) git_argv);

	if (r.stdout_len > 0)
		fwrite(r.stdout_buf, 1, r.stdout_len, stdout);
	if (r.stderr_len > 0)
		fwrite(r.stderr_buf, 1, r.stderr_len, stderr);

	int rc = r.exit_code;
	process_result_free(&r);

	config_free(&cfg);
	free(config_path);
	return rc;
}

void cmd_register_exec(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "exec",
	                                       "Run a git command on a registered repo",
	                                       cmd_exec);
	const char *exec_aliases[] = { "x" };
	argparse_command_set_aliases(cmd, exec_aliases, 1);
	argparse_add_positional(cmd, "name");
	argparse_add_positional(cmd, "command...");
}
