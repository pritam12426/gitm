/*
 * remote.c — `gitm remote` command
 *
 * Shows remote settings for all registered repos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

static void print_remotes(const char *name, const char *path, bool color)
{
	ProcessResult r = git_exec(path, "remote", "-v", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no remotes)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no remotes)\n", name);
		process_result_free(&r);
		return;
	}

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n", name);
	else
		fprintf(stderr, "\n%s\n", name);

	const char *p = r.stdout_buf;
	while (*p) {
		const char *start = p;
		while (*p && *p != '\n')
			p++;

		size_t len = (size_t) (p - start);
		char   line[512];
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, start, len);
		line[len] = '\0';

		// Color the remote name (first word)
		char *space = strchr(line, ' ');
		if (space && color) {
			*space = '\0';
			fprintf(stderr, "  \x1b[33m%-10s\x1b[0m %s\n", line, space + 1);
		} else {
			fprintf(stderr, "  %s\n", line);
		}

		if (*p == '\n')
			p++;
	}

	process_result_free(&r);
}

int cmd_remote(const ArgParseResult *result)
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
		print_remotes(cfg.entries[i].name, cfg.entries[i].path, color);
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_remote(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "remote",
	                                       "Show remote settings",
	                                       cmd_remote);
	(void) cmd;
}
