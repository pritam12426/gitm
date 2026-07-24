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

static int ensure_capacity(GitConfig *cfg)
{
	if (cfg->count < cfg->capacity)
		return 0;

	size_t     new_cap = cfg->capacity == 0 ? 8 : cfg->capacity * 2;
	RepoEntry *tmp     = realloc(cfg->entries, new_cap * sizeof(RepoEntry));
	if (!tmp)
		return -1;

	cfg->entries  = tmp;
	cfg->capacity = new_cap;
	return 0;
}

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

int config_add(GitConfig *cfg, const char *path, const char *name,
               const char *tags, const char *groups)
{
	if (!cfg || !path || !name)
		return -1;

	if (has_duplicate_path(cfg, path, cfg->count)) {
		LOG_ERROR("path already registered: %s", path);
		return -1;
	}
	if (has_duplicate_name(cfg, name, cfg->count)) {
		LOG_ERROR("name already registered: %s", name);
		return -1;
	}

	if (ensure_capacity(cfg) != 0)
		return -1;

	/* Resolve to absolute path */
	char abs_path[512];
	if (realpath(path, abs_path)) {
		cfg->entries[cfg->count].path = strdup(abs_path);
	} else {
		cfg->entries[cfg->count].path = strdup(path);
	}
	cfg->entries[cfg->count].name   = strdup(name);
	cfg->entries[cfg->count].tags   = tags ? strdup(tags) : NULL;
	cfg->entries[cfg->count].groups = groups ? strdup(groups) : NULL;
	cfg->count++;

	LOG_DEBUG("added entry: %s (%s)", name, path);
	return 0;
}

int config_remove(GitConfig *cfg, const char *name)
{
	if (!cfg || !name)
		return -1;

	for (size_t i = 0; i < cfg->count; i++) {
		if (strcmp(cfg->entries[i].name, name) == 0) {
			free(cfg->entries[i].path);
			free(cfg->entries[i].name);
			free(cfg->entries[i].tags);
			free(cfg->entries[i].groups);

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

	for (size_t i = 0; i < cfg->count; i++) {
		if (strcmp(cfg->entries[i].name, name) == 0)
			return &cfg->entries[i];
	}
	return NULL;
}

int config_rename(GitConfig *cfg, const char *old_name, const char *new_name)
{
	if (!cfg || !old_name || !new_name)
		return -1;

	RepoEntry *entry = config_find(cfg, old_name);
	if (!entry) {
		LOG_ERROR("repo not found: %s", old_name);
		return -1;
	}

	if (has_duplicate_name(cfg, new_name, cfg->count)) {
		LOG_ERROR("name already in use: %s", new_name);
		return -1;
	}

	free(entry->name);
	entry->name = strdup(new_name);
	LOG_DEBUG("renamed %s -> %s", old_name, new_name);
	return 0;
}
