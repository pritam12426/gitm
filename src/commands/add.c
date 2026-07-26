/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * add.c — `gitm add` command
 *
 * Registers a Git repository in the config.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"

static const char *add_tags   = NULL;
static const char *add_groups = NULL;

static int cmd_add(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm add <path> [name] [--tag TAGS] [--group GROUPS]\n");
		return 1;
	}

	LOG_TRACE("cmd_add");
	const char *repo_path = result->positionals[0];
	const char *repo_name = result->positional_count > 1 ? result->positionals[1] : NULL;

	LOG_DEBUG("adding repo: path=%s name=%s", repo_path, repo_name ? repo_name : "(auto)");

	/* Validate: is it a git repo? */
	if (!git_is_repo(repo_path)) {
		LOG_ERROR("not a git repository: %s", repo_path);
		return 1;
	}

	/* Resolve to absolute path */
	char abs_path[PATH_MAX];
	if (realpath(repo_path, abs_path) == NULL) {
		LOG_ERROR("could not resolve path: %s", repo_path);
		return 1;
	}

	/* Derive name from path if not provided */
	char name_buf[MAX_NAME_LEN];
	if (!repo_name) {
		const char *base = strrchr(abs_path, '/');
		if (base && *(base + 1)) {
			snprintf(name_buf, sizeof(name_buf), "%s", base + 1);
			repo_name = name_buf;
		} else {
			repo_name = abs_path;
		}
	}

	if (cmd_ensure_config_dir() != 0)
		return 1;

	GitConfig cfg = { 0 };
	char      *loaded_path = NULL;
	if (cmd_load_config(&cfg, &loaded_path) != 0)
		return 1;

	if (config_add(&cfg, abs_path, repo_name, add_tags, add_groups) != 0) {
		cmd_cleanup(&cfg, loaded_path);
		return 1;
	}

	if (cmd_save_config(&cfg, loaded_path) != 0)
		return 1;

	fprintf(stderr, "Added %s (%s)", repo_name, abs_path);
	if (add_tags)
		fprintf(stderr, " tags=%s", add_tags);
	if (add_groups)
		fprintf(stderr, " groups=%s", add_groups);
	fprintf(stderr, "\n");

	LOG_INFO("registered %s at %s", repo_name, abs_path);

	cmd_cleanup(&cfg, loaded_path);
	return 0;
}

void cmd_register_add(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser, "add", "Register a Git repository", cmd_add);
	argparse_add_positional(cmd, "path");
	argparse_add_positional(cmd, "[name]");
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAGS",
	                    "Comma-separated tags (e.g., work,c)", &add_tags);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUPS",
	                    "Comma-separated groups (e.g., projects)", &add_groups);
}
