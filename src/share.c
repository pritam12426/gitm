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
#include <time.h>

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

/* ── Date parsing ───────────────────────────────────────────────────────────── */

long parse_date_to_timestamp(const char *date_str)
{
	int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

	if (sscanf(date_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6)
		return 0;

	struct tm tm = { 0 };
	tm.tm_year = year - 1900;
	tm.tm_mon  = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min  = min;
	tm.tm_sec  = sec;

	return (long) mktime(&tm);
}

void format_relative_time(char *buf, size_t buflen, long timestamp)
{
	if (timestamp <= 0) {
		snprintf(buf, buflen, "unknown");
		return;
	}

	long diff = (long) time(NULL) - timestamp;
	if (diff < 0)
		diff = 0;

	if (diff < 60)
		snprintf(buf, buflen, "just now");
	else if (diff < 3600) {
		long v = diff / 60;
		snprintf(buf, buflen, "%ld min ago", v);
	} else if (diff < 86400) {
		long v = diff / 3600;
		snprintf(buf, buflen, "%ld hour%s ago", v, v == 1 ? "" : "s");
	} else if (diff < 604800) {
		long v = diff / 86400;
		snprintf(buf, buflen, "%ld day%s ago", v, v == 1 ? "" : "s");
	} else if (diff < 2592000) {
		long v = diff / 604800;
		snprintf(buf, buflen, "%ld week%s ago", v, v == 1 ? "" : "s");
	} else if (diff < 31536000) {
		long v = diff / 2592000;
		snprintf(buf, buflen, "%ld month%s ago", v, v == 1 ? "" : "s");
	} else {
		long v = diff / 31536000;
		snprintf(buf, buflen, "%ld year%s ago", v, v == 1 ? "" : "s");
	}
}

/* ── Command config helpers ─────────────────────────────────────────────────── */

int cmd_save_config(GitConfig *cfg, char *config_path)
{
	if (config_save(config_path, cfg) != 0) {
		LOG_ERROR(MSG_CFG_SAVE_ERR);
		cmd_cleanup(cfg, config_path);
		return -1;
	}
	return 0;
}

int cmd_ensure_config_dir(void)
{
	char *path = config_default_path();
	if (!path) {
		LOG_ERROR(MSG_CFG_PATH_ERR);
		return -1;
	}
	int rc = config_ensure_dir();
	free(path);
	return rc;
}
