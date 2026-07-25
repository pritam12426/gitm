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

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ansi_color.h"
#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

static int cmd_doctor(const ArgParseResult *result)
{
	(void) result;

	GitConfig cfg = { 0 };
	char      *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	/* Stack arrays — no heap */
	size_t      indices[MAX_REPOS];
	const char *statuses[MAX_REPOS];
	size_t      checked = 0;
	int         errors  = 0;

	LOG_DEBUG("running health check on %zu repos", cfg.count);

	for (size_t i = 0; i < cfg.count; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg.entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg.entries[i], filter_group))
			continue;

		LOG_TRACE("checking %s", cfg.entries[i].name);

		struct stat st;
		if (stat(cfg.entries[i].path, &st) != 0) {
			statuses[checked] = "MISSING";
			errors++;
		} else if (!S_ISDIR(st.st_mode)) {
			statuses[checked] = "NOT A DIRECTORY";
			errors++;
		} else if (!git_is_repo(cfg.entries[i].path)) {
			statuses[checked] = "NOT A GIT REPO";
			errors++;
		} else {
			statuses[checked] = "ok";
		}

		indices[checked] = i;
		checked++;
	}

	if (g_table_mode) {
		const char *headers[] = { "Name", "Status" };
		Table *t = table_create(2, headers);
		bool color = CMD_COLOR();
		table_set_color(t, color);

		for (size_t i = 0; i < checked; i++) {
			const char *name = cfg.entries[indices[i]].name;
			const char *status = statuses[i];
			bool is_ok = (strcmp(status, "ok") == 0);

			if (color) {
				char colored[128];
				ansi_colorize(colored, sizeof(colored), status,
				              is_ok ? ANSI_FG_GREEN : ANSI_FG_RED);
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

	cmd_cleanup(&cfg, config_path);
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
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
