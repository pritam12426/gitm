/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * build.c — Parser construction and command tree building
 *
 * All public API for creating/destroying parsers, registering
 * commands and subcommands, adding options, and setting metadata.
 */

#include "argparse.h"

#include <stdlib.h>

// ── Constructor / destructor ────────────────────────────────────────────────

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
	p->root.callback    = NULL;
	p->root.parent      = NULL;

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

// ── Setters ─────────────────────────────────────────────────────────────────

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

// ── Building the command tree ───────────────────────────────────────────────

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

void argparse_command_set_aliases(ArgCommand *cmd, const char **aliases, int count)
{
	if (!cmd || count > ARGPARSE_MAX_ALIASES)
		return;

	for (int i = 0; i < count && i < ARGPARSE_MAX_ALIASES; i++)
		cmd->aliases[i] = aliases[i];
	cmd->alias_count = count;
}

// ── Option building ─────────────────────────────────────────────────────────

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

	ArgOption *opt       = &command->options[command->option_count++];
	opt->long_name       = long_name;
	opt->short_name      = short_name;
	opt->type            = type;
	opt->metavar         = metavar;
	opt->description     = description;
	opt->storage         = storage;
	opt->exclusive_group = 0;
	opt->env_var         = NULL;
	opt->was_set         = false;
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

	ArgOption *opt       = &command->options[command->option_count++];
	opt->long_name       = long_name;
	opt->short_name      = short_name;
	opt->type            = type;
	opt->metavar         = metavar;
	opt->description     = description;
	opt->storage         = storage;
	opt->env_var         = env_var;
	opt->exclusive_group = 0;
	opt->was_set         = false;
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

	ArgOption *opt       = &command->options[command->option_count++];
	opt->long_name       = long_name;
	opt->short_name      = short_name;
	opt->type            = type;
	opt->metavar         = metavar;
	opt->description     = description;
	opt->storage         = storage;
	opt->exclusive_group = exclusive_group;
	opt->env_var         = NULL;
	opt->was_set         = false;
}

void argparse_add_positional(ArgCommand *command, const char *name)
{
	if (!command || command->positional_count >= ARGPARSE_MAX_POSITIONAL)
		return;

	command->positionals[command->positional_count++] = name;
}
