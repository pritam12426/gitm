/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * help.c — Automatic help generation with colour
 *
 * Produces formatted, coloured help output from the command tree.
 * Colour scheme inspired by Rust's clap:
 *   - Section headers: bold
 *   - Command names: bold cyan
 *   - Option flags: bold green
 *   - Metavars: dim
 *   - Descriptions: dim
 *   - Usage keywords: bold
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "argparse.h"

/* ── Internal ANSI codes ──────────────────────────────────────────────────── */

static int g_use_color = -1; /* -1 = not detected yet */

static void detect_color(void)
{
	if (g_use_color >= 0)
		return;
	g_use_color = isatty(fileno(stderr)) ? 1 : 0;
}

#define C_RESET      (g_use_color ? "\x1b[0m" : "")
#define C_BOLD       (g_use_color ? "\x1b[1m" : "")
#define C_DIM        (g_use_color ? "\x1b[2m" : "")
#define C_CYAN       (g_use_color ? "\x1b[36m" : "")
#define C_BOLD_CYAN  (g_use_color ? "\x1b[1;36m" : "")
#define C_BOLD_GREEN (g_use_color ? "\x1b[1;32m" : "")

/* ── Helpers ───────────────────────────────────────────────────────────────── */

static size_t visible_len(const char *s)
{
	size_t      vis = 0;
	const char *p   = s;

	while (*p) {
		if (*p == '\x1b' && *(p + 1) == '[') {
			p += 2;
			while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')))
				p++;
			if (*p) p++;
			continue;
		}
		vis++;
		p++;
	}
	return vis;
}

static void print_padded(FILE *out, const char *s, size_t width)
{
	size_t vis = visible_len(s);
	fprintf(out, "%s", s);
	if (vis < width) {
		size_t pad = width - vis;
		for (size_t i = 0; i < pad; i++)
			fputc(' ', out);
	}
}

static void print_option(FILE *out, const ArgOption *opt)
{
	char   flags[128] = { 0 };
	size_t len        = 0;

	if (opt->short_name)
		len += (size_t) snprintf(flags + len, sizeof(flags) - len,
		                         "%s-%c%s, ", C_BOLD_GREEN, opt->short_name, C_RESET);
	else
		len += (size_t) snprintf(flags + len, sizeof(flags) - len, "    ");

	if (opt->long_name)
		len += (size_t) snprintf(flags + len, sizeof(flags) - len,
		                         "%s--%s%s", C_BOLD_GREEN, opt->long_name, C_RESET);

	if (opt->metavar)
		len += (size_t) snprintf(flags + len, sizeof(flags) - len,
		                         "=%s%s%s", C_DIM, opt->metavar, C_RESET);

	fprintf(out, "  ");
	print_padded(out, flags, 25);
	fprintf(out, " %s%s%s", C_DIM, opt->description ? opt->description : "", C_RESET);

	if (opt->env_var)
		fprintf(out, " %s[$%s]%s", C_DIM, opt->env_var, C_RESET);
	if (opt->required)
		fprintf(out, " %s(required)%s", C_DIM, C_RESET);
	if (opt->default_val)
		fprintf(out, " %s[default: %s]%s", C_DIM, opt->default_val, C_RESET);
	if (opt->exclusive_group > 0)
		fprintf(out, " %s[group: %d]%s", C_DIM, opt->exclusive_group, C_RESET);

	fprintf(out, "\n");
}

static void print_builtin_option(FILE       *out,
                                 const char *short_flag,
                                 const char *long_flag,
                                 const char *desc)
{
	char   flags[128] = { 0 };
	size_t len        = 0;

	if (short_flag)
		len += (size_t) snprintf(flags + len, sizeof(flags) - len,
		                         "%s%s%s, ", C_BOLD_GREEN, short_flag, C_RESET);
	else
		len += (size_t) snprintf(flags + len, sizeof(flags) - len, "    ");

	len += (size_t) snprintf(flags + len, sizeof(flags) - len,
	                         "%s%s%s", C_BOLD_GREEN, long_flag, C_RESET);

	fprintf(out, "  ");
	print_padded(out, flags, 25);
	fprintf(out, " %s%s%s\n", C_DIM, desc, C_RESET);
}

/* ── Command tree printing ─────────────────────────────────────────────────── */

static void print_commands_flat(const ArgParser *parser, const ArgCommand *cmd, int depth)
{
	int indent = depth * 2;

	for (int i = 0; i < cmd->subcommand_count; i++) {
		ArgCommand *sub = cmd->subcommands[i];

		fprintf(stderr, "%*s", indent, "");
		fprintf(stderr, "  ");
		print_padded(stderr, sub->name, 15);
		if (sub->alias_count > 0) {
			fprintf(stderr, " %s(", C_BOLD_CYAN);
			for (int j = 0; j < sub->alias_count; j++) {
				if (j > 0) fprintf(stderr, ", ");
				fputs(sub->aliases[j], stderr);
			}
			fprintf(stderr, ")%s", C_RESET);
		}
		fprintf(stderr, " %s%s%s\n",
		        C_DIM,
		        sub->description ? sub->description : "",
		        C_RESET);

		/* Recurse for deeper nesting */
		print_commands_flat(parser, (const ArgCommand *) sub, depth + 1);
	}
}

