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
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "share.h"

static int buf_append(char **buf, size_t *len, size_t *cap, const char *data, size_t data_len)
{
	/* Guard against size_t overflow */
	if (*len + data_len < *len || *len + data_len + 1 < *len + data_len)
		return -1;

	if (*len + data_len + 1 > *cap) {
		size_t need = *len + data_len + 1;
		size_t new_cap = (*cap == 0) ? PROCESS_BUF_SIZE : *cap * 2;

		/* Guard against overflow in doubling */
		if (new_cap < *cap)
			return -1;

		while (new_cap < need) {
			if (new_cap > (SIZE_MAX / 2))
				return -1;
			new_cap *= 2;
		}
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

	if (pipe(stdout_pipe) != 0) {
		LOG_ERROR("pipe creation failed: %s", strerror(errno));
		result.exit_code = -1;
		return result;
	}
	if (pipe(stderr_pipe) != 0) {
		LOG_ERROR("pipe creation failed: %s", strerror(errno));
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		result.exit_code = -1;
		return result;
	}

	LOG_TRACE("exec: %s in %s", argv[0], cwd ? cwd : "(inherit)");

	/* Ignore SIGPIPE so we don't die if the child exits early */
	struct sigaction sa   = { .sa_handler = SIG_IGN };
	struct sigaction old_sa;
	sigaction(SIGPIPE, &sa, &old_sa);

	pid_t pid = fork();
	if (pid < 0) {
		LOG_ERROR("fork failed: %s", strerror(errno));
		sigaction(SIGPIPE, &old_sa, NULL);
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

		if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
		    dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
			_exit(EXIT_CMD_NOT_FOUND);
		}

		close(stdout_pipe[1]);
		close(stderr_pipe[1]);

		if (cwd && chdir(cwd) != 0) {
			_exit(EXIT_CMD_NOT_FOUND);
		}

		execvp(argv[0], (char *const *) argv);
		_exit(EXIT_CMD_NOT_FOUND);
	}

	/* Restore parent's SIGPIPE disposition */
	sigaction(SIGPIPE, &old_sa, NULL);

	/* Parent process */
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	/*
	 * Read stdout and stderr concurrently using poll() to avoid deadlock.
	 * If the child writes >PIPE_BUF to stderr before stdout, a sequential
	 * reader would block on stdout while the child blocks on stderr.
	 */
	char   *std_out = NULL;
	size_t  out_len = 0, out_cap = 0;
	char   *std_err = NULL;
	size_t  err_len = 0, err_cap = 0;
	char    tmp[PROCESS_BUF_SIZE];
	ssize_t n;

	struct pollfd fds[2] = {
		{ .fd = stdout_pipe[0], .events = POLLIN },
		{ .fd = stderr_pipe[0], .events = POLLIN },
	};
	int open_fds = 2;

	while (open_fds > 0) {
		int ret = poll(fds, 2, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		if (fds[0].revents & POLLIN) {
			n = read(stdout_pipe[0], tmp, sizeof(tmp));
			if (n > 0) {
				if (buf_append(&std_out, &out_len, &out_cap, tmp, (size_t) n) != 0) {
					LOG_ERROR("stdout capture: allocation failed, output truncated");
				}
			} else if (n == 0 || (n < 0 && errno != EINTR)) {
				fds[0].fd = -1;
				open_fds--;
			}
		} else if (fds[0].revents & (POLLHUP | POLLERR)) {
			fds[0].fd = -1;
			open_fds--;
		}

		if (fds[1].revents & POLLIN) {
			n = read(stderr_pipe[0], tmp, sizeof(tmp));
			if (n > 0) {
				if (buf_append(&std_err, &err_len, &err_cap, tmp, (size_t) n) != 0) {
					LOG_ERROR("stderr capture: allocation failed, output truncated");
				}
			} else if (n == 0 || (n < 0 && errno != EINTR)) {
				fds[1].fd = -1;
				open_fds--;
			}
		} else if (fds[1].revents & (POLLHUP | POLLERR)) {
			fds[1].fd = -1;
			open_fds--;
		}
	}

	close(stdout_pipe[0]);
	close(stderr_pipe[0]);

	/* Wait for child */
	int status = -1;
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			break;
	}

	if (status >= 0 && WIFEXITED(status))
		result.exit_code = WEXITSTATUS(status);
	else
		result.exit_code = -1;

	result.stdout_buf = std_out;
	result.stdout_len = out_len;
	result.stderr_buf = std_err;
	result.stderr_len = err_len;

	return result;
}

