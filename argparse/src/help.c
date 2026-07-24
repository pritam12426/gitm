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
 *
 * Column widths for the flag/description gutter are computed per
 * section from the actual content being printed, instead of a fixed
 * number — so alignment stays clean whether names are short or long,
 * and at any subcommand nesting depth.
 */

#define _POSIX_C_SOURCE 200809L /* for fileno() under strict -std=c17 */

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

#define GUTTER_MIN 12
#define GUTTER_MAX 36
#define GUTTER_PAD 2  /* spaces between the widest flag string and the description */

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

static size_t clamp_width(size_t w)
{
	if (w < GUTTER_MIN) return GUTTER_MIN;
	if (w > GUTTER_MAX) return GUTTER_MAX;
	return w;
}

/* Builds the coloured "-x, --flag=METAVAR" text for a real option. */
#define APPEND_INTO(buf, bufsz, lenvar, ...) \
	do { \
		if ((lenvar) < (bufsz)) { \
			int n = snprintf((buf) + (lenvar), (bufsz) - (lenvar), __VA_ARGS__); \
			if (n > 0) (lenvar) += (size_t) n; \
			if ((lenvar) > (bufsz)) (lenvar) = (bufsz); /* clamp on truncation */ \
		} \
	} while (0)

static void build_option_flags(char *buf, size_t bufsz, const ArgOption *opt)
{
	size_t len = 0;
	buf[0] = '\0';

	if (opt->short_name)
		APPEND_INTO(buf, bufsz, len, "%s-%c%s, ", C_BOLD_GREEN, opt->short_name, C_RESET);
	else
		APPEND_INTO(buf, bufsz, len, "    ");

	if (opt->long_name)
		APPEND_INTO(buf, bufsz, len, "%s--%s%s", C_BOLD_GREEN, opt->long_name, C_RESET);

	if (opt->metavar)
		APPEND_INTO(buf, bufsz, len, "=%s%s%s", C_DIM, opt->metavar, C_RESET);
}

/* Builds the coloured flag text for a built-in option (-h/--help etc).
 * `metavar`, if given, is coloured the same way a real option's metavar
 * is: the "=" itself is left uncoloured (plain terminal default) and
 * only the value after it is dim — matching e.g. --log-level=LEVEL. */
static void build_builtin_flags(char *buf, size_t bufsz,
                                 const char *short_flag, const char *long_flag,
                                 const char *metavar)
{
	size_t len = 0;
	buf[0] = '\0';

	if (short_flag)
		APPEND_INTO(buf, bufsz, len, "%s%s%s, ", C_BOLD_GREEN, short_flag, C_RESET);
	else
		APPEND_INTO(buf, bufsz, len, "    ");

	APPEND_INTO(buf, bufsz, len, "%s%s%s", C_BOLD_GREEN, long_flag, C_RESET);

	if (metavar)
		APPEND_INTO(buf, bufsz, len, "=%s%s%s", C_DIM, metavar, C_RESET);
}

/* Builds "name (alias1, alias2)" for a command/subcommand listing. */
static void build_command_label(char *buf, size_t bufsz,
                                 const char *name,
                                 const char *const *aliases, int alias_count)
{
	size_t len = 0;
	buf[0] = '\0';

	APPEND_INTO(buf, bufsz, len, "%s", name);

	if (alias_count > 0) {
		APPEND_INTO(buf, bufsz, len, " %s(", C_BOLD_CYAN);
		for (int j = 0; j < alias_count; j++) {
			if (j > 0)
				APPEND_INTO(buf, bufsz, len, ", ");
			APPEND_INTO(buf, bufsz, len, "%s", aliases[j]);
		}
		APPEND_INTO(buf, bufsz, len, ")%s", C_RESET);
	}
}

static void print_option(FILE *out, const ArgOption *opt, size_t width)
{
	char flags[160];
	build_option_flags(flags, sizeof(flags), opt);

	fprintf(out, "  ");
	print_padded(out, flags, width);
	fprintf(out, "  %s%s%s", C_DIM, opt->description ? opt->description : "", C_RESET);

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
                                 const char *metavar,
                                 const char *desc,
                                 size_t      width)
{
	char flags[160];
	build_builtin_flags(flags, sizeof(flags), short_flag, long_flag, metavar);

	fprintf(out, "  ");
	print_padded(out, flags, width);
	fprintf(out, "  %s%s%s\n", C_DIM, desc, C_RESET);
}

