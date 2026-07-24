/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * stats.c — `gitm stats` command
 *
 * Prints a frequency summary of tags and groups across all registered repos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "cmd_util.h"
#include "config.h"
#include "log.h"
#include "table.h"

typedef struct {
	char   name[MAX_NAME_LEN];
	size_t count;
} FreqEntry;

typedef struct {
	FreqEntry items[FREQ_MAP_MAX];
	size_t    count;
	size_t    none_count;
} FreqMap;

static void freq_map_init(FreqMap *m)
{
	m->count      = 0;
	m->none_count = 0;
}

static void freq_map_free(FreqMap *m)
{
	(void) m;
}

static void freq_map_add(FreqMap *m, const char *name)
{
	if (!*name) {
		m->none_count++;
		return;
	}

	for (size_t i = 0; i < m->count; i++) {
		if (strcmp(m->items[i].name, name) == 0) {
			m->items[i].count++;
			return;
		}
	}

	if (m->count >= FREQ_MAP_MAX)
		return;

	strncpy(m->items[m->count].name, name, sizeof(m->items[0].name) - 1);
	m->items[m->count].name[sizeof(m->items[0].name) - 1] = '\0';
	m->items[m->count].count = 1;
	m->count++;
}

static void freq_map_incr_none(FreqMap *m)
{
	m->none_count++;
}

static int freq_cmp_desc(const void *a, const void *b)
{
	const FreqEntry *ea = a;
	const FreqEntry *eb = b;
	if (eb->count > ea->count) return  1;
	if (eb->count < ea->count) return -1;
	return strcmp(ea->name, eb->name);
}

static void freq_map_sort(FreqMap *m)
{
	qsort(m->items, m->count, sizeof(FreqEntry), freq_cmp_desc);
}

static void parse_tokens(FreqMap *m, const char *csv)
{
	if (!csv || *csv == '\0') {
		freq_map_incr_none(m);
		return;
	}

	const char *p = csv;
	while (*p) {
		while (*p == ',')
			p++;
		if (*p == '\0')
			break;

		const char *end = p;
		while (*end && *end != ',')
			end++;

		size_t len = (size_t) (end - p);
		char   buf[MAX_NAME_LEN];
		if (len >= sizeof(buf))
			len = sizeof(buf) - 1;
		memcpy(buf, p, len);
		buf[len] = '\0';

		freq_map_add(m, buf);
		p = end;
	}
}

static void print_plain(const char *title, const FreqMap *m, bool color)
{
	if (color)
		fprintf(stderr, "\n\x1b[1m%s\x1b[0m\n", title);
	else
		fprintf(stderr, "\n%s\n", title);

	for (size_t i = 0; i < m->count; i++) {
		if (color)
			fprintf(stderr, "  \x1b[33m%-20s\x1b[0m %zu\n",
			        m->items[i].name, m->items[i].count);
		else
			fprintf(stderr, "  %-20s %zu\n",
			        m->items[i].name, m->items[i].count);
	}

	if (m->none_count > 0) {
		if (color)
			fprintf(stderr, "  \x1b[33m%-20s\x1b[0m %zu\n",
			        "(none)", m->none_count);
		else
			fprintf(stderr, "  %-20s %zu\n",
			        "(none)", m->none_count);
	}
}

static Table *build_table(const char *col_header, const FreqMap *m, bool color)
{
	const char *headers[] = { col_header, "Repos" };
	Table *t = table_create(2, headers);
	table_set_color(t, color);

	for (size_t i = 0; i < m->count; i++) {
		char count_str[32];
		snprintf(count_str, sizeof(count_str), "%zu", m->items[i].count);
		const char *cells[] = { m->items[i].name, count_str };
		table_add_row_raw(t, cells, 2);
	}

	if (m->none_count > 0) {
		char count_str[32];
		snprintf(count_str, sizeof(count_str), "%zu", m->none_count);
		const char *cells[] = { "(none)", count_str };
		table_add_row_raw(t, cells, 2);
	}

	return t;
}

int cmd_stats(const ArgParseResult *result)
{
	(void) result;

	bool   color = log_use_color();
	char  *config_path = NULL;
	GitConfig cfg = { 0 };

	if (cmd_load_config(&cfg, &config_path) != 0)
		return 1;

	if (cfg.count == 0) {
		fprintf(stderr, MSG_NO_REPOS);
		config_free(&cfg);
		free(config_path);
		return 0;
	}

	LOG_TRACE("building tag/group frequency maps for %zu entries", cfg.count);

	FreqMap tags, groups;
	freq_map_init(&tags);
	freq_map_init(&groups);

	for (size_t i = 0; i < cfg.count; i++) {
		parse_tokens(&tags,   cfg.entries[i].tags);
		parse_tokens(&groups, cfg.entries[i].groups);
	}

	freq_map_sort(&tags);
	freq_map_sort(&groups);

	if (g_table_mode) {
		Table *t_tags = build_table("Tag", &tags, color);
		table_print(t_tags, stdout);
		table_free(t_tags);

		fputc('\n', stdout);

		Table *t_groups = build_table("Group", &groups, color);
		table_print(t_groups, stdout);
		table_free(t_groups);
	} else {
		print_plain("Tags:",   &tags,   color);
		print_plain("Groups:", &groups, color);
		fputc('\n', stderr);
	}

	freq_map_free(&tags);
	freq_map_free(&groups);
	config_free(&cfg);
	free(config_path);
	return 0;
}

void cmd_register_stats(ArgParser *parser)
{
	ArgCommand *cmd = argparse_add_command(parser,
	                                       "stats",
	                                       "Show tag and group frequency summary",
	                                       cmd_stats);
	const char *aliases[] = { "ts" };
	argparse_command_set_aliases(cmd, aliases, 1);
	cmd_register_table_flag(cmd);
}
