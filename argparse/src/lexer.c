/*
 * lexer.c — Tokenizer for argv
 *
 * Converts argv into a stream of tokens:
 *   "--option"  → TOKEN_LONG_OPTION
 *   "-o"        → TOKEN_SHORT_OPTION
 *   "--key=val" → TOKEN_LONG_OPTION + TOKEN_VALUE (split at '=')
 *   "-"         → TOKEN_POSITIONAL
 *   "--"        → stop, rest are TOKEN_POSITIONAL
 *   anything else → TOKEN_POSITIONAL (or TOKEN_COMMAND if first)
 */

#include "lexer.h"

#include <string.h>

void lexer_init(Lexer *lex, int argc, char **argv)
{
	lex->argv = (const char **) argv;
	lex->argc = argc;
	lex->pos  = 0;
}

Token lexer_next(Lexer *lex)
{
	Token tok = { TOKEN_END, NULL };

	if (lex->pos >= lex->argc)
		return tok;

	const char *arg = lex->argv[lex->pos++];

	/* "--" terminates option parsing */
	if (strcmp(arg, "--") == 0) {
		/* Mark all remaining as positional */
		while (lex->pos < lex->argc) {
			lex->pos++;
		}
		/* Return the last real arg as positional */
		tok.type  = TOKEN_POSITIONAL;
		tok.value = arg;
		return tok;
	}

	/* Long option: "--something" */
	if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
		/* Check for "--key=value" */
		const char *eq = strchr(arg + 2, '=');
		if (eq != NULL) {
			/* Return the option part; caller splits on '=' */
			tok.type  = TOKEN_LONG_OPTION;
			tok.value = arg;
			return tok;
		}
		tok.type  = TOKEN_LONG_OPTION;
		tok.value = arg;
		return tok;
	}

	/* Short option: "-x" or combined "-abc" */
	if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-') {
		tok.type  = TOKEN_SHORT_OPTION;
		tok.value = arg;
		return tok;
	}

	/* Everything else is positional */
	tok.type  = TOKEN_POSITIONAL;
	tok.value = arg;
	return tok;
}