/* ── Public API ────────────────────────────────────────────────────────────── */

void argparse_help(const ArgParser *parser, const ArgCommand *cmd)
{
	detect_color();

	/* Root command — show global help with commands list */
	if (cmd == NULL || cmd == &parser->root) {
		fprintf(stderr, "%sUsage:%s %s%s [OPTIONS] COMMAND [ARGS]%s\n",
		        C_BOLD, C_RESET, C_BOLD, parser->prog_name, C_RESET);

		if (parser->description)
			fprintf(stderr, "%s%s%s — %s%s%s\n\n",
			        C_BOLD, parser->prog_name, C_RESET,
			        C_DIM, parser->description, C_RESET);

		if (parser->command_count > 0) {
			fprintf(stderr, "%sCommands:%s\n", C_BOLD, C_RESET);
			for (int i = 0; i < parser->command_count; i++) {
				const ArgCommand *c = &parser->commands[i];

				fprintf(stderr, "  ");
				print_padded(stderr, c->name, 15);
				if (c->alias_count > 0) {
					fprintf(stderr, " %s(", C_BOLD_CYAN);
					for (int j = 0; j < c->alias_count; j++) {
						if (j > 0) fprintf(stderr, ", ");
						fputs(c->aliases[j], stderr);
					}
					fprintf(stderr, ")%s", C_RESET);
				}
				fprintf(stderr, " %s%s%s\n",
				        C_DIM,
				        c->description ? c->description : "",
				        C_RESET);

				/* Show nested subcommands */
				if (c->subcommand_count > 0)
					print_commands_flat(parser, (const ArgCommand *) c, 1);
			}
			fprintf(stderr, "\n");
		}

		/* Global options */
		if (parser->root.option_count > 0) {
			fprintf(stderr, "%sOptions:%s\n", C_BOLD, C_RESET);
			for (int i = 0; i < parser->root.option_count; i++)
				print_option(stderr, &parser->root.options[i]);
		}

		print_builtin_option(stderr, "-h", "--help", "Show this help message");
		print_builtin_option(stderr, "-v", "--version", "Show version");
		print_builtin_option(stderr, NULL, "--shell-completion=SHELL", "Output shell completion script (bash, zsh, fish)");
		fprintf(stderr, "\n");

		/* Footer */
		fputs(C_DIM, stderr);
		if (parser->bug_url)
			fprintf(stderr, "Report bugs to: %s\n", parser->bug_url);
		if (parser->author)
			fprintf(stderr, "%s\n", parser->author);
		fputs(C_RESET, stderr);

		return;
	}

	/* Subcommand help — build full usage name */
	fprintf(stderr, "%sUsage:%s %s%s", C_BOLD, C_RESET, C_BOLD, parser->prog_name);

	/* Walk parent chain for nested commands */
	const ArgCommand *chain[32];
	int                chain_len = 0;
	const ArgCommand  *p = cmd;

	while (p && p->parent && p->parent->name && p->parent->name[0] != '\0') {
		chain[chain_len++] = p;
		p = p->parent;
	}

	/* Print in reverse order */
	for (int i = chain_len - 1; i >= 0; i--)
		fprintf(stderr, " %s%s%s", C_BOLD, chain[i]->name, C_RESET);

	fprintf(stderr, " %s%s%s", C_BOLD, cmd->name, C_RESET);

	for (int i = 0; i < cmd->positional_count; i++)
		fprintf(stderr, " %s%s%s", C_DIM, cmd->positionals[i], C_RESET);

	if (cmd->option_count > 0)
		fprintf(stderr, " %s[OPTIONS]%s", C_DIM, C_RESET);

	fprintf(stderr, "\n\n");

	if (cmd->description)
		fprintf(stderr, "%s%s%s\n\n", C_DIM, cmd->description, C_RESET);

	if (cmd->option_count > 0) {
		fprintf(stderr, "%sOptions:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < cmd->option_count; i++)
			print_option(stderr, &cmd->options[i]);
	}

	/* Show subcommands if any */
	if (cmd->subcommand_count > 0) {
		fprintf(stderr, "\n%sSubcommands:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < cmd->subcommand_count; i++) {
			ArgCommand *sub = cmd->subcommands[i];
			fprintf(stderr, "  ");
			print_padded(stderr, sub->name, 15);
			if (sub->alias_count > 0) {
				fprintf(stderr, " %s(", C_BOLD_CYAN);
				for (int j = 0; j < sub->alias_count; j++) {
					if (j > 0) fprintf(stderr, ", ");
					fputs(sub->aliases[j], stderr);
				}
				fprintf(stderr, ")%s", C_RESET);
			}
			fprintf(stderr, " %s%s%s\n",
			        C_DIM,
			        sub->description ? sub->description : "",
			        C_RESET);
		}
	}

	print_builtin_option(stderr, "-h", "--help", "Show this help message");
	print_builtin_option(stderr, NULL, "--shell-completion=SHELL", "Output shell completion script (bash, zsh, fish)");
	fprintf(stderr, "\n");
}
