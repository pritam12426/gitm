/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * share.c — Shared utilities
 *
 * Cross-cutting helpers used across commands, config, and git modules.
 */

#define _POSIX_C_SOURCE 200809L

#include "share.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ansi_color.h"
#include "config.h"
#include "log.h"

/* ── Config utilities ────────────────────────────────────────────────────────── */

int config_ensure_capacity(GitConfig *cfg)
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

bool config_has_duplicate_name(const GitConfig *cfg, const char *name, size_t exclude_index)
{
	for (size_t i = 0; i < cfg->count; i++) {
		if (i == exclude_index)
			continue;
		if (cfg->entries[i].name && strcmp(cfg->entries[i].name, name) == 0)
			return true;
	}
	return false;
}

bool config_has_duplicate_path(const GitConfig *cfg, const char *path, size_t exclude_index)
{
	for (size_t i = 0; i < cfg->count; i++) {
		if (i == exclude_index)
			continue;
		if (cfg->entries[i].path && strcmp(cfg->entries[i].path, path) == 0)
			return true;
	}
	return false;
}

/* ── Command cleanup ─────────────────────────────────────────────────────────── */

void cmd_cleanup(GitConfig *cfg, char *config_path)
{
	config_free(cfg);
	free(config_path);
}

/* ── ANSI color utilities ────────────────────────────────────────────────────── */

void ansi_colorize(char *buf, size_t buflen, const char *text, const char *code)
{
	snprintf(buf, buflen, "%s%s%s", code, text, ANSI_RESET);
}

void ansi_print_repo_header(const char *name, bool color)
{
	if (color)
		fprintf(stderr, "\n%s%s%s%s\n", ANSI_BOLD, ANSI_FG_CYAN, name, ANSI_RESET);
	else
		fprintf(stderr, "\n%s\n", name);
}

void ansi_print_repo_empty(const char *name, const char *msg, bool color)
{
	if (color)
		fprintf(stderr, "\n%s%s%s%s\n  %s%s%s\n", ANSI_BOLD, ANSI_FG_CYAN, name, ANSI_RESET,
		        ANSI_DIM, msg, ANSI_RESET);
	else
		fprintf(stderr, "\n%s\n  %s\n", name, msg);
}
