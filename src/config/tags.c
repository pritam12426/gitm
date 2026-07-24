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

bool config_entry_has_tag(const RepoEntry *entry, const char *tag)
{
	if (!entry || !tag || !entry->tags)
		return false;

	const char *p = entry->tags;
	size_t tag_len = strlen(tag);

	while (*p) {
		if (*p == ',') {
			p++;
			continue;
		}
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
