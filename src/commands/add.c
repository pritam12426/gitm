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
#include "config.h"
#include "git.h"
#include "log.h"

static const char *add_tags   = NULL;
static const char *add_groups = NULL;

int cmd_add(const ArgParseResult *result)
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
	char abs_path[512];
	if (realpath(repo_path, abs_path) == NULL) {
		LOG_ERROR("could not resolve path: %s", repo_path);
		return 1;
	}

	/* Derive name from path if not provided */
	char name_buf[256];
	if (!repo_name) {
		const char *base = strrchr(abs_path, '/');
		if (base && *(base + 1)) {
			snprintf(name_buf, sizeof(name_buf), "%s", base + 1);
			repo_name = name_buf;
		} else {
			repo_name = abs_path;
		}
	}

	char *config_path = config_default_path();
	if (!config_path) {
		LOG_ERROR("could not determine config path");
		return 1;
	}

	/* Ensure config dir exists */
	config_ensure_dir();

	GitConfig cfg = { 0 };
	config_load(config_path, &cfg);

	if (config_add(&cfg, abs_path, repo_name, add_tags, add_groups) != 0) {
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	if (config_save(config_path, &cfg) != 0) {
		LOG_ERROR("could not save config");
		config_free(&cfg);
		free(config_path);
		return 1;
	}

	fprintf(stderr, "Added %s (%s)", repo_name, abs_path);
	if (add_tags)
		fprintf(stderr, " tags=%s", add_tags);
	if (add_groups)
		fprintf(stderr, " groups=%s", add_groups);
	fprintf(stderr, "\n");

	LOG_INFO("registered %s at %s", repo_name, abs_path);

	config_free(&cfg);
	free(config_path);
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
