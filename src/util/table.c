/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * table.c — Tabular output formatter implementation
 */

#include "table.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_ROW_CAP 16

/* ── ANSI helpers ────────────────────────────────────────────────────────────── */

/* Returns the number of visible characters (skipping ANSI escape sequences). */
static size_t visible_width(const char *s)
{
	if (!s)
		return 0;

	size_t width = 0;
	const char *p = s;

	while (*p) {
		if (*p == '\x1b' && *(p + 1) == '[') {
			/* Skip ANSI escape sequence: \x1b[ ... letter */
			p += 2;
			while (*p && ((*p >= '0' && *p <= '?') || *p == ';' || *p == ' '))
				p++;
			if (*p)
				p++; /* skip the final letter (e.g. 'm') */
		} else {
			width++;
			p++;
		}
	}

	return width;
}

/* Pad string to target width with spaces. Writes to out. */
static void pad_right(FILE *out, const char *str, size_t target_width, bool is_last)
{
	size_t w = visible_width(str);
	fputs(str, out);

	if (!is_last) {
		size_t padding = (target_width > w) ? target_width - w : 0;
		for (size_t i = 0; i < padding + 1; i++)
			fputc(' ', out);
		fputc('|', out);
		fputc(' ', out);
	} else {
		fputc('\n', out);
	}
}

/* ── Table creation ──────────────────────────────────────────────────────────── */

Table *table_create(int col_count, const char **headers)
{
	Table *t = calloc(1, sizeof(Table));
	if (!t)
		return NULL;

	t->col_count   = col_count;
	t->headers     = headers;
	t->show_header = (headers != NULL);
	t->use_color   = isatty(fileno(stderr)); /* default: auto-detect */

	return t;
}

int table_add_row(Table *table, ...)
{
	if (!table || table->col_count == 0)
		return -1;

	if (table->row_count >= table->row_capacity) {
		size_t     new_cap = table->row_capacity ? table->row_capacity * 2 : INITIAL_ROW_CAP;
		TableRow *tmp     = realloc(table->rows, new_cap * sizeof(TableRow));
		if (!tmp)
			return -1;
		table->rows        = tmp;
		table->row_capacity = new_cap;
	}

	TableRow *row = &table->rows[table->row_count];
	row->cells = calloc((size_t) table->col_count, sizeof(char *));
	if (!row->cells)
		return -1;
	row->count = table->col_count;

	va_list ap;
	va_start(ap, table);
	for (int i = 0; i < table->col_count; i++) {
		const char *cell = va_arg(ap, const char *);
		row->cells[i] = cell ? strdup(cell) : strdup("");
	}
	va_end(ap);

	table->row_count++;
	return 0;
}

int table_add_row_raw(Table *table, const char **cells, int count)
{
	if (!table || !cells || count != table->col_count)
		return -1;

	if (table->row_count >= table->row_capacity) {
		size_t     new_cap = table->row_capacity ? table->row_capacity * 2 : INITIAL_ROW_CAP;
		TableRow *tmp     = realloc(table->rows, new_cap * sizeof(TableRow));
		if (!tmp)
			return -1;
		table->rows        = tmp;
		table->row_capacity = new_cap;
	}

	TableRow *row = &table->rows[table->row_count];
	row->cells = calloc((size_t) table->col_count, sizeof(char *));
	if (!row->cells)
		return -1;
	row->count = table->col_count;

	for (int i = 0; i < table->col_count; i++)
		row->cells[i] = cells[i] ? strdup(cells[i]) : strdup("");

	table->row_count++;
	return 0;
}

void table_set_header(Table *table, bool show)
{
	if (table)
		table->show_header = show;
}

void table_set_color(Table *table, bool use_color)
{
	if (table)
		table->use_color = use_color;
}

/* ── Printing ────────────────────────────────────────────────────────────────── */

void table_print(const Table *table, FILE *out)
{
	if (!table || table->col_count == 0)
		return;

	/* Calculate max width per column */
	size_t *widths = calloc((size_t) table->col_count, sizeof(size_t));
	if (!widths)
		return;

	/* Header widths */
	if (table->show_header && table->headers) {
		for (int i = 0; i < table->col_count; i++) {
			size_t w = visible_width(table->headers[i]);
			if (w > widths[i])
				widths[i] = w;
		}
	}

	/* Row widths */
	for (size_t r = 0; r < table->row_count; r++) {
		for (int c = 0; c < table->rows[r].count && c < table->col_count; c++) {
			size_t w = visible_width(table->rows[r].cells[c]);
			if (w > widths[c])
				widths[c] = w;
		}
	}

	/* Print header */
	if (table->show_header && table->headers) {
		for (int i = 0; i < table->col_count; i++) {
			const char *hdr = table->headers[i];
			size_t      w   = visible_width(hdr);

			if (table->use_color)
				fprintf(out, "\x1b[1m%s\x1b[0m", hdr);
			else
				fputs(hdr, out);

			/* Pad header to column width */
			if (i < table->col_count - 1) {
				for (size_t p = 0; p < widths[i] - w + 1; p++)
					fputc(' ', out);
				fputc('|', out);
				fputc(' ', out);
			} else {
				fputc('\n', out);
			}
		}

		/* Print separator line */
		for (int i = 0; i < table->col_count; i++) {
			for (size_t j = 0; j < widths[i]; j++)
				fputc('-', out);
			if (i < table->col_count - 1) {
				fputc('+', out);
				fputc(' ', out);
			} else {
				fputc('\n', out);
			}
		}
	}

	/* Print rows */
	for (size_t r = 0; r < table->row_count; r++) {
		const TableRow *row = &table->rows[r];
		for (int c = 0; c < row->count && c < table->col_count; c++) {
			pad_right(out, row->cells[c], widths[c], c == table->col_count - 1);
		}
	}

	free(widths);
}

void table_free(Table *table)
{
	if (!table)
		return;

	for (size_t r = 0; r < table->row_count; r++) {
		for (int c = 0; c < table->rows[r].count; c++)
			free(table->rows[r].cells[c]);
		free(table->rows[r].cells);
	}
	free(table->rows);
	free(table);
}
