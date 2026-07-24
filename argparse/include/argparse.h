/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * argparse.h — Custom argument parser with subcommand support
 *
 * A lightweight, zero-dependency CLI argument parser written in C17.
 * Supports nested subcommands, short/long options, auto-help, and
 * automatic version output.
 *
 * Features:
 *   - Nested subcommands (multi-level)
 *   - Global options inherited by subcommands
 *   - Mutually exclusive option groups
 *   - Required options with validation
 *   - Default values (wired to storage)
 *   - Environment variable fallback
 *   - Subcommand aliases
 *   - Bash/zsh completion generation
 *   - Coloured help output
 */

#ifndef _ARGPARSE_H_
#define _ARGPARSE_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits ─────────────────────────────────────────────────────────────────── */

#define ARGPARSE_MAX_OPTIONS        32
#define ARGPARSE_MAX_COMMANDS       32
#define ARGPARSE_MAX_POSITIONAL     8
#define ARGPARSE_MAX_LINE_LEN       1024
#define ARGPARSE_MAX_ALIASES        4
#define ARGPARSE_MAX_EXCLUSIVE_GROUPS 8

/* ── Option types ───────────────────────────────────────────────────────────── */

typedef enum {
	ARG_TYPE_NONE,     /* boolean flag (no value) */
	ARG_TYPE_STRING,   /* takes a string value */
	ARG_TYPE_INT,      /* takes an integer value */
	ARG_TYPE_COUNT,    /* increments a counter */
} ArgOptionType;

/* ── Option ─────────────────────────────────────────────────────────────────── */

typedef struct ArgOption {
	const char     *long_name;     /* "--name" (without --) */
	char            short_name;    /* 'n' or '\0' if no short form */
	const char     *description;
	const char     *metavar;       /* placeholder for help text, e.g. "FILE" */
	ArgOptionType   type;
	bool            required;
	const char     *default_val;   /* string representation of default */
	const char     *env_var;       /* environment variable fallback, e.g. "EDITOR" */
	int             exclusive_group; /* 0 = none, >0 = group ID */

	/* Storage pointers — caller owns the memory */
	void           *storage;       /* bool*, int*, or char** */

	/* Internal: set by parser to track if option was explicitly provided */
	bool            was_set;
} ArgOption;

/* ── Command callback ───────────────────────────────────────────────────────── */

typedef struct ArgParseResult ArgParseResult;

typedef int (*arg_callback)(const ArgParseResult *result);

/* ── Command ────────────────────────────────────────────────────────────────── */

typedef struct ArgCommand {
	const char     *name;
	const char     *description;
	arg_callback    callback;

	ArgOption       options[ARGPARSE_MAX_OPTIONS];
	int             option_count;

	/* Positional argument names (for help display) */
	const char     *positionals[ARGPARSE_MAX_POSITIONAL];
	int             positional_count;
	bool            takes_rest;     /* remaining args go to "args" */

	/* Aliases (e.g., "st" for "status") */
	const char     *aliases[ARGPARSE_MAX_ALIASES];
	int             alias_count;

	/* Nested subcommands */
	struct ArgCommand *subcommands[ARGPARSE_MAX_COMMANDS];
	int                subcommand_count;
	struct ArgCommand *parent;      /* NULL for top-level commands */
} ArgCommand;

/* ── Parser configuration (all program metadata in one place) ──────────────── */

typedef struct ArgParserConfig {
	const char     *prog_name;     /* required: program name */
	const char     *version;       /* required: version string */
	const char     *description;   /* one-line description */
	const char     *bug_url;       /* "Report bugs to: <url>" */
	const char     *author;        /* "Author: <name>" */
} ArgParserConfig;

/* ── Parser ─────────────────────────────────────────────────────────────────── */

typedef struct ArgParser {
	const char     *prog_name;
	const char     *version;
	const char     *description;
	const char     *bug_url;
	const char     *author;

	ArgCommand      root;           /* root command (global options) */
	ArgCommand      commands[ARGPARSE_MAX_COMMANDS];
	int             command_count;

	/* Filled after parse */
	ArgCommand     *matched_command;
	ArgCommand     *matched_subcommand; /* deepest matched subcommand */
	int             argc;
	char          **argv;
} ArgParser;

/* ── Parse result ───────────────────────────────────────────────────────────── */

struct ArgParseResult {
	ArgParser      *parser;
	ArgCommand     *command;        /* deepest matched command */
	int             argc;
	char          **argv;

	/* Positional arguments */
	const char     *positionals[ARGPARSE_MAX_POSITIONAL];
	int             positional_count;

	/* Remaining args (after --) */
	const char     *rest[ARGPARSE_MAX_POSITIONAL];
	int             rest_count;
};

/* ── Public API ─────────────────────────────────────────────────────────────── */

/* Create / destroy */
ArgParser *argparse_new(const ArgParserConfig *config);
void       argparse_free(ArgParser *parser);

/* Set individual metadata fields (optional, if not using config) */
void argparse_set_description(ArgParser *parser, const char *desc);
void argparse_set_bug_url(ArgParser *parser, const char *url);
void argparse_set_author(ArgParser *parser, const char *author);

/* Add a top-level subcommand */
ArgCommand *argparse_add_command(ArgParser *parser,
                                 const char *name,
                                 const char *description,
                                 arg_callback callback);

/* Add a nested subcommand under a parent command */
ArgCommand *argparse_add_subcommand(ArgCommand *parent,
                                    const char *name,
                                    const char *description,
                                    arg_callback callback);

/* Set aliases for a command */
void argparse_command_set_aliases(ArgCommand *cmd,
                                  const char **aliases,
                                  int count);

/* Add an option to a command (use &parser->root for global options) */
void argparse_add_option(ArgCommand *command,
                          const char *long_name,
                          char        short_name,
                          ArgOptionType type,
                          const char *metavar,
                          const char *description,
                          void       *storage);

/* Add an option with environment variable fallback */
void argparse_add_option_with_env(ArgCommand *command,
                                   const char *long_name,
                                   char        short_name,
                                   ArgOptionType type,
                                   const char *metavar,
                                   const char *description,
                                   void       *storage,
                                   const char *env_var);

/* Add an option to a mutually exclusive group */
void argparse_add_option_exclusive(ArgCommand *command,
                                    const char *long_name,
                                    char        short_name,
                                    ArgOptionType type,
                                    const char *metavar,
                                    const char *description,
                                    void       *storage,
                                    int exclusive_group);

/* Mark a command's positional arg name (for help text) */
void argparse_add_positional(ArgCommand *command, const char *name);

/* Parse argv. Returns 0 on success, -1 on error, -2 for help, -3 for version. */
int argparse_parse(ArgParser *parser, int argc, char **argv);

/* Print help for a command (or global help if cmd is NULL) */
void argparse_help(const ArgParser *parser, const ArgCommand *cmd);

/* Generate shell completion candidates */
void argparse_complete(const ArgParser *parser, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif  /* _ARGPARSE_H_ */
