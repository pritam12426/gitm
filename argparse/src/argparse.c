/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * argparse.c — Core argument parser
 *
 * Ties together lexer, parser, help, and error modules.
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

#include "argparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"

/* ── Constructor / destructor ──────────────────────────────────────────────── */

ArgParser *argparse_new(const ArgParserConfig *config)
{
	if (!config || !config->prog_name || !config->version)
		return NULL;

	ArgParser *p = calloc(1, sizeof(ArgParser));
	if (!p)
		return NULL;

	p->prog_name   = config->prog_name;
	p->version     = config->version;
	p->description = config->description;
	p->bug_url     = config->bug_url;
	p->author      = config->author;

	/* Root command for global options. Its parent is NULL — this is
	 * the one node in the tree that terminates the upward walk. */
	p->root.name        = "";
	p->root.description = NULL;
	p->root.callback     = NULL;
	p->root.parent       = NULL;

	return p;
}

/* Recursively free every heap-allocated subcommand under cmd (but not
 * cmd itself — cmd may be stack/array/struct-embedded, e.g. a
 * top-level command living inside parser->commands[], or the root). */
static void free_subtree(ArgCommand *cmd)
{
	for (int i = 0; i < cmd->subcommand_count; i++) {
		free_subtree(cmd->subcommands[i]);
		free(cmd->subcommands[i]);
	}
}

void argparse_free(ArgParser *parser)
{
	if (!parser)
		return;

	for (int i = 0; i < parser->command_count; i++)
		free_subtree(&parser->commands[i]);
	free_subtree(&parser->root);

	free(parser);
}

/* ── Setters ───────────────────────────────────────────────────────────────── */

void argparse_set_description(ArgParser *parser, const char *desc)
{
	if (parser)
		parser->description = desc;
}

void argparse_set_bug_url(ArgParser *parser, const char *url)
{
	if (parser)
		parser->bug_url = url;
}

void argparse_set_author(ArgParser *parser, const char *author)
{
	if (parser)
		parser->author = author;
}

/* ── Building the command tree ─────────────────────────────────────────────── */

ArgCommand *argparse_add_command(ArgParser   *parser,
                                 const char  *name,
                                 const char  *description,
                                 arg_callback callback)
{
	if (!parser || parser->command_count >= ARGPARSE_MAX_COMMANDS)
		return NULL;

	ArgCommand *cmd  = &parser->commands[parser->command_count++];
	cmd->name        = name;
	cmd->description = description;
	cmd->callback    = callback;

	/* Top-level commands are children of root, not orphans — this is
	 * what lets option lookup and help text walk the tree uniformly
	 * all the way up, instead of treating "top-level" as a special
	 * case with no ancestor. */
	cmd->parent = &parser->root;

	return cmd;
}

ArgCommand *argparse_add_subcommand(ArgCommand  *parent,
                                    const char  *name,
                                    const char  *description,
                                    arg_callback callback)
{
	if (!parent || parent->subcommand_count >= ARGPARSE_MAX_COMMANDS)
		return NULL;

	/* `subcommands` is an array of pointers — each entry needs real
	 * storage behind it. (Previously this indexed the array as if it
	 * already held a valid ArgCommand*, which it never did.) */
	ArgCommand *cmd = calloc(1, sizeof(ArgCommand));
	if (!cmd)
		return NULL;

	cmd->name        = name;
	cmd->description = description;
	cmd->callback    = callback;
	cmd->parent      = parent;

	parent->subcommands[parent->subcommand_count++] = cmd;

	return cmd;
}

void argparse_command_set_aliases(ArgCommand  *cmd,
                                  const char **aliases,
                                  int          count)
{
	if (!cmd || count > ARGPARSE_MAX_ALIASES)
		return;

	for (int i = 0; i < count && i < ARGPARSE_MAX_ALIASES; i++)
		cmd->aliases[i] = aliases[i];
	cmd->alias_count = count;
}

/* ── Option building ───────────────────────────────────────────────────────── */

void argparse_add_option(ArgCommand   *command,
                         const char   *long_name,
                         char          short_name,
                         ArgOptionType type,
                         const char   *metavar,
                         const char   *description,
                         void         *storage)
{
	if (!command || command->option_count >= ARGPARSE_MAX_OPTIONS)
		return;

	ArgOption *opt    = &command->options[command->option_count++];
	opt->long_name    = long_name;
	opt->short_name   = short_name;
	opt->type         = type;
	opt->metavar      = metavar;
	opt->description  = description;
	opt->storage      = storage;
	opt->exclusive_group = 0;
	opt->env_var      = NULL;
	opt->was_set      = false;
}

