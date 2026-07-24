/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * process.c — Process execution wrapper
 *
 * Uses fork()/execvp() to run external processes.
 * Captures stdout and stderr via pipes.
 */

#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Initial buffer size for capturing output */
#define INITIAL_BUF_SIZE 4096

static int buf_append(char **buf, size_t *len, size_t *cap, const char *data, size_t data_len)
{
	if (*len + data_len + 1 > *cap) {
		size_t new_cap = (*cap == 0) ? INITIAL_BUF_SIZE : *cap * 2;
		while (new_cap < *len + data_len + 1)
			new_cap *= 2;
		char *tmp = realloc(*buf, new_cap);
		if (!tmp)
			return -1;
		*buf = tmp;
		*cap = new_cap;
	}
	memcpy(*buf + *len, data, data_len);
	*len         += data_len;
	(*buf)[*len]  = '\0';
	return 0;
}

ProcessResult process_exec(const char *cwd, char *const argv[])
{
	ProcessResult result = { 0 };

	int stdout_pipe[2] = { -1, -1 };
	int stderr_pipe[2] = { -1, -1 };

	if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
		result.exit_code = -1;
		return result;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		close(stderr_pipe[0]);
		close(stderr_pipe[1]);
		result.exit_code = -1;
		return result;
	}

	if (pid == 0) {
		/* Child process */
		close(stdout_pipe[0]);
		close(stderr_pipe[0]);

		dup2(stdout_pipe[1], STDOUT_FILENO);
		dup2(stderr_pipe[1], STDERR_FILENO);

		close(stdout_pipe[1]);
		close(stderr_pipe[1]);

		if (cwd && chdir(cwd) != 0) {
			_exit(127);
		}

		execvp(argv[0], (char *const *) argv);
		_exit(127);
	}

	/* Parent process */
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	/* Read stdout */
	char   *std_out = NULL;
	size_t  out_len = 0, out_cap = 0;
	char    tmp[4096];
	ssize_t n;

	while ((n = read(stdout_pipe[0], tmp, sizeof(tmp))) > 0) {
		if (buf_append(&std_out, &out_len, &out_cap, tmp, (size_t) n) != 0)
			break;
	}
	close(stdout_pipe[0]);

	/* Read stderr */
	char  *std_err = NULL;
	size_t err_len = 0, err_cap = 0;

	while ((n = read(stderr_pipe[0], tmp, sizeof(tmp))) > 0) {
		if (buf_append(&std_err, &err_len, &err_cap, tmp, (size_t) n) != 0)
			break;
	}
	close(stderr_pipe[0]);

	/* Wait for child */
	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		result.exit_code = WEXITSTATUS(status);
	else
		result.exit_code = -1;

	result.stdout_buf = std_out;
	result.stdout_len = out_len;
	result.stderr_buf = std_err;
	result.stderr_len = err_len;

	return result;
}

void process_result_free(ProcessResult *r)
{
	if (!r)
		return;
	free(r->stdout_buf);
	free(r->stderr_buf);
	r->stdout_buf = NULL;
	r->stderr_buf = NULL;
	r->stdout_len = 0;
	r->stderr_len = 0;
}
