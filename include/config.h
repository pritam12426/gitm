/*
 * config.h — Repository registry configuration
 *
 * Manages the list of registered Git repositories.
 * Config file: ~/.local/share/gitm/registered_repos.txt
 * Format: /absolute/path:name (one per line)
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_


#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	char *path; /* absolute path to the repo */
	char *name; /* user-friendly alias */
} RepoEntry;

typedef struct {
	RepoEntry *entries;
	size_t     count;
	size_t     capacity;
} GitConfig;


/*
 * Get the default config file path (~/.local/share/gitm/registered_repos.txt).
 * Caller must free the returned string.
 */
char *config_default_path(void);

/*
 * Ensure the config directory exists.
 * Creates ~/.local/share/gitm/ if missing.
 * Returns 0 on success, -1 on error.
 */
int config_ensure_dir(void);

/*
 * Load the config from a file.
 * Returns 0 on success, -1 on error (file not found is not an error — returns empty config).
 */
int config_load(const char *path, GitConfig *cfg);

/*
 * Save the config to a file.
 * Returns 0 on success, -1 on error.
 */
int config_save(const char *path, const GitConfig *cfg);

/*
 * Free all memory owned by the config.
 */
void config_free(GitConfig *cfg);

/*
 * Add a repo to the config. Returns 0 on success, -1 on error (duplicate path or name).
 */
int config_add(GitConfig *cfg, const char *path, const char *name);

/*
 * Remove a repo by name. Returns 0 on success, -1 if not found.
 */
int config_remove(GitConfig *cfg, const char *name);

/*
 * Find a repo by name. Returns NULL if not found.
 */
RepoEntry *config_find(GitConfig *cfg, const char *name);

/*
 * Rename a repo. Returns 0 on success, -1 on error.
 */
int config_rename(GitConfig *cfg, const char *old_name, const char *new_name);

/*
 * Validate the config: check for duplicate names, duplicate paths,
 * non-existent directories, and non-git repos.
 * Prints errors to stderr. Returns the number of errors found.
 */
int config_validate(GitConfig *cfg);


#ifdef __cplusplus
}
#endif


#endif /* _CONFIG_H_ */
