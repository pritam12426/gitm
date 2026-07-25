/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * error.c — Error messages with suggestions
 *
 * Provides helpful error output when parsing fails.
 * Uses simple string similarity for "did you mean?" suggestions.
 * Colour scheme: bold program name, bold red "error:" prefix,
 * red error description.
 */

#define _POSIX_C_SOURCE 200809L /* for fileno() under strict -std=c17 */

#include "error.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ── Internal ANSI codes ──────────────────────────────────────────────────── */

static int g_use_color = -1;

static void detect_color(void)
{
	if (g_use_color >= 0)
		return;
	g_use_color = isatty(fileno(stderr)) ? 1 : 0;
}

#define C_RESET      (g_use_color ? "\x1b[0m" : "")
#define C_BOLD       (g_use_color ? "\x1b[1m" : "")
#define C_RED        (g_use_color ? "\x1b[31m" : "")
#define C_BOLD_RED   (g_use_color ? "\x1b[1;31m" : "")
#define C_DIM        (g_use_color ? "\x1b[2m" : "")

/* Simple Levenshtein distance */
static int levenshtein(const char *a, const char *b)
{
	int la = (int) strlen(a);
	int lb = (int) strlen(b);

	int d[128][128]; /* small limits */

	if (la > 127 || lb > 127)
		return 999;

	for (int i = 0; i <= la; i++)
		d[i][0] = i;
	for (int j = 0; j <= lb; j++)
		d[0][j] = j;

	for (int i = 1; i <= la; i++) {
		for (int j = 1; j <= lb; j++) {
			int cost
			    = (tolower((unsigned char) a[i - 1]) == tolower((unsigned char) b[j - 1])) ? 0 : 1;
			int del = d[i - 1][j] + 1;
			int ins = d[i][j - 1] + 1;
			int sub = d[i - 1][j - 1] + cost;
			d[i][j] = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
		}
	}
	return d[la][lb];
}

void arg_error_unknown_option(const char        *program,
                              const char        *option,
                              const char *const *known,
                              int                known_count)
{
	detect_color();
	fprintf(stderr, "%s%s%s: %serror:%s unknown option: %s%s%s\n",
	        C_BOLD, program, C_RESET,
	        C_BOLD_RED, C_RESET,
	        C_RED, option, C_RESET);

	/* Find closest match */
	int best_dist  = 999;
	int best_index = -1;

	for (int i = 0; i < known_count; i++) {
		int d = levenshtein(option, known[i]);
		if (d < best_dist) {
			best_dist  = d;
			best_index = i;
		}
	}

	if (best_index >= 0 && best_dist <= 3) {
		fprintf(stderr, "%sDid you mean:%s %s%s%s\n",
		        C_DIM, C_RESET, C_RED, known[best_index], C_RESET);
	}
}

void arg_error_missing_value(const char *program, const char *option)
{
	detect_color();
	fprintf(stderr, "%s%s%s: %serror:%s option '%s' requires a value\n",
	        C_BOLD, program, C_RESET,
	        C_BOLD_RED, C_RESET, option);
}

void arg_error_missing_argument(const char *program, const char *arg_name)
{
	detect_color();
	fprintf(stderr, "%s%s%s: %serror:%s %smissing required argument: %s%s%s\n",
	        C_BOLD, program, C_RESET,
	        C_BOLD_RED, C_RESET,
	        C_RED, arg_name, C_RED, C_RESET);
}

void arg_error_unknown_command(const char        *program,
                               const char        *command,
                               const char *const *known,
                               int                known_count)
{
	detect_color();
	fprintf(stderr, "%s%s%s: %serror:%s unknown command: %s%s%s\n",
	        C_BOLD, program, C_RESET,
	        C_BOLD_RED, C_RESET,
	        C_RED, command, C_RESET);

	int best_dist  = 999;
	int best_index = -1;

	for (int i = 0; i < known_count; i++) {
		int d = levenshtein(command, known[i]);
		if (d < best_dist) {
			best_dist  = d;
			best_index = i;
		}
	}

	if (best_index >= 0 && best_dist <= 3) {
		fprintf(stderr, "%sDid you mean:%s %s%s%s\n",
		        C_DIM, C_RESET, C_RED, known[best_index], C_RESET);
	}
}
