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

#ifndef _CMD_UTIL_H_
#define _CMD_UTIL_H_


#include <stddef.h>

#include "argparse.h"
#include "config.h"


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
                          const char *filter_tag,
                          const char *filter_group,
                          size_t *out_indices, size_t max);

/*
 * Register --tag and --group flags on a command.
 * Stores parsed values into *out_tag and *out_group.
 */
void cmd_register_filter_flags(ArgCommand *cmd,
                               const char **out_tag,
                               const char **out_group);


#ifdef __cplusplus
}
#endif


#endif /* _CMD_UTIL_H_ */
