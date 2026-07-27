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
#include "share.h"

static ProcessResult git_execv(const char *cwd, bool use_color, va_list ap)
{
	const char *args[GIT_MAX_ARGS];
	int         argc = 0;

	args[argc++] = GIT_BINARY;

	if (use_color) {
		args[argc++] = "-c";
		args[argc++] = "color.ui=always";
	}

	const char *arg;
	while (argc < GIT_MAX_ARGS - 1 && (arg = va_arg(ap, const char *)) != NULL) {
		args[argc++] = arg;
	}

	args[argc] = NULL;

	/* Cast away const for process_exec's char *const argv[] */
	char *mutable_argv[GIT_MAX_ARGS];
	for (int i = 0; i <= argc; i++) {
		mutable_argv[i] = (char *) args[i];
	}

	if (use_color)
		return process_exec_colored(cwd, mutable_argv);
	else
		return process_exec(cwd, mutable_argv);
}

ProcessResult git_exec(const char *cwd, ...)
{
	LOG_TRACE("git_exec(cwd=%s)", cwd ? cwd : "(inherit)");
	va_list ap;
	va_start(ap, cwd);
	ProcessResult r = git_execv(cwd, false, ap);
	va_end(ap);
	return r;
}

ProcessResult git_exec_color(const char *cwd, ...)
{
	LOG_TRACE("git_exec_color(cwd=%s)", cwd ? cwd : "(inherit)");
	va_list ap;
	va_start(ap, cwd);
	ProcessResult r = git_execv(cwd, true, ap);
	va_end(ap);
	return r;
}

ProcessResult git_exec_smart(const char *cwd, int use_color, ...)
{
	LOG_TRACE("git_exec_smart(cwd=%s, color=%d)", cwd ? cwd : "(inherit)", use_color);
	va_list ap;
	va_start(ap, use_color);
	ProcessResult r = git_execv(cwd, (bool) use_color, ap);
	va_end(ap);
	return r;
}

char *git_last_commit_date(const char *path)
{
	LOG_TRACE("git_last_commit_date(%s)", path);
	ProcessResult r = git_exec(path, "log", "-1", "--format=%ci", "HEAD", NULL);
	if (r.exit_code != 0 || r.stdout_len == 0) {
		process_result_free(&r);
		return NULL;
	}
	char *result = strdup_strip_newline(r.stdout_buf);
	process_result_free(&r);
	return result;
}

int git_last_commit_date_into(const char *path, char *buf, size_t buflen)
{
	LOG_TRACE("git_last_commit_date_into(%s)", path);
	if (!buf || buflen == 0)
		return -1;

	ProcessResult r = git_exec(path, "log", "-1", "--format=%ci", "HEAD", NULL);
	if (r.exit_code != 0 || r.stdout_len == 0) {
		process_result_free(&r);
		buf[0] = '\0';
		return -1;
	}

	size_t len = r.stdout_len;
	while (len > 0 && (r.stdout_buf[len - 1] == '\n' || r.stdout_buf[len - 1] == '\r'))
		len--;
	if (len >= buflen)
		len = buflen - 1;
	memcpy(buf, r.stdout_buf, len);
	buf[len] = '\0';
	process_result_free(&r);
	return 0;
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
	char *result = strdup_strip_newline(r.stdout_buf);
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
	char *result = strdup_strip_newline(r.stdout_buf);
	process_result_free(&r);
	return result;
}
