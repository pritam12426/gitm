/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * stash.c — `gitm stash` command
 *
 * Runs git stash across repos with dirty working trees.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	const char *name;
	const char *path;
	bool        was_dirty;
	bool        stashed;
	char        message[MAX_NAME_LEN];
} StashResult;

static void stash_collect(const RepoEntry *entry, void *out)
{
	StashResult *r = out;
	r->name        = entry->name;
	r->path        = entry->path;

	/* Check if working tree is dirty */
	ProcessResult pr = git_exec(entry->path, "status", "--porcelain", NULL);

	if (pr.exit_code != 0) {
		r->was_dirty = false;
		r->stashed   = false;
		snprintf(r->message, sizeof(r->message), "could not check status");
		process_result_free(&pr);
		return;
	}

	/* If no output, tree is clean */
	if (pr.stdout_len == 0) {
		r->was_dirty  = false;
		r->stashed    = false;
		r->message[0] = '\0';
		process_result_free(&pr);
		return;
	}

	r->was_dirty = true;
	process_result_free(&pr);

	/* Working tree is dirty — run git stash */
	ProcessResult stash_pr = git_exec(entry->path, "stash", NULL);

	if (stash_pr.exit_code == 0 && stash_pr.stdout_len > 0) {
		r->stashed = true;
		size_t len = stash_pr.stdout_len;
		if (len > 0 && stash_pr.stdout_buf[len - 1] == '\n')
			len--;
		if (len >= sizeof(r->message))
			len = sizeof(r->message) - 1;
		memcpy(r->message, stash_pr.stdout_buf, len);
		r->message[len] = '\0';
	} else {
		r->stashed = false;
		snprintf(r->message, sizeof(r->message), "stash failed");
	}

	process_result_free(&stash_pr);
}

static int cmd_stash(const ArgParseResult *result)
{
	(void) result;

	LOG_TRACE("cmd_stash");

	GitConfig cfg         = { 0 };
	char     *config_path = NULL;
	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	LOG_DEBUG("loaded %zu repos from config", cfg.count);

	CMD_RETURN_IF_EMPTY(cfg, config_path);

	size_t indices[MAX_REPOS];
	size_t filtered = cmd_filter_entries(&cfg, filter_tag, filter_group, indices, MAX_REPOS);

	if (filtered == 0) {
		cmd_cleanup(&cfg, config_path);
		return 0;
	}

	LOG_DEBUG("checking %zu repos for dirty working trees", filtered);

	StashResult results[MAX_REPOS] = { 0 };
	parallel_collect(&cfg, indices, filtered, stash_collect, sizeof(StashResult), results);

	/* Count results */
	size_t dirty_count = 0, stashed_count = 0, clean_count = 0;
	for (size_t i = 0; i < filtered; i++) {
		if (!results[i].was_dirty)
			clean_count++;
		else if (results[i].stashed)
			stashed_count++;
		else
			dirty_count++;
	}

	bool color = CMD_COLOR();

	if (g_table_mode) {
		LOG_DEBUG("table mode enabled");
		const char *headers[] = { "Repository", "Status", "Message" };
		Table      *t         = table_create(3, headers);
		table_set_color(t, color);

		for (size_t i = 0; i < filtered; i++) {
			const char *status;
			const char *msg;

			if (!results[i].was_dirty) {
				status = "clean";
				msg    = "-";
			} else if (results[i].stashed) {
				status = "stashed";
				msg    = results[i].message;
			} else {
				status = "failed";
				msg    = results[i].message;
			}

			const char *cells[] = { results[i].name, status, msg };
			table_add_row_raw(t, cells, 3);
		}

		table_print(t, stdout);
		table_free(t);
	} else {
		for (size_t i = 0; i < filtered; i++) {
			if (!results[i].was_dirty)
				continue;

			if (color) {
				if (results[i].stashed)
					fprintf(
					    stderr, "%s%s ... stashed%s\n", ANSI_FG_GREEN, results[i].name, ANSI_RESET);
				else
					fprintf(
					    stderr, "%s%s ... failed%s\n", ANSI_FG_RED, results[i].name, ANSI_RESET);
			} else {
				fprintf(stderr,
				        "%s ... %s\n",
				        results[i].name,
				        results[i].stashed ? "stashed" : "failed");
			}
		}
	}

	fprintf(stderr,
	        "\n%zu stashed, %zu failed, %zu clean (out of %zu)\n",
	        stashed_count,
	        dirty_count,
	        clean_count,
	        filtered);

	cmd_cleanup(&cfg, config_path);
	return dirty_count > 0 ? 1 : 0;
}

void cmd_register_stash(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "stash", "Stash dirty working trees", cmd_stash);
	const char *stash_aliases[] = { "sh" };
	argparse_command_set_aliases(cmd, stash_aliases, 1);
	cmd_register_filter_flags(cmd, &filter_tag, &filter_group);
	cmd_register_table_flag(cmd);
	(void) cmd;
}
