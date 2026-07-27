/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * tags.c — Tag and group query helpers
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "log.h"

static bool has_csv_token(const char *csv, const char *token)
{
	size_t token_len = strlen(token);
	const char *p    = csv;

	while (*p) {
		if (*p == ',') {
			p++;
			continue;
		}
		const char *end = p;
		while (*end && *end != ',')
			end++;

		size_t len = (size_t) (end - p);
		if (len == token_len && memcmp(p, token, token_len) == 0)
			return true;

		p = end;
	}
	return false;
}

bool config_entry_has_tag(const RepoEntry *entry, const char *tag)
{
	if (!entry || !tag || !entry->tags[0])
		return false;
	LOG_TRACE("config_entry_has_tag(%s, %s)", entry->name, tag);
	return has_csv_token(entry->tags, tag);
}

bool config_entry_has_group(const RepoEntry *entry, const char *group)
{
	if (!entry || !group || !entry->groups[0])
		return false;
	LOG_TRACE("config_entry_has_group(%s, %s)", entry->name, group);
	return has_csv_token(entry->groups, group);
}
