/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * completion.c — Shell completion script generation
 *
 * Generates bash, zsh, and fish completion scripts, and provides
 * runtime completion candidate generation (argparse_complete).
 */

#define _POSIX_C_SOURCE 200809L /* for fileno() */

#include "argparse.h"
#include "argparse_internal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ── Bash completion ───────────────────────────────────────────────────────── */

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

/* ── Zsh completion ────────────────────────────────────────────────────────── */

/* Print a single _arguments option spec line for the given option.
 * Handles short+long pairing, value metavars, and description quoting.
 * If trailing is true, adds a line continuation backslash. */
static void zsh_print_option_spec(const ArgOption *opt, bool trailing)
{
	if (!opt->long_name)
		return;

	const char *desc = opt->description ? opt->description : "";
	const char *meta = NULL;

	if (opt->type == ARG_TYPE_STRING || opt->type == ARG_TYPE_INT) {
		if (opt->metavar)
			meta = opt->metavar;
		else if (opt->long_name)
			meta = opt->long_name;
	}

	if (opt->short_name) {
		if (meta)
			printf("            '(-%c --%s)'{-%c,--%s}'[%s]:%s:'",
			       opt->short_name, opt->long_name,
			       opt->short_name, opt->long_name, desc, meta);
		else
			printf("            '(-%c --%s)'{-%c,--%s}'[%s]'",
			       opt->short_name, opt->long_name,
			       opt->short_name, opt->long_name, desc);
	} else {
		if (meta)
			printf("            '--%s[%s]:%s:'",
			       opt->long_name, desc, meta);
		else
			printf("            '--%s[%s]'",
			       opt->long_name, desc);
	}

	if (trailing)
		printf(" \\\n");
	else
		printf("\n");
}

static void print_completion_zsh(const ArgParser *parser)
{
	const char *prog = parser->prog_name;

	printf("#compdef %s\n\n", prog);
	printf("_%s()\n{\n", prog);
	printf("    emulate -L zsh\n");
	printf("    local curcontext=\"${curcontext}\" state state_descr line\n");
	printf("    typeset -A opt_args\n\n");

	/* Global options */
	printf("    _arguments -s \\\n");
	for (int i = 0; i < parser->root.option_count; i++) {
		zsh_print_option_spec(&parser->root.options[i], true);
	}
	printf("        '1:command:->command' \\\n");
	printf("        '*::arg:->args'\n\n");

	/* State dispatch */
	printf("    case $state in\n");

	/* Command name completion */
	printf("    command)\n");
	printf("        _values 'command' \\\n");
	for (int i = 0; i < parser->command_count; i++) {
		printf("            '%s[%s]' \\\n",
		       parser->commands[i].name,
		       parser->commands[i].description ? parser->commands[i].description : "");
	}
	printf("        ;;\n");

	/* Command-specific option completion */
	printf("    args)\n");
	printf("        case $words[1] in\n");
	for (int i = 0; i < parser->command_count; i++) {
		const ArgCommand *cmd = &parser->commands[i];
		if (cmd->option_count == 0)
			continue;

		int last_idx = -1;
		for (int j = cmd->option_count - 1; j >= 0; j--) {
			if (cmd->options[j].long_name) {
				last_idx = j;
				break;
			}
		}

		printf("        %s)\n", cmd->name);
		printf("            _arguments -s \\\n");
		for (int j = 0; j < cmd->option_count; j++) {
			zsh_print_option_spec(&cmd->options[j], j != last_idx);
		}
		printf("            ;;\n");
	}
	printf("        esac\n");
	printf("        ;;\n");

	printf("    esac\n");

	printf("}\n");
	printf("_%s \"$@\"\n", prog);
}

/* ── Fish completion ───────────────────────────────────────────────────────── */

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

/* ── Completion dispatch ───────────────────────────────────────────────────── */

void shell_completion(const ArgParser *parser, const char *shell)
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

/* ── Runtime completion ────────────────────────────────────────────────────── */

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
