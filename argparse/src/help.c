/*
 * help.c — Automatic help generation with colour
 *
 * Produces formatted, coloured help output from the command tree.
 * Colour scheme inspired by Rust's clap:
 *   - Section headers: bold
 *   - Command names: bold cyan
 *   - Option flags: bold green
 *   - Metavars: cyan
 *   - Descriptions: dim
 *   - Usage keywords: bold
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "project_config.h"
#include "argparse.h"

/* ── Internal ANSI codes (no external deps) ──────────────────────────────────
 */

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

/* Count visible characters, skipping ANSI escape sequences */
static size_t visible_len(const char *s)
{
	size_t      vis = 0;
	const char *p   = s;

	while (*p) {
		if (*p == '\x1b' && *(p + 1) == '[') {
			/* Skip ANSI escape: \x1b[...<letter> */
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

/* Print a string padded to `width` visible columns */
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
		                         " %s%s%s", C_DIM, opt->metavar, C_RESET);

	fprintf(out, "  ");
	print_padded(out, flags, 38);
	fprintf(out, " %s%s%s\n", C_DIM, opt->description ? opt->description : "", C_RESET);
}


static void print_builtin_option(FILE       *out,
                                 const char *short_flag,
                                 const char *long_flag,
                                 const char *desc)
{
	char   flags[128] = { 0 };
	size_t len        = 0;

	len += (size_t) snprintf(flags + len, sizeof(flags) - len,
	                         "%s%s%s, ", C_BOLD_GREEN, short_flag, C_RESET);
	len += (size_t) snprintf(flags + len, sizeof(flags) - len,
	                         "%s%s%s", C_BOLD_GREEN, long_flag, C_RESET);

	fprintf(out, "  ");
	print_padded(out, flags, 38);
	fprintf(out, " %s%s%s\n", C_DIM, desc, C_RESET);
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
				fprintf(stderr, "  ");
				print_padded(stderr, parser->commands[i].name, 15);
				fprintf(stderr, " %s%s%s\n",
				        C_DIM,
				        parser->commands[i].description ? parser->commands[i].description : "",
				        C_RESET);
			}
			fprintf(stderr, "\n");
		}

		/* Global options */
		if (parser->root.option_count > 0) {
			fprintf(stderr, "%sOptions:%s\n", C_BOLD, C_RESET);
			for (int i = 0; i < parser->root.option_count; i++) {
				print_option(stderr, &parser->root.options[i]);
			}
		}

		/* Always show --help and --version */
		print_builtin_option(stderr, "-h", "--help", "Show this help message");
		print_builtin_option(stderr, "-v", "--version", "Show version");
		fprintf(stderr, "\n");

		fputs(C_DIM, stderr);
		fputs("Report bugs to: " PROJECT_HOMEPAGE_URL "/issues\n" , stderr);
		fputs(AUTH_MESSAGE "\n", stderr);
		fputs(C_RESET, stderr);

		return;
	}

	/* Subcommand help */
	fprintf(stderr, "%sUsage:%s %s%s %s%s",
	        C_BOLD, C_RESET, C_BOLD, parser->prog_name, cmd->name, C_RESET);

	for (int i = 0; i < cmd->positional_count; i++) {
		fprintf(stderr, " %s%s%s", C_DIM, cmd->positionals[i], C_RESET);
	}

	if (cmd->option_count > 0)
		fprintf(stderr, " %s[OPTIONS]%s", C_DIM, C_RESET);

	fprintf(stderr, "\n\n");

	if (cmd->description)
		fprintf(stderr, "%s%s%s\n\n", C_DIM, cmd->description, C_RESET);

	if (cmd->option_count > 0) {
		fprintf(stderr, "%sOptions:%s\n", C_BOLD, C_RESET);
		for (int i = 0; i < cmd->option_count; i++) {
			print_option(stderr, &cmd->options[i]);
		}
	}

	print_builtin_option(stderr, "-h", "--help", "Show this help message");
	fprintf(stderr, "\n");
}
