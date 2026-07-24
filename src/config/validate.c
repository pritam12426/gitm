/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * validate.c — Config validation and orphan cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "git.h"
#include "log.h"

static bool has_duplicate_name(const GitConfig *cfg, const char *name, size_t exclude_index)
{
	for (size_t i = 0; i < cfg->count; i++) {
		if (i == exclude_index)
			continue;
		if (cfg->entries[i].name && strcmp(cfg->entries[i].name, name) == 0)
			return true;
	}
	return false;
}

static bool has_duplicate_path(const GitConfig *cfg, const char *path, size_t exclude_index)
{
	for (size_t i = 0; i < cfg->count; i++) {
		if (i == exclude_index)
			continue;
		if (cfg->entries[i].path && strcmp(cfg->entries[i].path, path) == 0)
			return true;
	}
	return false;
}

int config_validate(GitConfig *cfg)
{
	if (!cfg)
		return 0;

	LOG_TRACE("config_validate(%zu entries)", cfg->count);
	int errors = 0;

	/* Check for duplicate names and paths */
	for (size_t i = 0; i < cfg->count; i++) {
		if (has_duplicate_name(cfg, cfg->entries[i].name, i)) {
			LOG_ERROR("duplicate name: %s", cfg->entries[i].name);
			errors++;
		}
		if (has_duplicate_path(cfg, cfg->entries[i].path, i)) {
			LOG_ERROR("duplicate path: %s", cfg->entries[i].path);
			errors++;
		}
	}

	/* Check for existence and git validity */
	for (size_t i = 0; i < cfg->count; i++) {
		struct stat st;
		if (stat(cfg->entries[i].path, &st) != 0) {
			LOG_ERROR("path does not exist: %s", cfg->entries[i].path);
			errors++;
			continue;
		}
		if (!S_ISDIR(st.st_mode)) {
			LOG_ERROR("path is not a directory: %s", cfg->entries[i].path);
			errors++;
			continue;
		}
		if (!git_is_repo(cfg->entries[i].path)) {
			LOG_WARN("not a git repository: %s", cfg->entries[i].path);
			errors++;
		}
	}

	return errors;
}

size_t config_find_orphans(const GitConfig *cfg, size_t *out_indices, size_t max)
{
	if (!cfg || !out_indices)
		return 0;

	LOG_TRACE("config_find_orphans(%zu entries)", cfg->count);
	size_t found = 0;

	for (size_t i = 0; i < cfg->count; i++) {
		struct stat st;
		if (stat(cfg->entries[i].path, &st) != 0 || !S_ISDIR(st.st_mode)) {
			if (found < max)
				out_indices[found] = i;
			found++;
		}
	}

	return found;
}

int config_remove_at_indices(GitConfig *cfg, const size_t *indices, size_t count)
{
	if (!cfg || !indices || count == 0)
		return -1;

	LOG_TRACE("config_remove_at_indices(%zu indices)", count);
	/*
	 * Process indices in reverse order so earlier indices remain valid
	 * as we shift entries.
	 */
	for (size_t k = count; k > 0; k--) {
		size_t i = indices[k - 1];
		if (i >= cfg->count)
			return -1;

		free(cfg->entries[i].path);
		free(cfg->entries[i].name);
		free(cfg->entries[i].tags);
		free(cfg->entries[i].groups);

		for (size_t j = i; j < cfg->count - 1; j++)
			cfg->entries[j] = cfg->entries[j + 1];

		cfg->count--;
	}

	return 0;
}
