/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * lexer.h — Tokenizer for argv
 */

#ifndef _LEXER_H_
#define _LEXER_H_

#include <stdbool.h>

typedef enum {
	TOKEN_COMMAND,
	TOKEN_SHORT_OPTION,
	TOKEN_LONG_OPTION,
	TOKEN_VALUE,
	TOKEN_POSITIONAL,
	TOKEN_END,
	TOKEN_ERROR,
} TokenType;

typedef struct {
	TokenType   type;
	const char *value;
} Token;

typedef struct {
	const char **argv;
	int          argc;
	int          pos;
	bool         stop_options; /* set after --: remaining are positional */
} Lexer;

void lexer_init(Lexer *lex, int argc, char **argv);
Token lexer_next(Lexer *lex);


#endif  // _LEXER_H_
