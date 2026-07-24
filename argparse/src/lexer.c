/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * lexer.c — Tokenizer for argv
 *
 * Converts argv into a stream of tokens:
 *   "--option"  → TOKEN_LONG_OPTION
 *   "-o"        → TOKEN_SHORT_OPTION
 *   "--key=val" → TOKEN_LONG_OPTION (split at '=' done by caller)
 *   "-"         → TOKEN_POSITIONAL
 *   "--"        → stop options, rest are TOKEN_POSITIONAL
 *   anything else → TOKEN_POSITIONAL
 */

#include "lexer.h"

#include <string.h>

void lexer_init(Lexer *lex, int argc, char **argv)
{
	lex->argv           = (const char **) argv;
	lex->argc           = argc;
	lex->pos            = 0;
	lex->stop_options   = false;
}

Token lexer_next(Lexer *lex)
{
	Token tok = { TOKEN_END, NULL };

	if (lex->pos >= lex->argc)
		return tok;

	const char *arg = lex->argv[lex->pos++];

	/* "--" terminates option parsing — rest are positional */
	if (strcmp(arg, "--") == 0) {
		lex->stop_options = true;
		/* Consume and return the next arg as the first positional, if any */
		if (lex->pos < lex->argc) {
			tok.type  = TOKEN_POSITIONAL;
			tok.value = lex->argv[lex->pos++];
			return tok;
		}
		/* "--" at end of args */
		return tok;
	}

	/* After "--", everything is positional */
	if (lex->stop_options) {
		tok.type  = TOKEN_POSITIONAL;
		tok.value = arg;
		return tok;
	}

	/* Long option: "--something" */
	if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
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
