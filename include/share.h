/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * share.h — Shared constants and cross-cutting utilities
 */

#ifndef _SHARE__H_
#define _SHARE__H_


#include <stdbool.h>
#include <stddef.h>

/* Forward declaration (full type in config.h) */
typedef struct GitConfig GitConfig;

/* Tag and group limits */
#define MAX_TAGS   10
#define MAX_GROUPS 10

/* Repository limit (enables stack allocation across commands) */
#define MAX_REPOS 50

/* Buffer sizes */
#define MAX_PATH_LEN  512
#define MAX_NAME_LEN  256
#define MAX_COUNT_STR 32

/* Git */
#define GIT_BINARY   "git"
#define GIT_MAX_ARGS 32

/* Process capture */
#define PROCESS_BUF_SIZE 4096

/* Output column width for repo names (non-table mode) */
#define NAME_COL_WIDTH 22

/* Stats frequency map */
#define FREQ_MAP_MAX 512

/* Repeated user-facing messages */
#define MSG_NO_REPOS       "No repositories registered.\n"
#define MSG_CFG_PATH_ERR   "could not determine config path"
#define MSG_CFG_LOAD_ERR   "could not load config"
#define MSG_CFG_SAVE_ERR   "could not save config"
#define MSG_REPO_NOT_FOUND "Repository not found: %s\n"

/* Pluralization helper */
#define PLURAL(n, s, p) ((n) == 1 ? (s) : (p))

/* Exit codes */
#define EXIT_CMD_NOT_FOUND 127

/* ── Config utilities ─────────────────────────────────────────────────────── */

int config_ensure_capacity(GitConfig *cfg);
bool config_has_duplicate_name(const GitConfig *cfg, const char *name, size_t exclude_index);
bool config_has_duplicate_path(const GitConfig *cfg, const char *path, size_t exclude_index);

/* ── Command cleanup ──────────────────────────────────────────────────────── */

void cmd_cleanup(GitConfig *cfg, char *config_path);

/* ── ANSI color utilities ─────────────────────────────────────────────────── */

void ansi_colorize(char *buf, size_t buflen, const char *text, const char *code);
void ansi_print_repo_header(const char *name, bool color);
void ansi_print_repo_empty(const char *name, const char *msg, bool color);

/* ── Date parsing ─────────────────────────────────────────────────────────── */

long parse_date_to_timestamp(const char *date_str);

/*
 * Format a Unix timestamp as a relative time string ("3 hours ago", etc.).
 * Writes into buf (up to buflen bytes). Uses current time as reference.
 */
void format_relative_time(char *buf, size_t buflen, long timestamp);

/* ── String utilities ─────────────────────────────────────────────────────── */

/*
 * Duplicate a string, stripping trailing newline/carriage return.
 * Returns a heap-allocated string, or NULL on OOM. Caller must free.
 */
char *strdup_strip_newline(const char *s);

/* ── Command config helpers ────────────────────────────────────────────────── */

int cmd_save_config(GitConfig *cfg, char *config_path);
int cmd_ensure_config_dir(void);


#endif  // _SHARE__H_
