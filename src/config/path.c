/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * path.c — Config file path resolution and directory creation
 *
 * Config file resolution order:
 *   1. $XDG_DATA_HOME/gitm/registered_repos.txt
 *   2. macOS:  ~/Library/Application Support/gitm/registered_repos.txt
 *   3. Linux:  ~/.local/share/gitm/registered_repos.txt
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "log.h"

#define CONFIG_FILE "registered_repos.txt"

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
	*last_slash = '\0';

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

	/* Create directory (and any missing parents) */
	int rc = 0;
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
	/* Final component */
	if (rc == 0 && mkdir(path, 0755) != 0 && errno != EEXIST) {
		LOG_ERROR("failed to create config dir: %s: %s", path, strerror(errno));
		rc = -1;
	}

	if (rc == 0)
		LOG_DEBUG("created config dir: %s", path);

	free(path);
	return rc;
}
