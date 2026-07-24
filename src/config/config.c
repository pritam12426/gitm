/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * config.c — Repository registry configuration
 *
 * Loads, saves, and validates the list of registered Git repositories.
 * Config file format: /absolute/path:name[:tags[:groups]] (one per line)
 *
 * Config file resolution:
 *   1. $XDG_DATA_HOME/gitm/registered_repos.txt
 *   2. macOS: ~/Library/Application Support/gitm/registered_repos.txt
 *   3. Linux: ~/.local/share/gitm/registered_repos.txt
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "git.h"
#include "log.h"

#define CONFIG_FILE  "registered_repos.txt"
#define MAX_LINE_LEN 1024

/* ── Path helpers ─────────────────────────────────────────────────────────────
 *
 * Config file resolution order:
 *   1. $XDG_DATA_HOME/gitm/registered_repos.txt  (if XDG_DATA_HOME is set)
 *   2. macOS:  ~/Library/Application Support/gitm/registered_repos.txt
 *   3. Linux:  ~/.local/share/gitm/registered_repos.txt
 *
 * If the resolved path does not exist, config_default_path() still returns
 * the path — callers use config_ensure_dir() before writing.
 */

char *config_default_path(void)
{
	const char *xdg = getenv("XDG_DATA_HOME");
	if (xdg && xdg[0] != '\0') {
		char path[512];
		snprintf(path, sizeof(path), "%s/gitm/%s", xdg, CONFIG_FILE);
		LOG_TRACE("config path (XDG): %s", path);
		return strdup(path);
	}

	const char *home = getenv("HOME");
	if (!home) {
		LOG_WARN("neither XDG_DATA_HOME nor HOME is set");
		return NULL;
	}

	char path[512];
#if defined(__APPLE__)
	snprintf(path, sizeof(path), "%s/Library/Application Support/gitm/%s", home,
	         CONFIG_FILE);
#else
	snprintf(path, sizeof(path), "%s/.local/share/gitm/%s", home, CONFIG_FILE);
#endif
	LOG_TRACE("config path (fallback): %s", path);
	return strdup(path);
}

int config_ensure_dir(void)
{
	char *path = config_default_path();
	if (!path)
		return -1;

	/* Find last '/' to isolate directory portion */
	char *last_slash = strrchr(path, '/');
	if (!last_slash) {
		free(path);
		return -1;
	}
	*last_slash = '\0'; /* path now holds just the directory */

	LOG_TRACE("ensuring config dir exists: %s", path);

	struct stat st;
	if (stat(path, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			free(path);
			return 0;
		}
		LOG_WARN("config path exists but is not a directory: %s", path);
		free(path);
		return -1;
	}

	/* Create directory (and any missing parents via recursive approach) */
	int rc = 0;
#if defined(__APPLE__)
	/* macOS: mkdir -p equivalent via recursive creation */
	char *p = path;
	while (*p) {
		if (*p == '/') {
			*p = '\0';
			if (path[0] != '\0') { /* skip empty root */
				if (mkdir(path, 0755) != 0 && errno != EEXIST) {
					LOG_ERROR("failed to create config dir: %s: %s", path, strerror(errno));
					rc = -1;
					break;
				}
			}
			*p = '/';
		}
		p++;
	}
	/* Final component */
	if (rc == 0 && mkdir(path, 0755) != 0 && errno != EEXIST) {
		LOG_ERROR("failed to create config dir: %s: %s", path, strerror(errno));
		rc = -1;
	}
#else
	/* Linux: mkdir -p equivalent */
	char *p = path;
	while (*p) {
		if (*p == '/') {
			*p = '\0';
			if (path[0] != '\0') {
				if (mkdir(path, 0755) != 0 && errno != EEXIST) {
					LOG_ERROR("failed to create config dir: %s: %s", path, strerror(errno));
					rc = -1;
					break;
				}
			}
			*p = '/';
		}
		p++;
	}
	if (rc == 0 && mkdir(path, 0755) != 0 && errno != EEXIST) {
		LOG_ERROR("failed to create config dir: %s: %s", path, strerror(errno));
		rc = -1;
	}
#endif

	if (rc == 0)
		LOG_DEBUG("created config dir: %s", path);

	free(path);
	return rc;
}

/* ── Config entry management ──────────────────────────────────────────────────
 */

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

/* ── Public API ───────────────────────────────────────────────────────────────
 */

int config_load(const char *path, GitConfig *cfg)
{
	if (!cfg)
		return -1;

	cfg->entries  = NULL;
	cfg->count    = 0;
	cfg->capacity = 0;

	LOG_DEBUG("loading config from %s", path);

	FILE *f = fopen(path, "r");
	if (!f) {
		LOG_DEBUG("config file not found, starting empty");
		return 0; /* File not found → empty config */
	}

	char line[MAX_LINE_LEN];
	while (fgets(line, sizeof(line), f)) {
		/* Strip newline */
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		/* Skip empty lines and comments */
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

		/* Find second colon for name */
		char *second_colon = strchr(rest, ':');
		char *name_str;
		char *tags_str  = NULL;
		char *groups_str = NULL;

		if (second_colon) {
			*second_colon = '\0';
			name_str = rest;

			/* Find third colon for tags */
			char *after_name = second_colon + 1;
			char *third_colon = strchr(after_name, ':');

			if (third_colon) {
				*third_colon = '\0';
				tags_str = after_name;

				/* Find fourth colon for groups */
				char *after_tags = third_colon + 1;
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

		if (ensure_capacity(cfg) != 0) {
			fclose(f);
			return -1;
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

			/* Shift remaining entries */
			for (size_t j = i; j < cfg->count - 1; j++) {
				cfg->entries[j] = cfg->entries[j + 1];
			}
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

int config_validate(GitConfig *cfg)
{
	if (!cfg)
		return 0;

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

/* ── Tag / group / orphan helpers ──────────────────────────────────────────────
 */

bool config_entry_has_tag(const RepoEntry *entry, const char *tag)
{
	if (!entry || !tag || !entry->tags)
		return false;

	/* Walk comma-separated list */
	const char *p = entry->tags;
	size_t tag_len = strlen(tag);

	while (*p) {
		/* Skip leading comma */
		if (*p == ',') {
			p++;
			continue;
		}
		/* Find end of this token */
		const char *end = p;
		while (*end && *end != ',')
			end++;

		size_t token_len = (size_t) (end - p);
		if (token_len == tag_len && memcmp(p, tag, tag_len) == 0)
			return true;

		p = end;
	}
	return false;
}

bool config_entry_has_group(const RepoEntry *entry, const char *group)
{
	if (!entry || !group || !entry->groups)
		return false;

	const char *p = entry->groups;
	size_t group_len = strlen(group);

	while (*p) {
		if (*p == ',') {
			p++;
			continue;
		}
		const char *end = p;
		while (*end && *end != ',')
			end++;

		size_t token_len = (size_t) (end - p);
		if (token_len == group_len && memcmp(p, group, group_len) == 0)
			return true;

		p = end;
	}
	return false;
}

size_t config_find_orphans(const GitConfig *cfg, size_t *out_indices, size_t max)
{
	if (!cfg || !out_indices)
		return 0;

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
