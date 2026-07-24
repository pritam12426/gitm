/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

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

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

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
	size_t checked = 0;

	LOG_DEBUG("running health check on %zu repos", cfg.count);

	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		checked++;
		LOG_TRACE("checking %s", cfg.entries[i].name);
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

	fprintf(stderr, "\n%d/%zu repositories OK\n", (int) (checked - (size_t) errors), checked);

	if (errors > 0)
		LOG_WARN("%d/%zu repos failed health check", errors, checked);
	else
		LOG_INFO("all %zu repos passed health check", checked);

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
	const char *doctor_aliases[] = { "doc", "d" };
	argparse_command_set_aliases(cmd, doctor_aliases, 2);
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", &filter_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", &filter_group);
	(void) cmd;
}