void argparse_add_option_with_env(ArgCommand   *command,
                                   const char   *long_name,
                                   char          short_name,
                                   ArgOptionType type,
                                   const char   *metavar,
                                   const char   *description,
                                   void         *storage,
                                   const char   *env_var)
{
	if (!command || command->option_count >= ARGPARSE_MAX_OPTIONS)
		return;

	ArgOption *opt    = &command->options[command->option_count++];
	opt->long_name    = long_name;
	opt->short_name   = short_name;
	opt->type         = type;
	opt->metavar      = metavar;
	opt->description  = description;
	opt->storage      = storage;
	opt->env_var      = env_var;
	opt->exclusive_group = 0;
	opt->was_set      = false;
}

void argparse_add_option_exclusive(ArgCommand   *command,
                                   const char   *long_name,
                                   char          short_name,
                                   ArgOptionType type,
                                   const char   *metavar,
                                   const char   *description,
                                   void         *storage,
                                   int           exclusive_group)
{
	if (!command || command->option_count >= ARGPARSE_MAX_OPTIONS)
		return;

	ArgOption *opt    = &command->options[command->option_count++];
	opt->long_name    = long_name;
	opt->short_name   = short_name;
	opt->type         = type;
	opt->metavar      = metavar;
	opt->description  = description;
	opt->storage      = storage;
	opt->exclusive_group = exclusive_group;
	opt->env_var      = NULL;
	opt->was_set      = false;
}

void argparse_add_positional(ArgCommand *command, const char *name)
{
	if (!command || command->positional_count >= ARGPARSE_MAX_POSITIONAL)
		return;

	command->positionals[command->positional_count++] = name;
}

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

static int validate_exclusive(ArgCommand *cmd, const char *program)
{
	int groups[ARGPARSE_MAX_EXCLUSIVE_GROUPS] = { 0 };

	for (int i = 0; i < cmd->option_count; i++) {
		ArgOption *opt = &cmd->options[i];
		if (opt->exclusive_group > 0 && opt->was_set) {
			int gid = opt->exclusive_group;
			if (groups[gid]) {
				fprintf(stderr, "%s: options in group %d are mutually exclusive\n",
				        program, gid);
				return -1;
			}
			groups[gid] = 1;
		}
	}
	return 0;
}

/* ── Required options validation ───────────────────────────────────────────── */

static int validate_required(ArgCommand *cmd, const char *program)
{
	for (int i = 0; i < cmd->option_count; i++) {
		ArgOption *opt = &cmd->options[i];
		if (opt->required && !opt->was_set) {
			if (opt->long_name)
				fprintf(stderr, "%s: required option '--%s' is missing\n",
				        program, opt->long_name);
			else
				fprintf(stderr, "%s: required option '-%c' is missing\n",
				        program, opt->short_name);
			return -1;
		}
	}
	return 0;
}

/* ── Shell completion script generation ────────────────────────────────────── */

