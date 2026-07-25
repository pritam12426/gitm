/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _ARGPARSE_INTERNAL__H_
#define _ARGPARSE_INTERNAL__H_


#include "argparse.h"

/* Shared between argparse.c and completion.c */
ArgCommand *match_command(ArgParser *parser, const char *name);
ArgCommand *match_subcommand(ArgCommand *parent, const char *name);

/* Called from parse_tokens on --shell-completion */
void shell_completion(const ArgParser *parser, const char *shell);


#endif  // _ARGPARSE_INTERNAL__H_
