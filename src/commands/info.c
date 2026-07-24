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
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_info(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm info <name>\n");
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

	fprintf(stderr, "Name:      %s\n", entry->name);
	fprintf(stderr, "Path:      %s\n", entry->path);
	if (entry->tags)
		fprintf(stderr, "Tags:      %s\n", entry->tags);
	if (entry->groups)
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
		const char *p = r.stdout_buf;
		while (*p) {
			fprintf(stderr, "  ");
			while (*p && *p != '\n')
				fputc(*p++, stderr);
			if (*p == '\n') {
				fputc('\n', stderr);
				p++;
			}
		}
	}
	process_result_free(&r);

	/* Last commit */
	ProcessResult r2 = git_exec(entry->path, "log", "-1", "--format=%h %s (%ar)", NULL);
	if (r2.exit_code == 0 && r2.stdout_len > 0) {
		fprintf(stderr, "Last:      %s", r2.stdout_buf);
	}
	process_result_free(&r2);

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_info(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "info", "Show repository metadata", cmd_info);
	const char *info_aliases[] = { "i" };
	argparse_command_set_aliases(cmd, info_aliases, 1);
	argparse_add_positional(cmd, "name");
}
