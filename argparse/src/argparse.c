/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * argparse.c — Core argument parser
 *
 * Ties together lexer, parser, help, error, and completion modules.
 * Manages the command tree and performs the actual parsing.
 *
 * Features:
 *   - Nested subcommands (arbitrary depth, not just one level)
 *   - Global options inherited by every level below them
 *   - Mutually exclusive option groups
 *   - Required options with validation
 *   - Default values wired to storage
 *   - Environment variable fallback
 *   - Subcommand aliases
 *   - Shell completion generation
 *
 * Parsing model
 * -------------
 * The whole command tree — root, top-level commands, and every nested
 * subcommand — forms a single tree where every node's `parent` points
 * upward, all the way to `&parser->root` (whose own parent is NULL).
 * Parsing is therefore a single forward pass over the token stream:
 * we track a "current" node starting at the root, and each time a
 * positional token matches one of `current`'s subcommands we descend
 * into it. Options are resolved by walking upward from `current`
 * through its ancestors, so a global option registered on root is
 * visible no matter how deep we've descended, and a subcommand's own
 * options are only visible once we've actually entered it.
 */

#define _POSIX_C_SOURCE 200809L /* for fileno() under strict -std=c17 */

#include "argparse.h"
#include "argparse_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "lexer.h"

/* ── Colour support for validation messages ───────────────────────────────── */

static int g_val_use_color = -1; /* -1 = not detected yet */

static void detect_val_color(void)
{
	if (g_val_use_color >= 0)
		return;
	g_val_use_color = isatty(fileno(stderr)) ? 1 : 0;
}

#define V_C_RESET    (g_val_use_color ? "\x1b[0m" : "")
#define V_C_BOLD     (g_val_use_color ? "\x1b[1m" : "")
#define V_C_BOLD_RED (g_val_use_color ? "\x1b[1;31m" : "")

/* ── Option lookup ─────────────────────────────────────────────────────────── */

static ArgOption *find_option_long(ArgCommand *cmd, const char *name)
{
	for (int i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].long_name && strcmp(cmd->options[i].long_name, name) == 0)
			return &cmd->options[i];
	}
	return NULL;
}

static ArgOption *find_option_short(ArgCommand *cmd, char c)
{
	for (int i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].short_name == c)
			return &cmd->options[i];
	}
	return NULL;
}

/* Walk upward from `cmd` through every ancestor (to root) looking for
 * the option. This is what makes "global options inherited by
 * subcommands" work at any depth, and also what makes a subcommand's
 * own options finally reachable while parsing that subcommand. */
static ArgOption *find_option_long_chain(ArgCommand *cmd, const char *name)
{
	for (ArgCommand *c = cmd; c; c = c->parent) {
		ArgOption *opt = find_option_long(c, name);
		if (opt)
			return opt;
	}
	return NULL;
}

static ArgOption *find_option_short_chain(ArgCommand *cmd, char name)
{
	for (ArgCommand *c = cmd; c; c = c->parent) {
		ArgOption *opt = find_option_short(c, name);
		if (opt)
			return opt;
	}
	return NULL;
}

/* ── Default value initialization ──────────────────────────────────────────── */

static void apply_defaults(ArgCommand *cmd)
{
	for (int i = 0; i < cmd->option_count; i++) {
		ArgOption *opt = &cmd->options[i];

		/* Apply env var fallback first (env < default < CLI) */
		if (opt->env_var && !opt->was_set) {
			const char *env_val = getenv(opt->env_var);
			if (env_val) {
				switch (opt->type) {
				case ARG_TYPE_STRING:
					if (opt->storage)
						*(const char **) opt->storage = env_val;
					opt->was_set = true;
					break;
				case ARG_TYPE_INT:
					if (opt->storage)
						*(int *) opt->storage = atoi(env_val);
					opt->was_set = true;
					break;
				default:
					break;
				}
			}
		}

		/* Apply default value */
		if (opt->default_val && !opt->was_set) {
			switch (opt->type) {
			case ARG_TYPE_STRING:
				if (opt->storage)
					*(const char **) opt->storage = opt->default_val;
				break;
			case ARG_TYPE_INT:
				if (opt->storage)
					*(int *) opt->storage = atoi(opt->default_val);
				break;
			case ARG_TYPE_NONE:
			case ARG_TYPE_COUNT:
				break;
			}
		}
	}
}

/* ── Exclusive group validation ────────────────────────────────────────────── */

