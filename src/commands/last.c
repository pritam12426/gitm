/*
 * last.c — `gitm last` command
 *
 * Shows the last commit log for each registered repo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

static void print_last(const char *name, const char *path, bool color)
{
	// Format: hash author date subject
	ProcessResult r = git_exec(path,
	                           "log", "-1",
	                           "--format=%h %an %ar %s",
	                           "HEAD", NULL);

	if (r.exit_code != 0 || r.stdout_len == 0) {
		if (color)
			fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n  \x1b[2m(no commits)\x1b[0m\n", name);
		else
			fprintf(stderr, "\n%s\n  (no commits)\n", name);
		process_result_free(&r);
		return;
	}

	// Strip newline
	char *line = strdup(r.stdout_buf);
	size_t len = strlen(line);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';

	// Parse: hash author date subject
	char *p = line;

	// Hash (first word)
	char *hash = p;
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';

	// Author (until we hit a date pattern like "2 hours ago")
	char *author = p;
	while (*p && !((*p >= '0' && *p <= '9') && *(p + 1) == ' ' &&
	               (*(p + 2) == 'm' || *(p + 2) == 'h' || *(p + 2) == 'd' ||
	                *(p + 2) == 'w' || *(p + 2) == 'y' || *(p + 2) == 's')))
		p++;

	// Find the date part
	char *date = p;
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';
	// Skip "ago"
	while (*p && *p != ' ')
		p++;
	if (*p) *p++ = '\0';

	char *subject = p;

	if (color)
		fprintf(stderr, "\n\x1b[1m\x1b[36m%s\x1b[0m\n"
		                "  \x1b[33m%s\x1b[0m %s \x1b[2m%s\x1b[0m %s\n",
		        name, hash, author, date, subject);
	else
		fprintf(stderr, "\n%s\n  %s %s %s %s\n",
		        name, hash, author, date, subject);

	free(line);
	process_result_free(&r);
}

int cmd_last(const ArgParseResult *result)
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
		print_last(cfg.entries[i].name, cfg.entries[i].path, color);
	}

	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_last(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "last",
	                                       "Show last commit log for each repo",
	                                       cmd_last);
	(void) cmd;
}
