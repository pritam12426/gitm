/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * table.h — Tabular output formatter
 *
 * Provides auto-width, pipe-separated columns with optional headers.
 * ANSI escape sequences are handled correctly for width calculation.
 *
 * Usage:
 *   Table *t = table_create(3, (const char *[]){"Name", "Path", "Status"});
 *   table_add_row(t, "my-repo", "/Users/dev/repo", "clean");
 *   table_add_row_colored(t, "\x1b[32mok\x1b[0m", "my-repo", "/path");
 *   table_print(t, stdout);
 *   table_free(t);
 */

#ifndef _TABLE_H_
#define _TABLE_H_


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	char **cells;      /* array of cell strings (may contain ANSI codes) */
	int    count;      /* number of cells in this row */
} TableRow;

typedef struct {
	TableRow *rows;
	size_t    row_count;
	size_t    row_capacity;

	const char **headers;
	int          col_count;

	bool show_header;
	bool use_color;
} Table;

/*
 * Create a table with the given column headers.
 * If headers is NULL, col_count must be specified and no header row is printed.
 */
Table *table_create(int col_count, const char **headers);

/*
 * Add a row with plain text cells.
 */
int table_add_row(Table *table, ...);

/*
 * Add a row with pre-formatted (possibly ANSI-colored) cells.
 */
int table_add_row_raw(Table *table, const char **cells, int count);

/*
 * Enable or disable the header row (enabled by default if headers were given).
 */
void table_set_header(Table *table, bool show);

/*
 * Enable or disable ANSI color in output (auto-detected from terminal).
 */
void table_set_color(Table *table, bool use_color);

/*
 * Print the table to the given stream.
 */
void table_print(const Table *table, FILE *out);

/*
 * Free all memory associated with the table.
 */
void table_free(Table *table);


#ifdef __cplusplus
}
#endif


#endif /* _TABLE_H_ */