static int validate_exclusive(ArgParser *parser, ArgCommand *cmd)
{
	int groups[ARGPARSE_MAX_EXCLUSIVE_GROUPS] = { 0 };

	for (int i = 0; i < cmd->option_count; i++) {
		ArgOption *opt = &cmd->options[i];
		if (opt->exclusive_group > 0 && opt->was_set) {
			int gid = opt->exclusive_group;
			if (groups[gid]) {
				argparse_usage(parser, cmd == &parser->root ? NULL : cmd);
				detect_val_color();
				fprintf(stderr,
				        "%s%s: %serror:%s options in group %d are mutually exclusive\n",
				        V_C_BOLD,
				        parser->prog_name,
				        V_C_BOLD_RED,
				        V_C_RESET,
				        gid);
				return -1;
			}
			groups[gid] = 1;
		}
	}
	return 0;
}

/* ── Required options validation ───────────────────────────────────────────── */

static int validate_required(ArgParser *parser, ArgCommand *cmd)
{
	for (int i = 0; i < cmd->option_count; i++) {
		ArgOption *opt = &cmd->options[i];
		if (opt->required && !opt->was_set) {
			argparse_usage(parser, cmd == &parser->root ? NULL : cmd);
			detect_val_color();
			if (opt->long_name)
				fprintf(stderr,
				        "%s%s: %serror:%s required option '--%s' is missing\n",
				        V_C_BOLD,
				        parser->prog_name,
				        V_C_BOLD_RED,
				        V_C_RESET,
				        opt->long_name);
			else
				fprintf(stderr,
				        "%s%s: %serror:%s required option '-%c' is missing\n",
				        V_C_BOLD,
				        parser->prog_name,
				        V_C_BOLD_RED,
				        V_C_RESET,
				        opt->short_name);
			return -1;
		}
	}
	return 0;
}

/* ── Command matching ──────────────────────────────────────────────────────── */

ArgCommand *match_command(ArgParser *parser, const char *name)
{
	for (int i = 0; i < parser->command_count; i++) {
		ArgCommand *cmd = &parser->commands[i];

		if (strcmp(cmd->name, name) == 0)
			return cmd;

		for (int j = 0; j < cmd->alias_count; j++) {
			if (cmd->aliases[j] && strcmp(cmd->aliases[j], name) == 0)
				return cmd;
		}
	}
	return NULL;
}

ArgCommand *match_subcommand(ArgCommand *parent, const char *name)
{
	for (int i = 0; i < parent->subcommand_count; i++) {
		ArgCommand *cmd = parent->subcommands[i];

		if (strcmp(cmd->name, name) == 0)
			return cmd;

		for (int j = 0; j < cmd->alias_count; j++) {
			if (cmd->aliases[j] && strcmp(cmd->aliases[j], name) == 0)
				return cmd;
		}
	}
	return NULL;
}

/* Program name to use in error/usage messages for whichever node in
 * the tree we're currently parsing. */
static const char *chain_program_name(const ArgParser *parser, const ArgCommand *cmd)
{
	return (cmd == &parser->root) ? parser->prog_name : cmd->name;
}

/* ── Token processing ──────────────────────────────────────────────────────── */

/*
 * Single forward pass over the whole token stream. `current` starts at
 * root and descends into a subcommand every time a positional token
 * matches one of `current`'s children — to any depth, not just one
 * level. `chain` records every node entered (root included) so the
 * caller can apply defaults/validation at every level afterward.
 */
