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

#include "cmd_util.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "parallel.h"

typedef struct {
	bool path_exists;
	bool is_dir;
	bool is_git_repo;
} ValidateResult;

static void validate_collect(const RepoEntry *entry, void *out)
{
	ValidateResult *r   = out;
	RepoHealth      h   = repo_check_health(entry->path);
	r->path_exists      = (h != REPO_HEALTH_MISSING);
	r->is_dir           = (h == REPO_HEALTH_OK || h == REPO_HEALTH_NOT_GIT);
	r->is_git_repo      = (h == REPO_HEALTH_OK);
}

int config_validate(GitConfig *cfg)
{
	if (!cfg)
		return 0;

	LOG_TRACE("config_validate(%zu entries)", cfg->count);
	int errors = 0;

	// Check for duplicate names and paths
	for (size_t i = 0; i < cfg->count; i++) {
		if (config_has_duplicate_name(cfg, cfg->entries[i].name, i)) {
			LOG_ERROR("duplicate name: %s", cfg->entries[i].name);
			errors++;
		}
		if (config_has_duplicate_path(cfg, cfg->entries[i].path, i)) {
			LOG_ERROR("duplicate path: %s", cfg->entries[i].path);
			errors++;
		}
	}

	// Check for existence and git validity in parallel
	if (cfg->count > 0) {
		size_t indices[MAX_REPOS];
		for (size_t i = 0; i < cfg->count && i < MAX_REPOS; i++)
			indices[i] = i;

		ValidateResult results[MAX_REPOS] = { 0 };
		parallel_collect(cfg, indices, cfg->count, validate_collect, sizeof(ValidateResult), results);

		for (size_t i = 0; i < cfg->count; i++) {
			if (!results[i].path_exists) {
				LOG_ERROR("path does not exist: %s", cfg->entries[i].path);
				errors++;
			} else if (!results[i].is_dir) {
				LOG_ERROR("path is not a directory: %s", cfg->entries[i].path);
				errors++;
			} else if (!results[i].is_git_repo) {
				LOG_WARN("not a git repository: %s", cfg->entries[i].path);
				errors++;
			}
		}
	}

	return errors;
}

size_t config_find_orphans(const GitConfig *cfg, size_t *out_indices, size_t max)
{
	if (!cfg || !out_indices || max == 0)
		return 0;

	LOG_TRACE("config_find_orphans(%zu entries)", cfg->count);
	size_t found = 0;

	for (size_t i = 0; i < cfg->count && found < max; i++) {
		struct stat st;
		if (stat(cfg->entries[i].path, &st) != 0 || !S_ISDIR(st.st_mode)) {
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

	// Validate indices are in bounds and sorted
	for (size_t k = 0; k < count; k++) {
		if (indices[k] >= cfg->count)
			return -1;
		if (k > 0 && indices[k] <= indices[k - 1])
			return -1;
	}

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

		for (size_t j = i; j < cfg->count - 1; j++)
			cfg->entries[j] = cfg->entries[j + 1];

		cfg->count--;
	}

	return 0;
}
