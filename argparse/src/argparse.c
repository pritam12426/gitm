/*
 * argparse.c — Core argument parser
 *
 * Ties together lexer, parser, help, and error modules.
 * Manages the command tree and performs the actual parsing.
 */

#include "argparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"

/* ── Constructor / destructor ────────────────────────────────────────────────
 */

ArgParser *argparse_new(const char *prog_name, const char *version)
{
	ArgParser *p = calloc(1, sizeof(ArgParser));
	if (!p)
		return NULL;

	p->prog_name = prog_name;
	p->version   = version;

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

void argparse_set_description(ArgParser *parser, const char *desc)
{
	if (parser)
		parser->description = desc;
}

/* ── Building the command tree ───────────────────────────────────────────────
 */

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

	return cmd;
}

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

	ArgOption *opt   = &command->options[command->option_count++];
	opt->long_name   = long_name;
	opt->short_name  = short_name;
	opt->type        = type;
	opt->metavar     = metavar;
	opt->description = description;
	opt->storage     = storage;
}

void argparse_add_positional(ArgCommand *command, const char *name)
{
	if (!command || command->positional_count >= ARGPARSE_MAX_POSITIONAL)
		return;

	command->positionals[command->positional_count++] = name;
}

/* ── Option lookup ───────────────────────────────────────────────────────────
 */

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

/* ── Token processing ────────────────────────────────────────────────────────
 */

static int parse_tokens(ArgParser *parser, ArgCommand *cmd, Lexer *lex, ArgParseResult *result)
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

			ArgOption *opt = find_option_long(cmd, key);
			if (!opt) {
				/* Collect known option names for suggestion */
				const char *known[ARGPARSE_MAX_OPTIONS];
				int         known_count = 0;
				for (int i = 0; i < cmd->option_count; i++) {
					known[known_count++] = cmd->options[i].long_name;
				}
				arg_error_unknown_option(program, tok.value, known, known_count);
				return -1;
			}

			/* Store value based on type */
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
					/* Need next token as value */
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

				/* Special: -h */
				if (c == 'h') {
					argparse_help(parser, cmd);
					return -2;
				}

				/* Special: -v */
				if (c == 'v') {
					fprintf(stderr, "%s %s\n", parser->prog_name, parser->version);
					return -3;
				}

				ArgOption *opt = find_option_short(cmd, c);
				if (!opt) {
					char        short_str[3] = { '-', c, '\0' };
					const char *known[ARGPARSE_MAX_OPTIONS];
					int         known_count = 0;
					for (int j = 0; j < cmd->option_count; j++) {
						char buf[4]          = { '-', cmd->options[j].short_name, '\0' };
						known[known_count++] = cmd->options[j].long_name ? cmd->options[j].long_name
						                                                 : buf;
					}
					arg_error_unknown_option(program, short_str, known, known_count);
					return -1;
				}

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
					/* If more chars remain in combined flags, they are the value */
					if (flags[i + 1] != '\0') {
						if (opt->storage) {
							if (opt->type == ARG_TYPE_INT)
								*(int *) opt->storage = atoi(&flags[i + 1]);
							else
								*(const char **) opt->storage = &flags[i + 1];
						}
						i = strlen(flags); /* consume rest */
					} else {
						/* Need next token */
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

/* ── Main parse entry point ──────────────────────────────────────────────────
 */

int argparse_parse(ArgParser *parser, int argc, char **argv)
{
	if (!parser || argc < 1)
		return -1;

	Lexer lex;
	lexer_init(&lex, argc, argv);

	/* Skip argv[0] (program name) */
	lexer_next(&lex);

	/* Peek at first token to see if it's a subcommand */
	ArgCommand *matched   = NULL;
	int         saved_pos = lex.pos;
	Token       peek      = lexer_next(&lex);
	lex.pos               = saved_pos;

	if (peek.type == TOKEN_POSITIONAL || peek.type == TOKEN_COMMAND) {
		for (int i = 0; i < parser->command_count; i++) {
			if (strcmp(parser->commands[i].name, peek.value) == 0) {
				matched = &parser->commands[i];
				/* Consume the command token */
				lexer_next(&lex);
				break;
			}
		}
	}

	if (!matched)
		matched = &parser->root;

	/* Build result */
	ArgParseResult result = { 0 };
	result.parser         = parser;
	result.command        = matched;
	result.argc           = argc;
	result.argv           = argv;

	/* Parse options and positionals */
	int rc = parse_tokens(parser, matched, &lex, &result);

	if (rc == -2) /* help was printed */
		return 0;
	if (rc == -3) /* version was printed */
		return 0;
	if (rc != 0)
		return -1;

	/* Store result in parser for the callback */
	parser->matched_command = matched;
	parser->argc            = argc;
	parser->argv            = argv;

	/* Validate required positional args */
	if (matched->callback && matched->positional_count > 0) {
		/* Check if enough positionals were provided */
		for (int i = 0; i < matched->positional_count; i++) {
			/* We only enforce if the positional is named and not optional */
			/* For now, be lenient — commands can check themselves */
			(void) i;
		}
	}

	/* Execute callback */
	if (matched->callback) {
		/* Copy result into a heap-allocated version the callback can use */
		ArgParseResult *heap_result = malloc(sizeof(ArgParseResult));
		if (!heap_result)
			return -1;
		memcpy(heap_result, &result, sizeof(ArgParseResult));

		int cb_rc = matched->callback(heap_result);
		free(heap_result);
		return cb_rc;
	}

	return 0;
}
