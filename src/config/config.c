/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * config.c — Config load, save, and free
 *
 * Config file format: /absolute/path:name[:tags[:groups]] (one per line)
 *
 * Split into:
 *   path.c     — path resolution and directory creation
 *   config.c   — load/save/free
 *   crud.c     — add/remove/find/rename
 *   validate.c — validation and orphan cleanup
 *   tags.c     — tag/group query helpers
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define MAX_LINE_LEN 1024

int config_load(const char *path, GitConfig *cfg)
{
	if (!cfg)
		return -1;

	LOG_TRACE("config_load(%s)", path);
	cfg->entries  = NULL;
	cfg->count    = 0;
	cfg->capacity = 0;

	LOG_DEBUG("loading config from %s", path);

	FILE *f = fopen(path, "r");
	if (!f) {
		LOG_DEBUG("config file not found, starting empty");
		return 0;
	}

	char line[MAX_LINE_LEN];
	while (fgets(line, sizeof(line), f)) {
		// Strip trailing newlines
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		if (len == 0 || line[0] == '#')
			continue;

		// Parse: /path:name[:tags[:groups]]
		char *first_colon = strchr(line, ':');
		if (!first_colon) {
			LOG_WARN("skipping malformed line: %s", line);
			continue;
		}

		*first_colon   = '\0';
		char *path_str = line;
		char *rest     = first_colon + 1;

		char *second_colon = strchr(rest, ':');
		char *name_str;
		char *tags_str   = NULL;
		char *groups_str = NULL;

		if (second_colon) {
			*second_colon = '\0';
			name_str      = rest;

			char *after_name  = second_colon + 1;
			char *third_colon = strchr(after_name, ':');

			if (third_colon) {
				*third_colon = '\0';
				tags_str     = after_name;

				char *after_tags   = third_colon + 1;
				char *fourth_colon = strchr(after_tags, ':');
				if (fourth_colon) {
					*fourth_colon = '\0';
					groups_str    = after_tags;
				} else {
					groups_str = after_tags;
				}
			} else {
				tags_str = after_name;
			}
		} else {
			name_str = rest;
		}

		// Trim whitespace
		while (*path_str == ' ' || *path_str == '\t')
			path_str++;
		while (*name_str == ' ' || *name_str == '\t')
			name_str++;
		if (tags_str) {
			while (*tags_str == ' ' || *tags_str == '\t')
				tags_str++;
			if (*tags_str == '\0')
				tags_str = NULL;
		}
		if (groups_str) {
			while (*groups_str == ' ' || *groups_str == '\t')
				groups_str++;
			if (*groups_str == '\0')
				groups_str = NULL;
		}

		if (path_str[0] == '\0' || name_str[0] == '\0') {
			LOG_WARN("skipping empty entry: %s", line);
			continue;
		}

		if (cfg->count >= MAX_REPOS) {
			LOG_WARN("repo limit reached (%d), skipping: %s", MAX_REPOS, name_str);
			continue;
		}

		if (config_ensure_capacity(cfg) != 0) {
			fclose(f);
			return -1;
		}

		if (repo_entry_init(&cfg->entries[cfg->count], path_str, name_str) != 0) {
			fclose(f);
			return -1;
		}

		repo_entry_set_tags_groups(&cfg->entries[cfg->count], tags_str, groups_str);
		cfg->count++;
	}

	fclose(f);
	LOG_DEBUG("loaded %zu entries from config", cfg->count);
	return 0;
}

int repo_entry_init(RepoEntry *entry, const char *path, const char *name)
{
	entry->path = strdup(path);
	entry->name = strdup(name);
	if (!entry->path || !entry->name) {
		free(entry->path);
		free(entry->name);
		entry->path = NULL;
		entry->name = NULL;
		return -1;
	}
	return 0;
}

void repo_entry_set_tags_groups(RepoEntry *entry, const char *tags, const char *groups)
{
	entry->tags[0]   = '\0';
	entry->groups[0] = '\0';
	if (tags)
		strncpy(entry->tags, tags, TAG_BUF_SIZE - 1);
	entry->tags[TAG_BUF_SIZE - 1] = '\0';
	if (groups)
		strncpy(entry->groups, groups, GROUP_BUF_SIZE - 1);
	entry->groups[GROUP_BUF_SIZE - 1] = '\0';
}

int config_save(const char *path, const GitConfig *cfg)
{
	if (!cfg)
		return -1;

	LOG_TRACE("config_save(%s, %zu entries)", path, cfg->count);
	FILE *f = fopen(path, "w");
	if (!f) {
		LOG_ERROR("could not open config for writing: %s", path);
		return -1;
	}

	char line[MAX_LINE_LEN];
	for (size_t i = 0; i < cfg->count; i++) {
		const RepoEntry *e = &cfg->entries[i];
		if (e->tags[0] && e->groups[0])
			snprintf(line, sizeof(line), "%s:%s:%s:%s\n", e->path, e->name, e->tags, e->groups);
		else if (e->tags[0])
			snprintf(line, sizeof(line), "%s:%s:%s\n", e->path, e->name, e->tags);
		else if (e->groups[0])
			snprintf(line, sizeof(line), "%s:%s::%s\n", e->path, e->name, e->groups);
		else
			snprintf(line, sizeof(line), "%s:%s\n", e->path, e->name);
		fputs(line, f);
	}

	fclose(f);
	LOG_DEBUG("saved %zu entries to config", cfg->count);
	return 0;
}

void config_free(GitConfig *cfg)
{
	if (!cfg)
		return;

	LOG_TRACE("config_free(%zu entries)", cfg->count);
	for (size_t i = 0; i < cfg->count; i++) {
		free(cfg->entries[i].path);
		free(cfg->entries[i].name);
	}
	free(cfg->entries);
	cfg->entries  = NULL;
	cfg->count    = 0;
	cfg->capacity = 0;
}
