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
 *   - Nested subcommands (multi-level)
 *   - Global options inherited by subcommands
 *   - Mutually exclusive option groups
 *   - Required options with validation
 *   - Default values wired to storage
 *   - Environment variable fallback
 *   - Subcommand aliases
 *   - Shell completion generation
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

	/* Root command for global options */
	p->root.name        = "";
	p->root.description = NULL;
	p->root.callback    = NULL;

	return p;
}

void argparse_free(ArgParser *parser)
{
	if (!parser)
		return;
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
	cmd->parent      = NULL;

	return cmd;
}

ArgCommand *argparse_add_subcommand(ArgCommand  *parent,
                                    const char  *name,
                                    const char  *description,
                                    arg_callback callback)
{
	if (!parent || parent->subcommand_count >= ARGPARSE_MAX_COMMANDS)
		return NULL;

	ArgCommand *cmd  = parent->subcommands[parent->subcommand_count++];
	memset(cmd, 0, sizeof(ArgCommand));
	cmd->name        = name;
	cmd->description = description;
	cmd->callback    = callback;
	cmd->parent      = parent;

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

static ArgOption *find_option_global(ArgParser *parser, const char *long_name)
{
	ArgOption *opt = find_option_long(&parser->root, long_name);
	if (opt)
		return opt;

	for (int i = 0; i < parser->command_count; i++) {
		opt = find_option_long(&parser->commands[i], long_name);
		if (opt)
			return opt;
	}
	return NULL;
}

static ArgOption *find_option_global_short(ArgParser *parser, char c)
{
	ArgOption *opt = find_option_short(&parser->root, c);
	if (opt)
		return opt;

	for (int i = 0; i < parser->command_count; i++) {
		opt = find_option_short(&parser->commands[i], c);
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
	printf("    commands=\"%s", parser->commands[0].name);
	for (int i = 1; i < parser->command_count; i++)
		printf(" %s", parser->commands[i].name);
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
		printf("        '");
		if (opt->short_name)
			printf("-%c[%%s]--%s[%%s]'", opt->short_name, opt->long_name);
		else
			printf("--%s[%%s]'", opt->long_name);
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

/* ── Token processing ──────────────────────────────────────────────────────── */

static int parse_tokens(ArgParser  *parser,
                        ArgCommand *cmd,
                        Lexer      *lex,
                        ArgParseResult *result)
{
	const char *program = parser->prog_name;
	if (cmd != &parser->root && cmd->name && cmd->name[0] != '\0')
		program = cmd->name;

	while (1) {
		Token tok = lexer_next(lex);

		switch (tok.type) {
		case TOKEN_END:
			return 0;

		case TOKEN_LONG_OPTION: {
			const char *raw = tok.value + 2; /* skip "--" */

			/* Split on '=' */
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

			/* Special: --help */
			if (strcmp(key, "help") == 0) {
				argparse_help(parser, cmd);
				return -2;
			}
			/* Special: --version */
			if (strcmp(key, "version") == 0) {
				fprintf(stderr, "%s %s\n", parser->prog_name, parser->version);
				return -3;
			}
			/* Special: --shell-completion <shell> */
			if (strcmp(key, "shell-completion") == 0) {
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

			ArgOption *opt = find_option_long(cmd, key);
			if (!opt && cmd != &parser->root)
				opt = find_option_long(&parser->root, key);
			if (!opt) {
				const char *known[ARGPARSE_MAX_OPTIONS];
				int         known_count = 0;
				for (int i = 0; i < cmd->option_count; i++)
					known[known_count++] = cmd->options[i].long_name;
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

				if (c == 'h') {
					argparse_help(parser, cmd);
					return -2;
				}
				if (c == 'v') {
					fprintf(stderr, "%s %s\n", parser->prog_name, parser->version);
					return -3;
				}

				ArgOption *opt = find_option_short(cmd, c);
				if (!opt && cmd != &parser->root)
					opt = find_option_short(&parser->root, c);
				if (!opt) {
					char        short_str[3] = { '-', c, '\0' };
					const char *known[ARGPARSE_MAX_OPTIONS];
					int         known_count = 0;
					for (int j = 0; j < cmd->option_count; j++) {
						known[known_count++] = cmd->options[j].long_name
						                        ? cmd->options[j].long_name
						                        : "?";
					}
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
						/* Next token is value */
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

		case TOKEN_POSITIONAL:
			if (result->positional_count < ARGPARSE_MAX_POSITIONAL) {
				result->positionals[result->positional_count++] = tok.value;
			}
			break;

		default:
			break;
		}
	}
}

/* ── Command matching ──────────────────────────────────────────────────────── */

static ArgCommand *match_command(ArgParser *parser, const char *name)
{
	for (int i = 0; i < parser->command_count; i++) {
		ArgCommand *cmd = &parser->commands[i];

		/* Match by name */
		if (strcmp(cmd->name, name) == 0)
			return cmd;

		/* Match by aliases */
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

/* ── Main parse entry point ────────────────────────────────────────────────── */

int argparse_parse(ArgParser *parser, int argc, char **argv)
{
	if (!parser || argc < 1)
		return -1;

	Lexer lex;
	lexer_init(&lex, argc, argv);

	/* Skip argv[0] (program name) */
	lexer_next(&lex);

	/* First pass: scan past global options to find command name.
	 * This allows: gitm -L debug list, gitm --dry-run status, etc.
	 *
	 * We do a manual scan of argv[1..] looking for the first
	 * non-option positional that matches a command name. */
	int cmd_argc    = argc - 1;
	char **cmd_argv = argv + 1;

	ArgCommand *matched         = NULL;
	int         command_arg_pos = -1;

	for (int i = 0; i < cmd_argc; i++) {
		const char *arg = cmd_argv[i];

		/* Skip options and their values */
		if (arg[0] == '-') {
			if (arg[1] == '-') {
				/* Long option */
				if (arg[2] == '\0')
					break; /* "--" stops option scanning */
				/* Check for --key=val (no value consumed) */
				if (strchr(arg + 2, '=') != NULL)
					continue;
				/* Check if this is a known long option that takes a value */
				ArgOption *opt = find_option_global(parser, arg + 2);
				if (opt && (opt->type == ARG_TYPE_INT || opt->type == ARG_TYPE_STRING))
					i++; /* skip value */
				continue;
			}
			/* Short option(s) */
			const char *flags = arg + 1;
			for (size_t j = 0; flags[j] != '\0'; j++) {
				ArgOption *opt = find_option_global_short(parser, flags[j]);
				if (opt && (opt->type == ARG_TYPE_INT || opt->type == ARG_TYPE_STRING)) {
					if (flags[j + 1] != '\0') {
						/* Attached value like -Ldebug — rest of flags is value */
						break;
					}
					i++; /* skip next token as value */
					break;
				}
			}
			continue;
		}

		/* Found a non-option — check if it matches a command */
		ArgCommand *cmd = match_command(parser, arg);
		if (cmd) {
			matched         = cmd;
			command_arg_pos = i;
			break;
		}

		/* Unknown positional — stop scanning */
		break;
	}

	if (!matched)
		matched = &parser->root;

	/* Build result */
	ArgParseResult result = { 0 };
	result.parser         = parser;
	result.command        = matched;
	result.argc           = argc;
	result.argv           = argv;

	/* Reset lexer to start parsing from command position.
	 * We need to re-init the lexer so parse_tokens processes
	 * everything from the beginning (argv[0] is skipped by lexer_next). */
	lexer_init(&lex, argc, argv);
	lexer_next(&lex); /* skip argv[0] */

	/* If we found a command at command_arg_pos, skip to it in the lexer */
	if (command_arg_pos >= 0) {
		/* Skip global args + the command token itself */
		for (int i = 0; i < command_arg_pos; i++)
			lexer_next(&lex);
		/* Consume the command token */
		lexer_next(&lex);
	}

	/* Parse tokens */
	int rc = parse_tokens(parser, matched, &lex, &result);

	if (rc == -2 || rc == -3 || rc == -4) {
		/* help, version, or shell-completion was already printed */
		parser->matched_command    = NULL;
		parser->matched_subcommand = NULL;
		return 0;
	}

	if (rc != 0)
		return -1;

	/* Apply defaults and env var fallback */
	apply_defaults(matched);

	/* Validate exclusive groups */
	if (validate_exclusive(matched, parser->prog_name) != 0)
		return -1;

	/* Validate required options */
	if (validate_required(matched, parser->prog_name) != 0)
		return -1;

	/* Check for nested subcommands */
	ArgCommand *sub = NULL;
	if (result.positional_count > 0) {
		sub = match_subcommand(matched, result.positionals[0]);
		if (sub) {
			/* Shift positionals: remove matched subcommand name */
			result.positional_count--;
			for (int i = 0; i < result.positional_count; i++)
				result.positionals[i] = result.positionals[i + 1];

			/* Apply defaults to subcommand too */
			apply_defaults(sub);
			if (validate_exclusive(sub, parser->prog_name) != 0)
				return -1;
			if (validate_required(sub, parser->prog_name) != 0)
				return -1;
		}
	}

	/* Store results */
	parser->matched_command    = matched;
	parser->matched_subcommand = sub;
	parser->argc               = argc;
	parser->argv               = argv;

	/* Execute callback — deepest matched command first, then parent */
	if (sub && sub->callback) {
		ArgParseResult *heap_result = malloc(sizeof(ArgParseResult));
		if (!heap_result)
			return -1;
		memcpy(heap_result, &result, sizeof(ArgParseResult));
		heap_result->command = sub;

		int cb_rc = sub->callback(heap_result);
		free(heap_result);
		return cb_rc;
	}

	if (matched->callback) {
		ArgParseResult *heap_result = malloc(sizeof(ArgParseResult));
		if (!heap_result)
			return -1;
		memcpy(heap_result, &result, sizeof(ArgParseResult));

		int cb_rc = matched->callback(heap_result);
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

	/* If currently typing an option */
	if (cur[0] == '-') {
		/* Determine which command we're completing for */
		const ArgCommand *cmd = &parser->root;

		for (int i = 1; i < argc - 1; i++) {
			const ArgCommand *found = match_command((ArgParser *) parser, argv[i]);
			if (found) {
				cmd = found;
				break;
			}
		}

		/* Long options */
		if (cur[1] == '-') {
			for (int i = 0; i < cmd->option_count; i++) {
				if (cmd->options[i].long_name)
					printf("--%s\n", cmd->options[i].long_name);
			}
		} else {
			/* Short options */
			for (int i = 0; i < cmd->option_count; i++) {
				if (cmd->options[i].short_name)
					printf("-%c\n", cmd->options[i].short_name);
			}
		}
		return;
	}

	/* Completing a command name */
	/* First find the deepest matched command */
	const ArgCommand *parent = &parser->root;

	for (int i = 1; i < argc - 1; i++) {
		const ArgCommand *found = match_command((ArgParser *) parser, argv[i]);
		if (found)
			parent = found;
	}

	/* If we're at root, list top-level commands */
	if (parent == &parser->root) {
		for (int i = 0; i < parser->command_count; i++) {
			if (strncmp(parser->commands[i].name, cur, strlen(cur)) == 0)
				printf("%s\n", parser->commands[i].name);
		}
	} else {
		/* List subcommands of parent */
		for (int i = 0; i < parent->subcommand_count; i++) {
			if (strncmp(parent->subcommands[i]->name, cur, strlen(cur)) == 0)
				printf("%s\n", parent->subcommands[i]->name);
		}
	}
}
