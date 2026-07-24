/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * git.c — Git execution wrapper
 *
 * Builds argv arrays and calls process_exec() to run git commands.
 * Provides convenience functions for common git queries.
 */

#include "git.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

/* Max args we accept */
#define GIT_MAX_ARGS 32

ProcessResult git_exec(const char *cwd, ...)
{
	LOG_TRACE("git_exec(cwd=%s)", cwd ? cwd : "(inherit)");
	va_list ap;
	va_start(ap, cwd);

	const char *args[GIT_MAX_ARGS];
	int         argc = 0;

	args[argc++] = "git";

	const char *arg;
	while (argc < GIT_MAX_ARGS - 1 && (arg = va_arg(ap, const char *)) != NULL) {
		args[argc++] = arg;
	}
	va_end(ap);

	args[argc] = NULL;

	/* Cast away const for process_exec's char *const argv[] */
	char *mutable_argv[GIT_MAX_ARGS];
	for (int i = 0; i <= argc; i++) {
		mutable_argv[i] = (char *) args[i];
	}

	return process_exec(cwd, mutable_argv);
}

ProcessResult git_exec_quiet(const char *cwd, ...)
{
	LOG_TRACE("git_exec_quiet(cwd=%s)", cwd ? cwd : "(inherit)");
	va_list ap;
	va_start(ap, cwd);

	const char *args[GIT_MAX_ARGS];
	int         argc = 0;

	args[argc++] = "git";

	const char *arg;
	while (argc < GIT_MAX_ARGS - 1 && (arg = va_arg(ap, const char *)) != NULL) {
		args[argc++] = arg;
	}
	va_end(ap);

	args[argc] = NULL;

	char *mutable_argv[GIT_MAX_ARGS];
	for (int i = 0; i <= argc; i++) {
		mutable_argv[i] = (char *) args[i];
	}

	return process_exec(cwd, mutable_argv);
}

bool git_is_repo(const char *path)
{
	ProcessResult r       = git_exec(path, "rev-parse", "--is-inside-work-tree", NULL);
	bool          is_repo = (r.exit_code == 0 && r.stdout_len > 0);
	LOG_TRACE("git_is_repo(%s) = %s", path, is_repo ? "true" : "false");
	process_result_free(&r);
	return is_repo;
}

char *git_toplevel(const char *path)
{
	LOG_TRACE("git_toplevel(%s)", path);
	ProcessResult r = git_exec(path, "rev-parse", "--show-toplevel", NULL);
	if (r.exit_code != 0 || r.stdout_len == 0) {
		process_result_free(&r);
		return NULL;
	}

	/* Strip trailing newline */
	char  *result = strdup(r.stdout_buf);
	size_t len    = strlen(result);
	if (len > 0 && result[len - 1] == '\n')
		result[len - 1] = '\0';

	process_result_free(&r);
	return result;
}

char *git_current_branch(const char *path)
{
	LOG_TRACE("git_current_branch(%s)", path);
	ProcessResult r = git_exec(path, "rev-parse", "--abbrev-ref", "HEAD", NULL);
	if (r.exit_code != 0 || r.stdout_len == 0) {
		process_result_free(&r);
		return NULL;
	}

	char  *result = strdup(r.stdout_buf);
	size_t len    = strlen(result);
	if (len > 0 && result[len - 1] == '\n')
		result[len - 1] = '\0';

	process_result_free(&r);
	return result;
}
