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

#ifndef _GIT__H_
#define _GIT__H_


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
 * Run a git command with FORCE_COLOR=1.
 *
 * Same as git_exec() but forces ANSI colour output from git.
 * Use for commands whose output is displayed directly to the user.
 * Caller must free the result with process_result_free().
 */
ProcessResult git_exec_color(const char *cwd, ...);

/*
 * Run a git command, choosing color or plain based on use_color.
 * Equivalent to: use_color ? git_exec_color(...) : git_exec(...)
 */
ProcessResult git_exec_smart(const char *cwd, int use_color, ...);

/*
 * Get the last commit date of a repo (format: "YYYY-MM-DD HH:MM:SS +ZZZZ").
 * Returns NULL on error. Caller must free.
 */
char *git_last_commit_date(const char *path);

/*
 * Copy the last commit date into a caller-provided buffer (no heap alloc).
 * Returns 0 on success, -1 on error. buf is always null-terminated.
 */
int git_last_commit_date_into(const char *path, char *buf, size_t buflen);

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


#endif  // _GIT__H_
