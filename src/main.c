/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * main.c — gitm entry point
 *
 * Parses CLI arguments using the custom argparse library,
 * registers all commands, and dispatches to the matched command.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argparse.h"
#include "cmd.h"
#include "config.h"
#include "log.h"
#include "project_config.h"

/* Global options (stored on root command) */
static bool        g_edit_entry    = false;
static const char *g_log_level_str = NULL;
static const char *g_log_file      = NULL;

static Log_level_t parse_log_level(const char *str)
{
	if (!str)                              return LOG_LEVEL_INFO;
	if (strcmp(str, "error") == 0) return LOG_LEVEL_ERROR;
	if (strcmp(str, "warn") == 0)  return LOG_LEVEL_WARN;
	if (strcmp(str, "info") == 0)  return LOG_LEVEL_INFO;
	if (strcmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
	return                                        LOG_LEVEL_INFO;
}

int main(int argc, char *argv[])
{
	/* Create parser */
	ArgParserConfig config = {
		.prog_name   = MAIN_BINARY,
		.version     = PROJECT_VERSION,
		.description = PROJECT_DESCRIPTION,
		.bug_url     = PROJECT_HOMEPAGE_URL "/issues",
		.author      = AUTH_MESSAGE,
	};

	ArgParser *parser = argparse_new(&config);
	if (!parser) {
		fprintf(stderr, "%s: failed to initialize parser\n", MAIN_BINARY);
		return 1;
	}

	/* Register global options */
	ArgCommand *root = &parser->root;

	argparse_add_option(root,
	                    "log-level",
	                    'L',
	                    ARG_TYPE_STRING,
	                    "LEVEL",
	                    "Set log verbosity: error, warn, info, debug",
	                    &g_log_level_str);

	argparse_add_option(root,
	                    "log-file",
	                    'F',
	                    ARG_TYPE_STRING,
	                    "FILE",
	                    "Set logging file",
	                    &g_log_file);

	argparse_add_option(root,
	                    "edit-entry",
	                    'E',
	                    ARG_TYPE_NONE,
	                    NULL,
	                    "Open registered_repos.txt in $EDITOR",
	                    &g_edit_entry);

	/* Register all subcommands */
	cmd_register_all(parser);

	/* Initialize logging early so commands can log during parse.
	 * Re-init after parsing if user specified different options. */
	log_init(NULL, LOG_LEVEL_INFO);

	/* Parse */
	int rc = argparse_parse(parser, argc, argv);

	/* Re-init logging with user-specified options */
	log_init(g_log_file, parse_log_level(g_log_level_str));

	/* Handle --edit-entry before dispatching to commands */
	if (g_edit_entry) {
		char *path = config_default_path();
		if (!path) {
			LOG_ERROR("could not determine config path");
			argparse_free(parser);
			return 1;
		}

		config_ensure_dir();

		const char *editor = getenv("EDITOR");
		if (!editor) editor = getenv("VISUAL");
		if (!editor) {
			LOG_ERROR("no $EDITOR or $VISUAL set");
			free(path);
			argparse_free(parser);
			return 1;
		}

		/* Fork directly — process_exec captures stdout/stderr via pipes,
		 * which breaks interactive editors. We need stdin/stdout/stderr
		 * connected to the terminal. */
		pid_t pid = fork();
		if (pid < 0) {
			LOG_ERROR("fork failed");
			free(path);
			argparse_free(parser);
			return 1;
		}

		if (pid == 0) {
			/* Child: exec editor with config path */
			execlp(editor, editor, path, (char *) NULL);
			_exit(127);
		}

		/* Parent: wait for editor to exit */
		int status;
		waitpid(pid, &status, 0);

		int editor_rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		free(path);
		argparse_free(parser);
		return editor_rc;
	}

	/* If no command matched and no error, show help */
	if (rc == 0 && parser->matched_command == &parser->root) {
		argparse_help(parser, NULL);
	}

	argparse_free(parser);
	return rc;
}
