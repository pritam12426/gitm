/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * clone.c — `gitm clone` command
 *
 * Clones a repository and registers it.
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

static int cmd_clone(const ArgParseResult *result)
{
	if (result->positional_count < 1) {
		fprintf(stderr, "Usage: gitm clone <url> [name]\n");
		return 1;
	}

	LOG_TRACE("cmd_clone");
	const char *url  = result->positionals[0];
	const char *name = result->positional_count > 1 ? result->positionals[1] : NULL;
	LOG_DEBUG("cloning %s", url);

	/* Derive name from URL if not provided */
	char name_buf[MAX_NAME_LEN];
	if (!name) {
		const char *base = strrchr(url, '/');
		if (base && *(base + 1)) {
			base++;
			snprintf(name_buf, sizeof(name_buf), "%s", base);
			/* Strip .git suffix */
			size_t len = strlen(name_buf);
			if (len > 4 && strcmp(name_buf + len - 4, ".git") == 0)
				name_buf[len - 4] = '\0';
			name = name_buf;
		} else {
			name = url;
		}
	}

	/* Build destination: current dir / name */
	char        dest[PATH_MAX];
	const char *pwd = getenv("PWD");
	if (pwd)
		snprintf(dest, sizeof(dest), "%s/%s", pwd, name);
	else
		snprintf(dest, sizeof(dest), "%s", name);
	dest[sizeof(dest) - 1] = '\0';

	fprintf(stderr, "Cloning %s into %s...\n", url, dest);

	ProcessResult r = process_exec(NULL,
	                               (char *const *) (const char *[]) {
	                                   GIT_BINARY, "clone", url, dest, NULL });

	if (r.exit_code != 0) {
		if (r.stderr_len > 0)
			fwrite(r.stderr_buf, 1, r.stderr_len, stderr);
		process_result_free(&r);
		return 1;
	}

	process_result_free(&r);

	/* Register the cloned repo */
	if (cmd_ensure_config_dir() != 0)
		return 1;

	GitConfig cfg = { 0 };
	char      *loaded_path = NULL;
	if (cmd_load_config(&cfg, &loaded_path) != 0)
		return 1;

	if (config_add(&cfg, dest, name, NULL, NULL) != 0) {
		LOG_WARN("cloned successfully but failed to register");
		cmd_cleanup(&cfg, loaded_path);
		return 0;
	}

	if (cmd_save_config(&cfg, loaded_path) != 0) {
		LOG_WARN("cloned successfully but failed to save config");
		/* cmd_save_config already called cmd_cleanup on failure */
		return 1;
	}

	fprintf(stderr, "Registered as '%s'\n", name);
	LOG_INFO("cloned and registered as %s", name);

	cmd_cleanup(&cfg, loaded_path);
	return 0;
}

void cmd_register_clone(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "clone",
	                                       "Clone a repository and register it",
	                                       cmd_clone);
	argparse_add_positional(cmd, "url");
	argparse_add_positional(cmd, "[name]");
}
