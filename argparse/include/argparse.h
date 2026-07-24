/*
 * argparse.h — Custom argument parser with subcommand support
 *
 * A lightweight, zero-dependency CLI argument parser written in C17.
 * Supports nested subcommands, short/long options, auto-help, and
 * automatic version output.
 */

#ifndef _ARGPARSE_H_
#define _ARGPARSE_H_


#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


/* ── Limits ─────────────────────────────────────────────────────────────────── */

#define ARGPARSE_MAX_OPTIONS     32
#define ARGPARSE_MAX_COMMANDS    32
#define ARGPARSE_MAX_POSITIONAL   8
#define ARGPARSE_MAX_LINE_LEN   1024


/* ── Option types ───────────────────────────────────────────────────────────── */

typedef enum {
	ARG_TYPE_NONE,     /* boolean flag (no value) */
	ARG_TYPE_STRING,   /* takes a string value */
	ARG_TYPE_INT,      /* takes an integer value */
	ARG_TYPE_COUNT,    /* increments a counter */
} ArgOptionType;


/* ── Option ─────────────────────────────────────────────────────────────────── */

typedef struct ArgOption {
	const char     *long_name;   /* "--name" (without --) */
	char            short_name;  /* 'n' or '\0' if no short form */
	const char     *description;
	const char     *metavar;     /* placeholder for help text, e.g. "FILE" */
	ArgOptionType   type;
	bool            required;
	const char     *default_val; /* string representation of default */

	/* Storage pointers — caller owns the memory */
	void           *storage;     /* bool*, int*, or char** */
} ArgOption;


/* ── Command callback ───────────────────────────────────────────────────────── */

/* Forward declaration */
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
	bool            takes_rest;   /* if true, remaining args go to "args" */
} ArgCommand;


/* ── Parser ─────────────────────────────────────────────────────────────────── */

typedef struct ArgParser {
	const char     *prog_name;
	const char     *version;
	const char     *description;

	ArgCommand      root;              /* root command (global options) */
	ArgCommand      commands[ARGPARSE_MAX_COMMANDS];
	int             command_count;

	/* Filled after parse */
	ArgCommand     *matched_command;   /* NULL if no subcommand matched */
	int             argc;
	char          **argv;
} ArgParser;


/* ── Parse result ───────────────────────────────────────────────────────────── */

struct ArgParseResult {
	ArgParser      *parser;
	ArgCommand     *command;          /* which command was invoked */
	int             argc;
	char          **argv;

	/* Positional arguments */
	const char     *positionals[ARGPARSE_MAX_POSITIONAL];
	int             positional_count;

	/* Remaining args (after --) */
	const char     *args[ARGPARSE_MAX_POSITIONAL];
	int             args_count;
};


/* ── Public API ─────────────────────────────────────────────────────────────── */

/* Create / destroy */
ArgParser *argparse_new(const char *prog_name, const char *version);
void       argparse_free(ArgParser *parser);

/* Set description (shown in global --help) */
void argparse_set_description(ArgParser *parser, const char *desc);

/* Add a subcommand */
ArgCommand *argparse_add_command(ArgParser *parser,
                                 const char *name,
                                 const char *description,
                                 arg_callback callback);

/* Add an option to a command (use &parser->root for global options) */
void argparse_add_option(ArgCommand *command,
                          const char *long_name,
                          char        short_name,
                          ArgOptionType type,
                          const char *metavar,
                          const char *description,
                          void       *storage);

/* Mark a command's positional arg name (for help text) */
void argparse_add_positional(ArgCommand *command, const char *name);

/* Parse argv. Returns 0 on success, -1 on error. */
int argparse_parse(ArgParser *parser, int argc, char **argv);

/* Print help for a command (or global help if cmd is NULL) */
void argparse_help(const ArgParser *parser, const ArgCommand *cmd);


#ifdef __cplusplus
}
#endif


#endif  /* _ARGPARSE_H_ */