/* Computes the gutter width for a list of options, also accounting for
 * the built-in options that will be printed alongside them so the
 * whole block lines up. `with_shell_completion` should match whether
 * --shell-completion=SHELL is actually going to be printed at this
 * call site (root only — see argparse_help). */
static size_t options_gutter_width(const ArgOption *opts, int count, bool with_shell_completion)
{
	size_t max = 0;
	char   buf[160];

	for (int i = 0; i < count; i++) {
		build_option_flags(buf, sizeof(buf), &opts[i]);
		size_t w = visible_len(buf);
		if (w > max) max = w;
	}

	build_builtin_flags(buf, sizeof(buf), "-h", "--help", NULL);
	if (visible_len(buf) > max) max = visible_len(buf);

	build_builtin_flags(buf, sizeof(buf), "-v", "--version", NULL);
	if (visible_len(buf) > max) max = visible_len(buf);

	if (with_shell_completion) {
		build_builtin_flags(buf, sizeof(buf), "-S", "--shell-completion", "SHELL");
		if (visible_len(buf) > max) max = visible_len(buf);
	}

	return clamp_width(max + GUTTER_PAD);
}

static size_t commands_gutter_width(const ArgCommand *const *cmds, int count)
{
	size_t max = 0;
	char   buf[160];

	for (int i = 0; i < count; i++) {
		build_command_label(buf, sizeof(buf), cmds[i]->name,
		                     cmds[i]->aliases, cmds[i]->alias_count);
		size_t w = visible_len(buf);
		if (w > max) max = w;
	}
	return clamp_width(max + GUTTER_PAD);
}

/* ── Command tree printing ─────────────────────────────────────────────────── */

