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


#include "argparse.h"


/* Register all commands on the parser */
void cmd_register_all(ArgParser *parser);


#endif /* _CMD_H_ */
