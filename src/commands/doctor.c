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
#include "parallel.h"
#include "table.h"

static const char *filter_tag   = NULL;
static const char *filter_group = NULL;

typedef struct {
	const char *status;
	bool        is_ok;
} DoctorResult;

static void doctor_collect(const RepoEntry *entry, void *out)
{
	DoctorResult *r = out;

	struct stat st;
	if (stat(entry->path, &st) != 0) {
		r->status = "MISSING";
		r->is_ok  = false;
	} else if (!S_ISDIR(st.st_mode)) {
		r->status = "NOT A DIRECTORY";
		r->is_ok  = false;
	} else if (!git_is_repo(entry->path)) {
		r->status = "NOT A GIT REPO";
		r->is_ok  = false;
	} else {
		r->status = "ok";
		r->is_ok  = true;
	}
}

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

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group,
	                                     indices, cfg.count);

	LOG_DEBUG("running health check on %zu repos", filtered);

	/* Phase 1: parallel collection */
	DoctorResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered,
	                 doctor_collect, sizeof(DoctorResult), results);

	/* Phase 2: display sequentially */
	int errors = 0;
	for (size_t i = 0; i < filtered; i++) {
		if (!results[i].is_ok)
			errors++;
	}

	if (g_table_mode) {
		const char *headers[] = { "Name", "Status" };
		Table *t = table_create(2, headers);
		bool color = CMD_COLOR();
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++) {
			const char *name   = cfg.entries[indices[i]].name;
			const char *status = results[i].status;

			if (color) {
				char colored[128];
				ansi_colorize(colored, sizeof(colored), status,
				              results[i].is_ok ? ANSI_FG_GREEN : ANSI_FG_RED);
				const char *cells[] = { name, colored };
				table_add_row_raw(t, cells, 2);
			} else {
				table_add_row(t, name, status);
			}
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			fprintf(stderr, "%22s : %s\n",
			        cfg.entries[indices[i]].name, results[i].status);
		}
		fprintf(stderr, "\n%d/%zu repositories OK\n",
		        (int) (filtered - (size_t) errors), filtered);
	}

	if (errors > 0)
		LOG_WARN("%d/%zu repos failed health check", errors, filtered);
	else
		LOG_INFO("all %zu repos passed health check", filtered);

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
