/*
 * branch.c — `gitm branch` command
 *
 * Shows local branches for all registered repos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

static void print_branches(const char *name, const char *path, bool color)
{
	ProcessResult r = git_exec(path, "branch", "--list", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no branches)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no branches)\n", name);
		process_result_free(&r);
		return;
	}

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n", name);
	else
		fprintf(stderr, "\n%s\n", name);

	const char *p = r.stdout_buf;
	while (*p) {
		// Skip leading spaces
		while (*p == ' ')
			p++;

		const char *start = p;
		while (*p && *p != '\n')
			p++;

		size_t len = (size_t) (p - start);
		char   line[256];
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, start, len);
		line[len] = '\0';

		// Detect current branch (* prefix)
		if (line[0] == '*' && line[1] == ' ') {
			if (color)
				fprintf(stderr, "  \x1b[1m\x1b[32m* %s\x1b[0m\n", line + 2);
			else
				fprintf(stderr, "  * %s\n", line + 2);
		} else {
			fprintf(stderr, "    %s\n", line);
		}

		if (*p == '\n')
			p++;
	}

	process_result_free(&r);
}

int cmd_branch(const ArgParseResult *result)
{
	(void) result;

	bool color = log_use_color();

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

	for (size_t i = 0; i < cfg.count; i++) {
		print_branches(cfg.entries[i].name, cfg.entries[i].path, color);
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_branch(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "branch",
	                                       "Show local branches",
	                                       cmd_branch);
	(void) cmd;
}