static int parse_tokens(ArgParser      *parser,
                        Lexer          *lex,
                        ArgParseResult *result,
                        ArgCommand    **chain,
                        int            *chain_len,
                        ArgCommand    **current_out)
{
	ArgCommand *current = *current_out;

	while (1) {
		Token       tok     = lexer_next(lex);
		const char *program = chain_program_name(parser, current);

		switch (tok.type) {
		case TOKEN_END:
			*current_out = current;
			return 0;

		case TOKEN_LONG_OPTION: {
			const char *raw = tok.value + 2; /* skip "--" */

			char        key[256] = { 0 };
			const char *eq       = strchr(raw, '=');
			const char *val      = NULL;

			if (eq) {
				size_t klen = (size_t) (eq - raw);
				if (klen >= sizeof(key))
					klen = sizeof(key) - 1;
				memcpy(key, raw, klen);
				val = eq + 1;
			} else {
				snprintf(key, sizeof(key), "%s", raw);
			}

			/* User-registered options take priority over the
			 * built-ins below — e.g. if you deliberately give your
			 * own option the long name "help", yours wins. The
			 * built-ins are a sensible default binding, not a
			 * reserved word. */
			ArgOption *opt = find_option_long_chain(current, key);

			if (!opt && strcmp(key, "help") == 0) {
				argparse_help(parser, current == &parser->root ? NULL : current);
				return -2;
			}
			if (!opt && strcmp(key, "version") == 0) {
				fprintf(stderr, "%s %s\n", parser->prog_name, parser->version);
				return -3;
			}
			if (!opt && strcmp(key, "shell-completion") == 0) {
				const char *shell = val;
				if (!shell) {
					Token vtok = lexer_next(lex);
					if (vtok.type == TOKEN_END || vtok.type == TOKEN_LONG_OPTION
					    || vtok.type == TOKEN_SHORT_OPTION) {
						argparse_usage(parser, current == &parser->root ? NULL : current);
						arg_error_missing_value(program, "--shell-completion");
						return -1;
					}
					shell = vtok.value;
				}
				shell_completion(parser, shell);
				return -4;
			}

			if (!opt) {
				const char *known[ARGPARSE_MAX_OPTIONS];
				int         known_count = 0;
				for (int i = 0; i < current->option_count; i++)
					known[known_count++] = current->options[i].long_name;
				argparse_usage(parser, current == &parser->root ? NULL : current);
				arg_error_unknown_option(program, tok.value, known, known_count);
				return -1;
			}

			opt->was_set = true;

			switch (opt->type) {
			case ARG_TYPE_NONE:
				if (opt->storage)
					*(bool *) opt->storage = true;
				break;
			case ARG_TYPE_COUNT:
				if (opt->storage)
					*(int *) opt->storage += 1;
				break;
			case ARG_TYPE_INT:
			case ARG_TYPE_STRING:
				if (!val) {
					Token vtok = lexer_next(lex);
					if (vtok.type == TOKEN_END || vtok.type == TOKEN_LONG_OPTION
					    || vtok.type == TOKEN_SHORT_OPTION) {
						argparse_usage(parser, current == &parser->root ? NULL : current);
						arg_error_missing_value(program, tok.value);
						return -1;
					}
					val = vtok.value;
				}
				if (opt->storage) {
					if (opt->type == ARG_TYPE_INT)
						*(int *) opt->storage = atoi(val);
					else
						*(const char **) opt->storage = val;
				}
				break;
			}
			break;
		}

		case TOKEN_SHORT_OPTION: {
			const char *flags = tok.value + 1; /* skip "-" */

			for (size_t i = 0; flags[i] != '\0'; i++) {
				char c = flags[i];

				/* Same priority rule as long options: a
				 * user-registered -h, -v, or -S wins over the
				 * built-in default. */
				ArgOption *opt = find_option_short_chain(current, c);

				if (!opt && c == 'h') {
					argparse_help(parser, current == &parser->root ? NULL : current);
					return -2;
				}
				if (!opt && c == 'v') {
					fprintf(stderr, "%s %s\n", parser->prog_name, parser->version);
					return -3;
				}
				if (!opt && c == 'S') {
					const char *shell;
					if (flags[i + 1] != '\0') {
						/* Attached: -Sbash */
						shell = &flags[i + 1];
					} else {
						Token vtok = lexer_next(lex);
						if (vtok.type == TOKEN_END || vtok.type == TOKEN_LONG_OPTION
						    || vtok.type == TOKEN_SHORT_OPTION) {
							argparse_usage(parser, current == &parser->root ? NULL : current);
							arg_error_missing_value(program, "-S");
							return -1;
						}
						shell = vtok.value;
					}
					shell_completion(parser, shell);
					return -4;
				}

				if (!opt) {
					char        short_str[3] = { '-', c, '\0' };
					const char *known[ARGPARSE_MAX_OPTIONS];
					int         known_count = 0;
					for (int j = 0; j < current->option_count; j++)
						known[known_count++] = current->options[j].long_name ? current->options[j].long_name : "?";
					argparse_usage(parser, current == &parser->root ? NULL : current);
					arg_error_unknown_option(program, short_str, known, known_count);
					return -1;
				}

				opt->was_set = true;

				switch (opt->type) {
				case ARG_TYPE_NONE:
					if (opt->storage)
						*(bool *) opt->storage = true;
					break;
				case ARG_TYPE_COUNT:
					if (opt->storage)
						*(int *) opt->storage += 1;
					break;
				case ARG_TYPE_INT:
				case ARG_TYPE_STRING: {
					if (flags[i + 1] != '\0') {
						/* Attached value: -Ldebug, -j4 */
						if (opt->storage) {
							if (opt->type == ARG_TYPE_INT)
								*(int *) opt->storage = atoi(&flags[i + 1]);
							else
								*(const char **) opt->storage = &flags[i + 1];
						}
						goto short_done;
					} else {
						Token vtok = lexer_next(lex);
						if (vtok.type == TOKEN_END || vtok.type == TOKEN_LONG_OPTION
						    || vtok.type == TOKEN_SHORT_OPTION) {
							char short_str2[3] = { '-', c, '\0' };
							argparse_usage(parser, current == &parser->root ? NULL : current);
							arg_error_missing_value(program, short_str2);
							return -1;
						}
						if (opt->storage) {
							if (opt->type == ARG_TYPE_INT)
								*(int *) opt->storage = atoi(vtok.value);
							else
								*(const char **) opt->storage = vtok.value;
						}
					}
					break;
				}
				}
			}

		short_done:
			break;
		}

		case TOKEN_POSITIONAL: {
			/* Tokens after "--" are raw data, never subcommands. */
			if (lex->stop_options) {
				if (result->rest_count < ARGPARSE_MAX_POSITIONAL)
					result->rest[result->rest_count++] = tok.value;
				break;
			}

			ArgCommand *sub = (current == &parser->root) ? match_command(parser, tok.value)
			                                             : match_subcommand(current, tok.value);

			if (sub && *chain_len < ARGPARSE_MAX_CHAIN_DEPTH) {
				chain[(*chain_len)++] = sub;
				current               = sub;
				break;
			}

			bool expects_subcommand = (current == &parser->root) ? (parser->command_count > 0)
			                                                     : (current->subcommand_count > 0);

			if (expects_subcommand && current->positional_count == 0) {
				const char *known[ARGPARSE_MAX_COMMANDS];
				int         known_count = 0;

				if (current == &parser->root) {
					for (int i = 0; i < parser->command_count; i++)
						known[known_count++] = parser->commands[i].name;
				} else {
					for (int i = 0; i < current->subcommand_count; i++)
						known[known_count++] = current->subcommands[i]->name;
				}
				argparse_usage(parser, current == &parser->root ? NULL : current);
				arg_error_unknown_command(program, tok.value, known, known_count);
				return -1;
			}

			if (result->positional_count < ARGPARSE_MAX_POSITIONAL)
				result->positionals[result->positional_count++] = tok.value;
			break;
		}

		default:
			break;
		}
	}
}

