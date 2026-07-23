/*
 * doctor.c — `gitm doctor` command
 *
 * Health check for all registered repositories.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_doctor(const ArgParseResult *result)
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

	int errors = 0;

	for (size_t i = 0; i < cfg.count; i++) {
		fprintf(stderr, "%s ... ", cfg.entries[i].name);

		struct stat st;
		if (stat(cfg.entries[i].path, &st) != 0) {
			fprintf(stderr, "MISSING\n");
			errors++;
			continue;
		}
		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "NOT A DIRECTORY\n");
			errors++;
			continue;
		}
		if (!git_is_repo(cfg.entries[i].path)) {
			fprintf(stderr, "NOT A GIT REPO\n");
			errors++;
			continue;
		}

		fprintf(stderr, "ok\n");
	}

	fprintf(stderr, "\n%d/%zu repositories OK\n", (int) (cfg.count - (size_t) errors), cfg.count);

	config_free(&cfg);
	free(config_path);
	return errors > 0 ? 1 : 0;
}

void cmd_register_doctor(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "doctor",
	                                       "Health check all registered repositories",
	                                       cmd_doctor);
	(void) cmd;
}