static void print_commands_flat(const ArgCommand *cmd, int depth)
{
	int indent = depth * 2;

	if (cmd->subcommand_count == 0)
		return;

	const ArgCommand *ptrs[ARGPARSE_MAX_COMMANDS];
	for (int i = 0; i < cmd->subcommand_count; i++)
		ptrs[i] = cmd->subcommands[i];
	size_t width = commands_gutter_width(ptrs, cmd->subcommand_count);

	for (int i = 0; i < cmd->subcommand_count; i++) {
		ArgCommand *sub = cmd->subcommands[i];
		char        label[160];
		build_command_label(label, sizeof(label), sub->name, sub->aliases, sub->alias_count);

		fprintf(stderr, "%*s  ", indent, "");
		print_padded(stderr, label, width);
		fprintf(stderr, "  %s%s%s\n",
		        C_DIM, sub->description ? sub->description : "", C_RESET);

		print_commands_flat(sub, depth + 1);
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
			fprintf(stderr, "\n%s%s\n", C_DIM, parser->description);
		fprintf(stderr, "%s\n", C_RESET);

		if (parser->command_count > 0) {
			const ArgCommand *ptrs[ARGPARSE_MAX_COMMANDS];
			for (int i = 0; i < parser->command_count; i++)
				ptrs[i] = &parser->commands[i];
			size_t cmd_width = commands_gutter_width(ptrs, parser->command_count);

			fprintf(stderr, "%sCommands:%s\n", C_BOLD, C_RESET);
			for (int i = 0; i < parser->command_count; i++) {
				const ArgCommand *c = &parser->commands[i];
				char              label[160];
				build_command_label(label, sizeof(label), c->name, c->aliases, c->alias_count);

				fprintf(stderr, "  ");
				print_padded(stderr, label, cmd_width);
				fprintf(stderr, "  %s%s%s\n",
				        C_DIM, c->description ? c->description : "", C_RESET);

				if (c->subcommand_count > 0)
					print_commands_flat(c, 1);
			}
			fprintf(stderr, "\n");
		}

		/* Global + built-in options, aligned in one block */
		size_t opt_width = options_gutter_width(parser->root.options,
		                                         parser->root.option_count, true);

		fprintf(stderr, "%sOptions:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < parser->root.option_count; i++)
			print_option(stderr, &parser->root.options[i], opt_width);

		print_builtin_option(stderr, "-h", "--help", NULL, "Show this help message", opt_width);
		print_builtin_option(stderr, "-v", "--version", NULL, "Show version", opt_width);
		print_builtin_option(stderr, "-S", "--shell-completion", "SHELL",
		                     "Output shell completion script (bash, zsh, fish)", opt_width);
		fprintf(stderr, "\n");

		if (parser->command_count > 0)
			fprintf(stderr, "%sSee '%s COMMAND --help' for more information on a command.%s\n\n",
			        C_DIM, parser->prog_name, C_RESET);

		/* Footer */
		if (parser->bug_url || parser->author) {
			fputs(C_DIM, stderr);
			if (parser->bug_url)
				fprintf(stderr, "Report bugs to: %s\n", parser->bug_url);
			if (parser->author)
				fprintf(stderr, "%s\n", parser->author);
			fputs(C_RESET, stderr);
		}

		return;
	}

	/* Subcommand help — build full usage name by walking the parent
	 * chain (root excluded, cmd's own name printed once at the end). */
	const ArgCommand *chain[ARGPARSE_MAX_CHAIN_DEPTH];
	int                chain_len = 0;
	for (const ArgCommand *p = cmd->parent; p && p->name && p->name[0] != '\0'; p = p->parent) {
		if (chain_len < ARGPARSE_MAX_CHAIN_DEPTH)
			chain[chain_len++] = p;
	}

	fprintf(stderr, "%sUsage:%s %s%s", C_BOLD, C_RESET, C_BOLD, parser->prog_name);
	for (int i = chain_len - 1; i >= 0; i--)
		fprintf(stderr, " %s", chain[i]->name);
	fprintf(stderr, " %s%s", cmd->name, C_RESET);

	for (int i = 0; i < cmd->positional_count; i++)
		fprintf(stderr, " %s%s%s", C_DIM, cmd->positionals[i], C_RESET);

	if (cmd->option_count > 0)
		fprintf(stderr, " %s[OPTIONS]%s", C_DIM, C_RESET);
	if (cmd->subcommand_count > 0)
		fprintf(stderr, " %s[SUBCOMMAND]%s", C_DIM, C_RESET);

	fprintf(stderr, "\n");

	if (cmd->description)
		fprintf(stderr, "\n%s%s%s\n", C_DIM, cmd->description, C_RESET);

	size_t opt_width = options_gutter_width(cmd->options, cmd->option_count, false);

	if (cmd->option_count > 0) {
		fprintf(stderr, "\n%sOptions:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < cmd->option_count; i++)
			print_option(stderr, &cmd->options[i], opt_width);
	}

	if (cmd->subcommand_count > 0) {
		const ArgCommand *ptrs[ARGPARSE_MAX_COMMANDS];
		for (int i = 0; i < cmd->subcommand_count; i++)
			ptrs[i] = cmd->subcommands[i];
		size_t sub_width = commands_gutter_width(ptrs, cmd->subcommand_count);

		fprintf(stderr, "\n%sSubcommands:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < cmd->subcommand_count; i++) {
			ArgCommand *sub = cmd->subcommands[i];
			char        label[160];
			build_command_label(label, sizeof(label), sub->name, sub->aliases, sub->alias_count);

			fprintf(stderr, "  ");
			print_padded(stderr, label, sub_width);
			fprintf(stderr, "  %s%s%s\n",
			        C_DIM, sub->description ? sub->description : "", C_RESET);
		}
	}

	fprintf(stderr, "\n%sOptions (inherited):%s\n", C_BOLD, C_RESET);
	print_builtin_option(stderr, "-h", "--help", NULL, "Show this help message", opt_width);
	print_builtin_option(stderr, "-v", "--version", NULL, "Show version", opt_width);

	if (cmd->subcommand_count > 0) {
		fprintf(stderr, "\n%sSee '%s", C_DIM, parser->prog_name);
		for (int i = chain_len - 1; i >= 0; i--)
			fprintf(stderr, " %s", chain[i]->name);
		fprintf(stderr, " %s SUBCOMMAND --help' for more information.%s\n", cmd->name, C_RESET);
	}

	fprintf(stderr, "\n");
}
