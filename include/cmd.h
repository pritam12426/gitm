/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd.h — Command declarations
 */

#ifndef _CMD__H_
#define _CMD__H_


#include <stdbool.h>

#include "argparse.h"


// Shared --table flag (set by argparse storage pointer)
extern bool g_table_mode;

// Register all commands on the parser
void cmd_register_all(ArgParser *parser);

// Register the --table flag on a command
void cmd_register_table_flag(ArgCommand *cmd);

// Individual command registration functions
void cmd_register_list(ArgParser *parser);
void cmd_register_add(ArgParser *parser);
void cmd_register_remove(ArgParser *parser);
void cmd_register_rename(ArgParser *parser);
void cmd_register_status(ArgParser *parser);
void cmd_register_info(ArgParser *parser);
void cmd_register_exec(ArgParser *parser);
void cmd_register_open(ArgParser *parser);
void cmd_register_doctor(ArgParser *parser);
void cmd_register_recent(ArgParser *parser);
void cmd_register_summary(ArgParser *parser);
void cmd_register_search(ArgParser *parser);
void cmd_register_list_tag(ArgParser *parser);
void cmd_register_remote(ArgParser *parser);
void cmd_register_last(ArgParser *parser);
void cmd_register_branch(ArgParser *parser);
void cmd_register_clean(ArgParser *parser);
void cmd_register_clone(ArgParser *parser);
void cmd_register_stale(ArgParser *parser);
void cmd_register_stash(ArgParser *parser);
void cmd_register_stats(ArgParser *parser);


#endif  // _CMD__H_
