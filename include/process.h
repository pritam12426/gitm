/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * process.h — Process execution wrapper
 *
 * Provides a clean interface for running external processes
 * (primarily git) via fork()/execvp() with stdout/stderr capture.
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_


#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	int    exit_code;
	char  *stdout_buf;
	size_t stdout_len;
	char  *stderr_buf;
	size_t stderr_len;
} ProcessResult;

/*
 * Execute a command in a child process.
 *
 *   cwd    — working directory (NULL inherits current)
 *   argv   — NULL-terminated argument array (argv[0] is the program)
 *
 * Returns a ProcessResult with exit_code, captured stdout, and captured stderr.
 * Caller must free with process_result_free().
 */
ProcessResult process_exec(const char *cwd, char *const argv[]);

/*
 * Free the buffers in a ProcessResult.
 */
void process_result_free(ProcessResult *r);


#ifdef __cplusplus
}
#endif


#endif /* _PROCESS_H_ */