/* ── Main parse entry point ────────────────────────────────────────────────── */

int argparse_parse(ArgParser *parser, int argc, char **argv)
{
	if (!parser || argc < 1)
		return -1;

	Lexer lex;
	lexer_init(&lex, argc, argv);
	lexer_next(&lex); /* skip argv[0] (program name) */

	ArgParseResult result = { 0 };
	result.parser         = parser;
	result.argc           = argc;
	result.argv           = argv;

	ArgCommand *chain[ARGPARSE_MAX_CHAIN_DEPTH];
	int         chain_len = 0;
	chain[chain_len++]    = &parser->root;
	ArgCommand *current   = &parser->root;

	int rc = parse_tokens(parser, &lex, &result, chain, &chain_len, &current);

	if (rc == -2 || rc == -3 || rc == -4) {
		/* help, version, or shell-completion was already printed */
		parser->matched_command    = NULL;
		parser->matched_subcommand = NULL;
		return 0;
	}

	if (rc != 0)
		return -1;

	/* Apply defaults/env fallback and validate every level that was
	 * actually entered — root through the deepest matched subcommand.
	 * (Previously only the top-level command and the one subcommand
	 * below it were handled, so root-level required/default/exclusive
	 * options were silently skipped whenever any command ran.) */
	for (int i = 0; i < chain_len; i++) {
		apply_defaults(chain[i]);
		if (validate_exclusive(parser, chain[i]) != 0)
			return -1;
		if (validate_required(parser, chain[i]) != 0)
			return -1;
	}

	parser->matched_command    = (chain_len > 1) ? chain[1] : NULL;
	parser->matched_subcommand = (chain_len > 2) ? chain[chain_len - 1] : NULL;
	parser->argc               = argc;
	parser->argv               = argv;

	result.command = current;

	/* Store for dispatch — caller calls argparse_dispatch() after re-init */
	parser->dispatch_cmd    = current;
	parser->dispatch_result = malloc(sizeof(*parser->dispatch_result));
	if (!parser->dispatch_result)
		return -1;
	memcpy(parser->dispatch_result, &result, sizeof(result));

	/* No callback — return 0, let caller decide what to do */
	return 0;
}

/* ── Dispatch ──────────────────────────────────────────────────────────────── */

int argparse_dispatch(ArgParser *parser)
{
	if (!parser || !parser->dispatch_result)
		return 0;

	ArgParseResult *result = parser->dispatch_result;
	ArgCommand     *cmd    = parser->dispatch_cmd;

	int cb_rc = 0;
	if (cmd && cmd->callback)
		cb_rc = cmd->callback(result);

	free(result);
	parser->dispatch_result = NULL;
	parser->dispatch_cmd    = NULL;

	return cb_rc;
}
