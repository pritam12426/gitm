/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd.h — Command declarations
 */

#ifndef _CMD_H_
#define _CMD_H_


#include <stdbool.h>

#include "argparse.h"


/* Shared --table flag (set by argparse storage pointer) */
extern bool g_table_mode;

/* Register all commands on the parser */
void cmd_register_all(ArgParser *parser);

/* Register the --table flag on a command */
void cmd_register_table_flag(ArgCommand *cmd);


#endif /* _CMD_H_ */
