/*
 * error.c — Error messages with suggestions
 *
 * Provides helpful error output when parsing fails.
 * Uses simple string similarity for "did you mean?" suggestions.
 */

#include "error.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
			int cost = (tolower((unsigned char) a[i - 1]) == tolower((unsigned char) b[j - 1])) ? 0
			                                                                                    : 1;
			int del  = d[i - 1][j] + 1;
			int ins  = d[i][j - 1] + 1;
			int sub  = d[i - 1][j - 1] + cost;
			d[i][j]  = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
		}
	}
	return d[la][lb];
}

void arg_error_unknown_option(const char        *program,
                              const char        *option,
                              const char *const *known,
                              int                known_count)
{
	fprintf(stderr, "%s: unknown option: %s\n", program, option);

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
		fprintf(stderr, "Did you mean: %s\n", known[best_index]);
	}

	fprintf(stderr, "Try '%s --help' for usage information.\n", program);
}

void arg_error_missing_value(const char *program, const char *option)
{
	fprintf(stderr, "%s: option '%s' requires a value\n", program, option);
	fprintf(stderr, "Try '%s --help' for usage information.\n", program);
}

void arg_error_missing_argument(const char *program, const char *arg_name)
{
	fprintf(stderr, "%s: missing required argument: %s\n", program, arg_name);
	fprintf(stderr, "Try '%s --help' for usage information.\n", program);
}

void arg_error_unknown_command(const char        *program,
                               const char        *command,
                               const char *const *known,
                               int                known_count)
{
	fprintf(stderr, "%s: unknown command: %s\n", program, command);

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
		fprintf(stderr, "Did you mean: %s\n", known[best_index]);
	}

	fprintf(stderr, "Try '%s --help' for usage information.\n", program);
}
