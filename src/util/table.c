/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * table.c — Tabular output formatter implementation
 */

#include "table.h"

#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_ROW_CAP 16

static const char PADDING[] = "                                        "; /* 40 spaces */
static const char DASHES[]  = "----------------------------------------"; /* 40 dashes */

/* ── ANSI helpers ────────────────────────────────────────────────────────────── */

/* Returns the number of visible characters (skipping ANSI escape sequences). */
static size_t visible_width(const char *s)
{
	if (!s)
		return 0;

	size_t      width = 0;
	const char *p     = s;

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

/* ── Table creation ──────────────────────────────────────────────────────────── */

static int table_ensure_capacity(Table *table)
{
	if (table->row_count < table->row_capacity)
		return 0;
	size_t    new_cap = table->row_capacity ? table->row_capacity * 2 : INITIAL_ROW_CAP;
	TableRow *tmp     = realloc(table->rows, new_cap * sizeof(TableRow));
	if (!tmp)
		return -1;
	table->rows         = tmp;
	table->row_capacity = new_cap;
	return 0;
}

Table *table_create(int col_count, const char **headers)
{
	LOG_TRACE("table_create(%d cols)", col_count);

	/* Clamp to maximum supported columns */
	if (col_count > 8)
		col_count = 8;
	if (col_count < 0)
		col_count = 0;

	Table *t = calloc(1, sizeof(Table));
	if (!t)
		return NULL;

	t->col_count   = col_count;
	t->show_header = (headers != NULL);
	t->use_color   = isatty(fileno(stdout)); /* default: auto-detect */

	if (headers) {
		for (int i = 0; i < col_count && i < 8; i++)
			t->headers[i] = headers[i];
	}

	return t;
}

int table_add_row(Table *table, ...)
{
	if (!table || table->col_count == 0)
		return -1;

	if (table_ensure_capacity(table) != 0)
		return -1;

	TableRow *row = &table->rows[table->row_count];
	row->count    = table->col_count;

	va_list ap;
	va_start(ap, table);
	for (int i = 0; i < table->col_count && i < 8; i++) {
		const char *cell = va_arg(ap, const char *);
		row->cells[i]    = cell ? strdup(cell) : strdup("");
		if (!row->cells[i]) {
			va_end(ap);
			return -1;
		}
		row->widths[i] = visible_width(row->cells[i]);
	}
	va_end(ap);

	table->row_count++;
	return 0;
}

int table_add_row_raw(Table *table, const char **cells, int count)
{
	if (!table || !cells || count != table->col_count)
		return -1;

	if (table_ensure_capacity(table) != 0)
		return -1;

	TableRow *row = &table->rows[table->row_count];
	row->count    = table->col_count;

	for (int i = 0; i < table->col_count && i < 8; i++) {
		row->cells[i] = cells[i] ? strdup(cells[i]) : strdup("");
		if (!row->cells[i])
			return -1;
		row->widths[i] = visible_width(row->cells[i]);
	}

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

	LOG_TRACE("table_print: calculating column widths (%d cols, %zu rows)",
	          table->col_count,
	          table->row_count);

	/* Calculate max width per column — stack, no heap */
	size_t widths[8] = { 0 };

	/* Header widths */
	if (table->show_header) {
		for (int i = 0; i < table->col_count && i < 8; i++) {
			if (!table->headers[i])
				continue;
			size_t w = visible_width(table->headers[i]);
			if (w > widths[i])
				widths[i] = w;
		}
	}

	/* Row widths */
	for (size_t r = 0; r < table->row_count; r++) {
		for (int c = 0; c < table->rows[r].count && c < table->col_count; c++) {
			if (table->rows[r].widths[c] > widths[c])
				widths[c] = table->rows[r].widths[c];
		}
	}

	/* Print header */
	if (table->show_header) {
		LOG_TRACE("table_print: printing header");
		for (int i = 0; i < table->col_count; i++) {
			const char *hdr = table->headers[i];
			if (!hdr)
				hdr = "";
			size_t w = visible_width(hdr);

			if (table->use_color)
				fprintf(out, "\x1b[1m%s\x1b[0m", hdr);
			else
				fputs(hdr, out);

			/* Pad header to column width */
			if (i < table->col_count - 1) {
				size_t pad = widths[i] - w + 1;
				while (pad > 0) {
					size_t chunk = pad > sizeof(PADDING) - 1 ? sizeof(PADDING) - 1 : pad;
					fwrite(PADDING, 1, chunk, out);
					pad -= chunk;
				}
				fputc('|', out);
				fputc(' ', out);
			} else {
				fputc('\n', out);
			}
		}

		/* Print separator line */
		LOG_TRACE("table_print: printing separator line");
		for (int i = 0; i < table->col_count; i++) {
			size_t w = widths[i];
			while (w > 0) {
				size_t chunk = w > sizeof(DASHES) - 1 ? sizeof(DASHES) - 1 : w;
				fwrite(DASHES, 1, chunk, out);
				w -= chunk;
			}
			if (i < table->col_count - 1) {
				fputc(' ', out);
				fputc('+', out);
				fputc(' ', out);
			} else {
				fputc('\n', out);
			}
		}
	}

	/* Print rows */
	LOG_TRACE("table_print: printing %zu data rows", table->row_count);
	for (size_t r = 0; r < table->row_count; r++) {
		const TableRow *row = &table->rows[r];
		for (int c = 0; c < row->count && c < table->col_count; c++) {
			const char *cell = row->cells[c] ? row->cells[c] : "";

			fputs(cell, out);

			if (c < table->col_count - 1) {
				size_t padding = (widths[c] > row->widths[c]) ? widths[c] - row->widths[c] : 0;
				padding++; /* separator column */
				while (padding > 0) {
					size_t chunk = padding > sizeof(PADDING) - 1 ? sizeof(PADDING) - 1 : padding;
					fwrite(PADDING, 1, chunk, out);
					padding -= chunk;
				}
				fputc('|', out);
				fputc(' ', out);
			} else {
				fputc('\n', out);
			}
		}
	}

	LOG_TRACE("table_print: done");
}

void table_free(Table *table)
{
	if (!table)
		return;

	for (size_t r = 0; r < table->row_count; r++) {
		for (int c = 0; c < table->rows[r].count; c++)
			free(table->rows[r].cells[c]);
	}
	free(table->rows);

	/* headers are inline const char* — nothing to free */

	free(table);
}
