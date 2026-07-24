/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * git.h — Git execution wrapper
 *
 * High-level interface for running git commands.
 * Uses process_exec() internally.
 */

#ifndef _GIT_H_
#define _GIT_H_


#include <stdbool.h>

#include "process.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Run a git command in the given directory.
 *
 *   cwd  — working directory (must be inside a git repo)
 *   ...  — NULL-terminated list of arguments (e.g. "git", "status", NULL)
 *
 * Caller must free the result with process_result_free().
 */
ProcessResult git_exec(const char *cwd, ...);

/*
 * Convenience: run a git command and return true if exit code is 0.
 * Output is still in result — caller must free.
 */
ProcessResult git_exec_quiet(const char *cwd, ...);

/*
 * Check if a directory is inside a git repository.
 */
bool git_is_repo(const char *path);

/*
 * Get the top-level directory of a git repo.
 * Returns NULL if not a git repo. Caller must free.
 */
char *git_toplevel(const char *path);

/*
 * Get the current branch name.
 * Returns NULL on error. Caller must free.
 */
char *git_current_branch(const char *path);


#ifdef __cplusplus
}
#endif


#endif /* _GIT_H_ */
