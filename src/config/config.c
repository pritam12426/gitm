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
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		if (len == 0 || line[0] == '#')
			continue;

		/* Parse: /path:name[:tags[:groups]] */
		char *first_colon = strchr(line, ':');
		if (!first_colon) {
			LOG_WARN("skipping malformed line: %s", line);
			continue;
		}

		*first_colon    = '\0';
		char *path_str  = line;
		char *rest      = first_colon + 1;

		char *second_colon = strchr(rest, ':');
		char *name_str;
		char *tags_str  = NULL;
		char *groups_str = NULL;

		if (second_colon) {
			*second_colon = '\0';
			name_str = rest;

			char *after_name  = second_colon + 1;
			char *third_colon = strchr(after_name, ':');

			if (third_colon) {
				*third_colon = '\0';
				tags_str = after_name;

				char *after_tags   = third_colon + 1;
				char *fourth_colon = strchr(after_tags, ':');
				if (fourth_colon) {
					*fourth_colon = '\0';
					groups_str = after_tags;
				} else {
					groups_str = after_tags;
				}
			} else {
				tags_str = after_name;
			}
		} else {
			name_str = rest;
		}

		/* Trim whitespace */
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

		if (cfg->count >= cfg->capacity) {
			size_t     new_cap = cfg->capacity == 0 ? 8 : cfg->capacity * 2;
			RepoEntry *tmp     = realloc(cfg->entries, new_cap * sizeof(RepoEntry));
			if (!tmp) {
				fclose(f);
				return -1;
			}
			cfg->entries  = tmp;
			cfg->capacity = new_cap;
		}

		cfg->entries[cfg->count].path   = strdup(path_str);
		cfg->entries[cfg->count].name   = strdup(name_str);
		cfg->entries[cfg->count].tags   = tags_str ? strdup(tags_str) : NULL;
		cfg->entries[cfg->count].groups = groups_str ? strdup(groups_str) : NULL;
		cfg->count++;
	}

	fclose(f);
	LOG_DEBUG("loaded %zu entries from config", cfg->count);
	return 0;
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

	for (size_t i = 0; i < cfg->count; i++) {
		if (cfg->entries[i].tags && cfg->entries[i].groups)
			fprintf(f, "%s:%s:%s:%s\n", cfg->entries[i].path, cfg->entries[i].name,
			        cfg->entries[i].tags, cfg->entries[i].groups);
		else if (cfg->entries[i].tags)
			fprintf(f, "%s:%s:%s\n", cfg->entries[i].path, cfg->entries[i].name,
			        cfg->entries[i].tags);
		else
			fprintf(f, "%s:%s\n", cfg->entries[i].path, cfg->entries[i].name);
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
		free(cfg->entries[i].tags);
		free(cfg->entries[i].groups);
	}
	free(cfg->entries);
	cfg->entries  = NULL;
	cfg->count    = 0;
	cfg->capacity = 0;
}