static void print_completion_bash(const ArgParser *parser)
{
	const char *prog = parser->prog_name;

	printf("_%s()\n{\n", prog);
	printf("    local cur prev commands\n");
	printf("    COMPREPLY=()\n");
	printf("    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
	printf("    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n");

	printf("    commands=\"");
	for (int i = 0; i < parser->command_count; i++)
		printf("%s%s", i > 0 ? " " : "", parser->commands[i].name);
	printf("\"\n\n");

	printf("    case \"${prev}\" in\n");
	for (int i = 0; i < parser->command_count; i++) {
		const ArgCommand *cmd = &parser->commands[i];
		printf("        %s)\n", cmd->name);
		printf("            COMPREPLY=($(compgen -W \"");
		for (int j = 0; j < cmd->option_count; j++) {
			if (j > 0) printf(" ");
			if (cmd->options[j].long_name)
				printf("--%s", cmd->options[j].long_name);
		}
		printf("\" -- \"${cur}\"))\n");
		printf("            return 0\n");
		printf("            ;;\n");
	}
	printf("    esac\n\n");

	printf("    if [[ ${cur} == -* ]]; then\n");
	printf("        COMPREPLY=($(compgen -W \"");
	for (int i = 0; i < parser->root.option_count; i++) {
		if (i > 0) printf(" ");
		if (parser->root.options[i].long_name)
			printf("--%s", parser->root.options[i].long_name);
	}
	printf("\" -- \"${cur}\"))\n");
	printf("    else\n");
	printf("        COMPREPLY=($(compgen -W \"${commands}\" -- \"${cur}\"))\n");
	printf("    fi\n");
	printf("    return 0\n");
	printf("}\n");
	printf("complete -F _%s %s\n", prog, prog);
}

static void print_completion_zsh(const ArgParser *parser)
{
	const char *prog = parser->prog_name;

	printf("#compdef %s\n\n", prog);
	printf("_%s()\n{\n", prog);
	printf("    _arguments \\\n");

	/* Global options */
	for (int i = 0; i < parser->root.option_count; i++) {
		const ArgOption *opt = &parser->root.options[i];
		if (!opt->long_name) continue;
		const char *desc = opt->description ? opt->description : "";
		printf("        '");
		if (opt->short_name)
			printf("-%c[%s]--%s[%s]'", opt->short_name, desc, opt->long_name, desc);
		else
			printf("--%s[%s]'", opt->long_name, desc);
		printf(" \\\n");
	}

	/* Commands */
	printf("        '1:command:->command' \\\n");
	printf("        '*::arg:->args'\n\n");

	printf("    case $state in\n");
	printf("    command)\n");
	printf("        _values 'command' \\\n");
	for (int i = 0; i < parser->command_count; i++) {
		printf("            '%s'['%s'] \\\n",
		       parser->commands[i].name,
		       parser->commands[i].description ? parser->commands[i].description : "");
	}
	printf("        ;;\n");
	printf("    esac\n");

	printf("    case $words[1] in\n");
	for (int i = 0; i < parser->command_count; i++) {
		const ArgCommand *cmd = &parser->commands[i];
		printf("    %s)\n", cmd->name);
		printf("        _arguments \\\n");
		for (int j = 0; j < cmd->option_count; j++) {
			const ArgOption *opt = &cmd->options[j];
			if (!opt->long_name) continue;
			printf("            '");
			if (opt->short_name)
				printf("-%c[%s]--%s[%s]'",
				       opt->short_name,
				       opt->description ? opt->description : "",
				       opt->long_name,
				       opt->description ? opt->description : "");
			else
				printf("--%s[%s]'",
				       opt->long_name,
				       opt->description ? opt->description : "");
			printf(" \\\n");
		}
		printf("        ;;\n");
	}
	printf("    esac\n");
	printf("}\n");
	printf("_%s \"$@\"\n", prog);
}

static void print_completion_fish(const ArgParser *parser)
{
	const char *prog = parser->prog_name;

	for (int i = 0; i < parser->command_count; i++) {
		const ArgCommand *cmd = &parser->commands[i];
		printf("complete -c %s -f -a '%s' -d '%s'\n",
		       prog, cmd->name,
		       cmd->description ? cmd->description : "");
	}

	printf("\n# Global options\n");
	for (int i = 0; i < parser->root.option_count; i++) {
		const ArgOption *opt = &parser->root.options[i];
		if (!opt->long_name) continue;
		if (opt->short_name)
			printf("complete -c %s -s %c -l %s -d '%s'\n",
			       prog, opt->short_name, opt->long_name,
			       opt->description ? opt->description : "");
		else
			printf("complete -c %s -l %s -d '%s'\n",
			       prog, opt->long_name,
			       opt->description ? opt->description : "");
	}

	printf("\n# Command-specific options\n");
	for (int i = 0; i < parser->command_count; i++) {
		const ArgCommand *cmd = &parser->commands[i];
		for (int j = 0; j < cmd->option_count; j++) {
			const ArgOption *opt = &cmd->options[j];
			if (!opt->long_name) continue;
			if (opt->short_name)
				printf("complete -c %s -n '__fish_seen_subcommand_from %s' -s %c -l %s -d '%s'\n",
				       prog, cmd->name, opt->short_name, opt->long_name,
				       opt->description ? opt->description : "");
			else
				printf("complete -c %s -n '__fish_seen_subcommand_from %s' -l %s -d '%s'\n",
				       prog, cmd->name, opt->long_name,
				       opt->description ? opt->description : "");
		}
	}
}

static void shell_completion(const ArgParser *parser, const char *shell)
{
	if (parser->command_count == 0 &&
	    (strcmp(shell, "bash") == 0 || strcmp(shell, "zsh") == 0)) {
		/* bash/zsh generators assume at least one registered command
		 * exists; with none, still emit a minimal, valid script
		 * instead of touching commands[0] out of bounds. */
		fprintf(stderr, "%s: no commands registered, nothing to complete\n",
		        parser->prog_name);
		return;
	}

	if (strcmp(shell, "bash") == 0) {
		print_completion_bash(parser);
	} else if (strcmp(shell, "zsh") == 0) {
		print_completion_zsh(parser);
	} else if (strcmp(shell, "fish") == 0) {
		print_completion_fish(parser);
	} else {
		fprintf(stderr, "%s: unsupported shell: %s (use bash, zsh, or fish)\n",
		        parser->prog_name, shell);
	}
}

/* ── Command matching ──────────────────────────────────────────────────────── */

static ArgCommand *match_command(ArgParser *parser, const char *name)
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

static ArgCommand *match_subcommand(ArgCommand *parent, const char *name)
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
		Token tok = lexer_next(lex);
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
						known[known_count++] = current->options[j].long_name
						                        ? current->options[j].long_name
						                        : "?";
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

			ArgCommand *sub = (current == &parser->root)
			                    ? match_command(parser, tok.value)
			                    : match_subcommand(current, tok.value);

			if (sub && *chain_len < ARGPARSE_MAX_CHAIN_DEPTH) {
				chain[(*chain_len)++] = sub;
				current               = sub;
				break;
			}

			bool expects_subcommand = (current == &parser->root)
			                            ? (parser->command_count > 0)
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
		if (validate_exclusive(chain[i], parser->prog_name) != 0)
			return -1;
		if (validate_required(chain[i], parser->prog_name) != 0)
			return -1;
	}

	parser->matched_command    = (chain_len > 1) ? chain[1] : NULL;
	parser->matched_subcommand = (chain_len > 2) ? chain[chain_len - 1] : NULL;
	parser->argc               = argc;
	parser->argv               = argv;

	result.command = current;

	if (current->callback) {
		ArgParseResult *heap_result = malloc(sizeof(*heap_result));
		if (!heap_result)
			return -1;
		memcpy(heap_result, &result, sizeof(result));

		int cb_rc = current->callback(heap_result);
		free(heap_result);
		return cb_rc;
	}

	/* No callback — return 0, let caller decide what to do */
	return 0;
}

