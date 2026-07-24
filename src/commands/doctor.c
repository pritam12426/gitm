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
#include <string.h>
#include <sys/stat.h>

#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "table.h"

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

	/* Collect results */
	size_t *indices   = calloc(cfg.count, sizeof(size_t));
	char  **statuses  = calloc(cfg.count, sizeof(char *));
	size_t  checked   = 0;
	int     errors    = 0;

	LOG_DEBUG("running health check on %zu repos", cfg.count);

	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		LOG_TRACE("checking %s", cfg.entries[i].name);

		struct stat st;
		if (stat(cfg.entries[i].path, &st) != 0) {
			statuses[checked] = strdup("MISSING");
			errors++;
		} else if (!S_ISDIR(st.st_mode)) {
			statuses[checked] = strdup("NOT A DIRECTORY");
			errors++;
		} else if (!git_is_repo(cfg.entries[i].path)) {
			statuses[checked] = strdup("NOT A GIT REPO");
			errors++;
		} else {
			statuses[checked] = strdup("ok");
		}

		indices[checked] = i;
		checked++;
	}

	if (g_table_mode) {
		const char *headers[] = { "Name", "Status" };
		Table *t = table_create(2, headers);
		table_set_color(t, log_use_color());

		for (size_t i = 0; i < checked; i++) {
			const char *name = cfg.entries[indices[i]].name;
			const char *status = statuses[i];
			bool is_ok = (strcmp(status, "ok") == 0);

			if (log_use_color() && is_ok) {
				char colored[128];
				snprintf(colored, sizeof(colored), "\x1b[32m%s\x1b[0m", status);
				const char *cells[] = { name, colored };
				table_add_row_raw(t, cells, 2);
			} else if (log_use_color() && !is_ok) {
				char colored[128];
				snprintf(colored, sizeof(colored), "\x1b[31m%s\x1b[0m", status);
				const char *cells[] = { name, colored };
				table_add_row_raw(t, cells, 2);
			} else {
				table_add_row(t, name, status);
			}
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < checked; i++) {
			fprintf(stderr, "%s ... %s\n", cfg.entries[indices[i]].name, statuses[i]);
		}
		fprintf(stderr, "\n%d/%zu repositories OK\n", (int) (checked - (size_t) errors), checked);
	}

	if (errors > 0)
		LOG_WARN("%d/%zu repos failed health check", errors, checked);
	else
		LOG_INFO("all %zu repos passed health check", checked);

	/* Cleanup */
	for (size_t i = 0; i < checked; i++)
		free(statuses[i]);
	free(statuses);
	free(indices);
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
	cmd_register_table_flag(cmd);
	(void) cmd;
}
