/*
 * config.c — Repository registry configuration
 *
 * Loads, saves, and validates the list of registered Git repositories.
 * Config file format: /absolute/path:name (one per line)
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

#define CONFIG_DIR   ".local/share/gitm"
#define CONFIG_FILE  "registered_repos.txt"
#define MAX_LINE_LEN 1024

/* ── Path helpers ─────────────────────────────────────────────────────────────
 */

char *config_default_path(void)
{
	const char *home = getenv("HOME");
	if (!home)
		return NULL;

	char path[512];
	snprintf(path, sizeof(path), "%s/%s/%s", home, CONFIG_DIR, CONFIG_FILE);
	return strdup(path);
}

int config_ensure_dir(void)
{
	const char *home = getenv("HOME");
	if (!home)
		return -1;

	char dir[512];
	snprintf(dir, sizeof(dir), "%s/%s", home, CONFIG_DIR);

	struct stat st;
	if (stat(dir, &st) == 0) {
		/* Exists — check it's a directory */
		if (S_ISDIR(st.st_mode))
			return 0;
		return -1;
	}

	/* Create directory */
	if (mkdir(dir, 0755) != 0 && errno != EEXIST)
		return -1;

	return 0;
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

	FILE *f = fopen(path, "r");
	if (!f)
		return 0; /* File not found → empty config */

	char line[MAX_LINE_LEN];
	while (fgets(line, sizeof(line), f)) {
		/* Strip newline */
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		/* Skip empty lines and comments */
		if (len == 0 || line[0] == '#')
			continue;

		/* Parse: /path:name */
		char *colon = strchr(line, ':');
		if (!colon) {
			LOG_WARN("skipping malformed line: %s", line);
			continue;
		}

		*colon         = '\0';
		char *path_str = line;
		char *name_str = colon + 1;

		/* Trim whitespace */
		while (*path_str == ' ' || *path_str == '\t')
			path_str++;
		while (*name_str == ' ' || *name_str == '\t')
			name_str++;

		if (path_str[0] == '\0' || name_str[0] == '\0') {
			LOG_WARN("skipping empty entry: %s", line);
			continue;
		}

		if (ensure_capacity(cfg) != 0) {
			fclose(f);
			return -1;
		}

		cfg->entries[cfg->count].path = strdup(path_str);
		cfg->entries[cfg->count].name = strdup(name_str);
		cfg->count++;
	}

	fclose(f);
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
		fprintf(f, "%s:%s\n", cfg->entries[i].path, cfg->entries[i].name);
	}

	fclose(f);
	return 0;
}

void config_free(GitConfig *cfg)
{
	if (!cfg)
		return;
	for (size_t i = 0; i < cfg->count; i++) {
		free(cfg->entries[i].path);
		free(cfg->entries[i].name);
	}
	free(cfg->entries);
	cfg->entries  = NULL;
	cfg->count    = 0;
	cfg->capacity = 0;
}

int config_add(GitConfig *cfg, const char *path, const char *name)
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
	cfg->entries[cfg->count].name = strdup(name);
	cfg->count++;

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

			/* Shift remaining entries */
			for (size_t j = i; j < cfg->count - 1; j++) {
				cfg->entries[j] = cfg->entries[j + 1];
			}
			cfg->count--;
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