/* ── Completion ────────────────────────────────────────────────────────────── */

void argparse_complete(const ArgParser *parser, int argc, char **argv)
{
	if (!parser || argc < 2)
		return;

	const char *cur = argv[argc - 1];

	/* Walk as deep into the command tree as the already-typed words
	 * allow, so completion works at any nesting depth, not just one
	 * level below root. */
	ArgCommand *current = (ArgCommand *) &parser->root;
	for (int i = 1; i < argc - 1; i++) {
		ArgCommand *next = (current == &parser->root)
		                     ? match_command((ArgParser *) parser, argv[i])
		                     : match_subcommand(current, argv[i]);
		if (!next)
			break;
		current = next;
	}

	if (cur[0] == '-') {
		if (cur[1] == '-') {
			for (ArgCommand *c = current; c; c = c->parent)
				for (int i = 0; i < c->option_count; i++)
					if (c->options[i].long_name)
						printf("--%s\n", c->options[i].long_name);
		} else {
			for (ArgCommand *c = current; c; c = c->parent)
				for (int i = 0; i < c->option_count; i++)
					if (c->options[i].short_name)
						printf("-%c\n", c->options[i].short_name);
		}
		return;
	}

	if (current == &parser->root) {
		for (int i = 0; i < parser->command_count; i++)
			if (strncmp(parser->commands[i].name, cur, strlen(cur)) == 0)
				printf("%s\n", parser->commands[i].name);
	} else {
		for (int i = 0; i < current->subcommand_count; i++)
			if (strncmp(current->subcommands[i]->name, cur, strlen(cur)) == 0)
				printf("%s\n", current->subcommands[i]->name);
	}
}
