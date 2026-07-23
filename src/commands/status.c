/*
 * status.c — `gitm status` command
 *
 * Shows git status for all registered repos.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_status(const ArgParseResult *result)
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

	for (size_t i = 0; i < cfg.count; i++) {
		fprintf(stderr, "\n%s (%s)\n", cfg.entries[i].name, cfg.entries[i].path);

		ProcessResult r = git_exec(cfg.entries[i].path, "status", "--porcelain", "--branch", NULL);

		if (r.exit_code != 0) {
			fprintf(stderr, "  error: could not get status\n");
		} else if (r.stdout_len == 0) {
			fprintf(stderr, "  clean\n");
		} else {
			/* Print indented status lines */
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
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_status(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "status",
	                                       "Show status of all registered repos",
	                                       cmd_status);
	(void) cmd;
}
