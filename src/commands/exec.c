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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"

static int cmd_exec(const ArgParseResult *result)
{
	if (result->positional_count < 2) {
		fprintf(stderr, "Usage: gitm exec <name> <git-command> [args...]\n");
		return 1;
	}

	LOG_TRACE("cmd_exec");
	const char *name = result->positionals[0];
	LOG_DEBUG("exec on %s: %s", name, result->positional_count > 1 ? result->positionals[1] : "(none)");

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	RepoEntry *entry = config_find(&cfg, name);
	if (!entry) {
		fprintf(stderr, MSG_REPO_NOT_FOUND, name);
		cmd_cleanup(&cfg, config_path);
		return 1;
	}

	/* Build git command: "git" + remaining positionals */
	const char *git_argv[GIT_MAX_ARGS];
	int         git_argc = 0;
	git_argv[git_argc++] = GIT_BINARY;

	for (int i = 1; i < result->positional_count && git_argc < GIT_MAX_ARGS - 2; i++) {
		git_argv[git_argc++] = result->positionals[i];
	}
	git_argv[git_argc] = NULL;

	ProcessResult r = process_exec(entry->path, (char *const *) git_argv);

	if (r.stdout_len > 0 && fwrite(r.stdout_buf, 1, r.stdout_len, stdout) != r.stdout_len) {
		if (errno != EPIPE)
			LOG_ERROR("fwrite stdout: %s", strerror(errno));
	}
	if (r.stderr_len > 0 && fwrite(r.stderr_buf, 1, r.stderr_len, stderr) != r.stderr_len) {
		if (errno != EPIPE)
			LOG_ERROR("fwrite stderr: %s", strerror(errno));
	}

	int rc = r.exit_code;
	process_result_free(&r);

	cmd_cleanup(&cfg, config_path);
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