ProcessResult process_exec_colored(const char *cwd, char *const argv[])
{
	ProcessResult result = { 0 };

	int stdout_pipe[2] = { -1, -1 };
	int stderr_pipe[2] = { -1, -1 };

	if (pipe(stdout_pipe) != 0) {
		LOG_ERROR("pipe creation failed: %s", strerror(errno));
		result.exit_code = -1;
		return result;
	}
	if (pipe(stderr_pipe) != 0) {
		LOG_ERROR("pipe creation failed: %s", strerror(errno));
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		result.exit_code = -1;
		return result;
	}

	LOG_TRACE("exec(color): %s in %s", argv[0], cwd ? cwd : "(inherit)");

	/* Ignore SIGPIPE so we don't die if the child exits early */
	struct sigaction sa   = { .sa_handler = SIG_IGN };
	struct sigaction old_sa;
	sigaction(SIGPIPE, &sa, &old_sa);

	pid_t pid = fork();
	if (pid < 0) {
		LOG_ERROR("fork failed: %s", strerror(errno));
		sigaction(SIGPIPE, &old_sa, NULL);
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

		if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
		    dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
			_exit(EXIT_CMD_NOT_FOUND);
		}

		close(stdout_pipe[1]);
		close(stderr_pipe[1]);

		/* Force ANSI colour output */
		if (!getenv("NO_COLOR")) {
			setenv("FORCE_COLOR", "1", 1);
			setenv("CLICOLOR_FORCE", "1", 1);
		}

		if (cwd && chdir(cwd) != 0) {
			_exit(EXIT_CMD_NOT_FOUND);
		}

		execvp(argv[0], (char *const *) argv);
		_exit(EXIT_CMD_NOT_FOUND);
	}

	/* Restore parent's SIGPIPE disposition */
	sigaction(SIGPIPE, &old_sa, NULL);

	/* Parent process */
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	/* Read stdout and stderr concurrently using poll() */
	char   *std_out = NULL;
	size_t  out_len = 0, out_cap = 0;
	char   *std_err = NULL;
	size_t  err_len = 0, err_cap = 0;
	char    tmp[PROCESS_BUF_SIZE];
	ssize_t n;

	struct pollfd fds[2] = {
		{ .fd = stdout_pipe[0], .events = POLLIN },
		{ .fd = stderr_pipe[0], .events = POLLIN },
	};
	int open_fds = 2;

	while (open_fds > 0) {
		int ret = poll(fds, 2, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		if (fds[0].revents & POLLIN) {
			n = read(stdout_pipe[0], tmp, sizeof(tmp));
			if (n > 0) {
				if (buf_append(&std_out, &out_len, &out_cap, tmp, (size_t) n) != 0) {
					LOG_ERROR("stdout capture (color): allocation failed, output truncated");
				}
			} else if (n == 0 || (n < 0 && errno != EINTR)) {
				fds[0].fd = -1;
				open_fds--;
			}
		} else if (fds[0].revents & (POLLHUP | POLLERR)) {
			fds[0].fd = -1;
			open_fds--;
		}

		if (fds[1].revents & POLLIN) {
			n = read(stderr_pipe[0], tmp, sizeof(tmp));
			if (n > 0) {
				if (buf_append(&std_err, &err_len, &err_cap, tmp, (size_t) n) != 0) {
					LOG_ERROR("stderr capture (color): allocation failed, output truncated");
				}
			} else if (n == 0 || (n < 0 && errno != EINTR)) {
				fds[1].fd = -1;
				open_fds--;
			}
		} else if (fds[1].revents & (POLLHUP | POLLERR)) {
			fds[1].fd = -1;
			open_fds--;
		}
	}

	close(stdout_pipe[0]);
	close(stderr_pipe[0]);

	/* Wait for child */
	int status = -1;
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			break;
	}

	if (status >= 0 && WIFEXITED(status))
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

void process_steal_stdout(CmdGitResult *dst, ProcessResult *src)
{
	dst->exit_code  = src->exit_code;
	dst->stdout_buf = src->stdout_buf;
	dst->stdout_len = src->stdout_len;
	src->stdout_buf = NULL;
	process_result_free(src);
}
