/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * crud.c — Config add/remove/find/rename operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "log.h"

int config_add(GitConfig *cfg, const char *path, const char *name,
               const char *tags, const char *groups)
{
	if (!cfg || !path || !name)
		return -1;

	LOG_TRACE("config_add(%s, %s)", name, path);
	if (config_has_duplicate_path(cfg, path, cfg->count)) {
		LOG_ERROR("path already registered: %s", path);
		return -1;
	}
	if (config_has_duplicate_name(cfg, name, cfg->count)) {
		LOG_ERROR("name already registered: %s", name);
		return -1;
	}

	if (config_ensure_capacity(cfg) != 0)
		return -1;

	/* Resolve to absolute path */
	char abs_path[MAX_PATH_LEN];
	if (realpath(path, abs_path)) {
	cfg->entries[cfg->count].path   = strdup(abs_path);
	} else {
		cfg->entries[cfg->count].path   = strdup(path);
	}
	cfg->entries[cfg->count].name   = strdup(name);
	if (tags)
		strncpy(cfg->entries[cfg->count].tags, tags, TAG_BUF_SIZE - 1);
	else
		cfg->entries[cfg->count].tags[0] = '\0';
	if (groups)
		strncpy(cfg->entries[cfg->count].groups, groups, GROUP_BUF_SIZE - 1);
	else
		cfg->entries[cfg->count].groups[0] = '\0';
	cfg->count++;

	LOG_DEBUG("added entry: %s (%s)", name, path);
	return 0;
}

int config_remove(GitConfig *cfg, const char *name)
{
	if (!cfg || !name)
		return -1;

	LOG_TRACE("config_remove(%s)", name);
	for (size_t i = 0; i < cfg->count; i++) {
		if (strcmp(cfg->entries[i].name, name) == 0) {
			free(cfg->entries[i].path);
			free(cfg->entries[i].name);

			for (size_t j = i; j < cfg->count - 1; j++)
				cfg->entries[j] = cfg->entries[j + 1];

			cfg->count--;
			LOG_DEBUG("removed entry: %s", name);
			return 0;
		}
	}

	LOG_ERROR("repo not found: %s", name);
	return -1;
}

RepoEntry *config_find(GitConfig *cfg, const char *name)
{
	if (!cfg || !name)
		return NULL;

	LOG_TRACE("config_find(%s)", name);
	for (size_t i = 0; i < cfg->count; i++) {
		if (strcmp(cfg->entries[i].name, name) == 0) {
			LOG_TRACE("config_find: found at index %zu", i);
			return &cfg->entries[i];
		}
	}
	LOG_TRACE("config_find: not found");
	return NULL;
}

int config_rename(GitConfig *cfg, const char *old_name, const char *new_name)
{
	if (!cfg || !old_name || !new_name)
		return -1;

	LOG_TRACE("config_rename(%s -> %s)", old_name, new_name);
	RepoEntry *entry = config_find(cfg, old_name);
	if (!entry) {
		LOG_ERROR("repo not found: %s", old_name);
		return -1;
	}

	if (config_has_duplicate_name(cfg, new_name, cfg->count)) {
		LOG_ERROR("name already in use: %s", new_name);
		return -1;
	}

	free(entry->name);
	entry->name = strdup(new_name);
	LOG_DEBUG("renamed %s -> %s", old_name, new_name);
	return 0;
}
