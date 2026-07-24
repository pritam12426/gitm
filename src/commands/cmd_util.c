/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd_util.c — Shared command utilities implementation
 */

#include "cmd_util.h"

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "log.h"


int cmd_load_config(GitConfig *cfg, char **path_out)
{
	*path_out = config_default_path();
	if (!*path_out) {
		LOG_ERROR("could not determine config path");
		return -1;
	}

	if (config_load(*path_out, cfg) != 0) {
		LOG_ERROR("could not load config");
		free(*path_out);
		*path_out = NULL;
		return -1;
	}

	LOG_DEBUG("loaded config with %zu entries", cfg->count);
	return 0;
}

size_t cmd_filter_entries(const GitConfig *cfg,
                           const char *filter_tag,
                           const char *filter_group,
                           size_t *out_indices, size_t max)
{
	size_t count = 0;

	for (size_t i = 0; i < cfg->count && count < max; i++) {
		if (filter_tag && !config_entry_has_tag(&cfg->entries[i], filter_tag))
			continue;
		if (filter_group && !config_entry_has_group(&cfg->entries[i], filter_group))
			continue;
		out_indices[count++] = i;
	}

	return count;
}

void cmd_register_filter_flags(ArgCommand *cmd,
                                const char **out_tag,
                                const char **out_group)
{
	argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
	                    "Filter by tag", out_tag);
	argparse_add_option(cmd, "group", 'g', ARG_TYPE_STRING, "GROUP",
	                    "Filter by group", out_group);
}
