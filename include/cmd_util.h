/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd_util.h — Shared command utilities
 *
 * Provides common boilerplate for commands that iterate registered repos
 * with --tag/--group filtering. Used by list, status, branch, last,
 * remote, list-tag, recent, and doctor.
 */

#ifndef _CMD_UTIL__H_
#define _CMD_UTIL__H_


#include <stddef.h>
#include <unistd.h>

#include "argparse.h"
#include "cmd.h"
#include "config.h"
#include "log.h"

/* Determine colour based on the current output destination.
 * In table mode output goes to stdout, otherwise to stderr. */
#define CMD_COLOR() (g_table_mode ? isatty(fileno(stdout)) : log_use_color())

/*
 * Early return if config has no repos registered.
 * Must be called after cmd_load_config and before any repo iteration.
 */
#define CMD_RETURN_IF_EMPTY(cfg, path)                         \
	do {                                                   \
		if ((cfg).count == 0) {                       \
			fprintf(stderr, MSG_NO_REPOS);       \
			cmd_cleanup(&(cfg), (path));          \
			return 0;                            \
		}                                              \
	} while (0)


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Load the config from the default path.
 * Returns 0 on success (cfg may be empty), -1 on error.
 * On success, *path_out is set to the config path (caller must free).
 * On error, *path_out is set to NULL.
 */
int cmd_load_config(GitConfig *cfg, char **path_out);

/*
 * Filter config entries by tag and/or group.
 * Writes matching indices into out_indices (up to max).
 * Returns the number of matching entries.
 */
size_t cmd_filter_entries(const GitConfig *cfg,
                          const char      *filter_tag,
                          const char      *filter_group,
                          size_t          *out_indices,
                          size_t           max);

/*
 * Register --tag and --group flags on a command.
 * Stores parsed values into *out_tag and *out_group.
 */
void cmd_register_filter_flags(ArgCommand *cmd, const char **out_tag, const char **out_group);

/*
 * Print "name : path" aligned to NAME_COL_WIDTH.
 */
void cmd_print_name_path(FILE *stream, const char *name, const char *path);

/*
 * Display a CmdGitResult in plain (non-table) mode.
 * If empty_msg is NULL and output is empty, returns silently.
 * If empty_msg is provided, shows an "empty" indicator.
 */
void cmd_display_plain_result(int         exit_code,
                              const char *stdout_buf,
                              size_t      stdout_len,
                              const char *name,
                              const char *empty_msg,
                              bool        color);

/*
 * Resolve the user's editor from $EDITOR or $VISUAL.
 * Returns the editor path, or NULL if not set.
 */
const char *cmd_resolve_editor(void);

/*
 * Get the last commit date string and days-ago for a repo.
 * Returns 0 on success, -1 if no commits found.
 * date_out is filled with the raw date string; days_out has the age in days.
 */
int cmd_repo_commit_age(const char *path, char *date_out, size_t date_len, long *days_out);

/*
 * Find a repo by name, or print an error and clean up.
 * Returns the entry on success, NULL on failure (caller should return 1).
 */
RepoEntry *cmd_find_repo_or_fail(GitConfig *cfg, char *config_path, const char *name);

/*
 * Copy the next line from a buffer into buf, advancing *pp past the newline.
 * Returns the length of the copied line (excluding null terminator), or 0 if at end.
 */
size_t copy_next_line(const char **pp, char *buf, size_t buflen);

/*
 * Repo health status codes.
 */
typedef enum {
	REPO_HEALTH_OK = 0,
	REPO_HEALTH_MISSING,
	REPO_HEALTH_NOT_DIR,
	REPO_HEALTH_NOT_GIT,
} RepoHealth;

/*
 * Check the health of a repo (stat + is_dir + is_git_repo).
 * Returns the health status code.
 */
RepoHealth repo_check_health(const char *path);


#ifdef __cplusplus
}
#endif


#endif  // _CMD_UTIL__H_
