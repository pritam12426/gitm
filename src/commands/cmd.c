/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * cmd.c — Command registration
 */

#include "cmd.h"

#include "log.h"

/* Registration functions (defined in each command file) */
extern void cmd_register_list(ArgParser *parser);
extern void cmd_register_add(ArgParser *parser);
extern void cmd_register_remove(ArgParser *parser);
extern void cmd_register_rename(ArgParser *parser);
extern void cmd_register_status(ArgParser *parser);
extern void cmd_register_info(ArgParser *parser);
extern void cmd_register_exec(ArgParser *parser);
extern void cmd_register_open(ArgParser *parser);
extern void cmd_register_doctor(ArgParser *parser);
extern void cmd_register_recent(ArgParser *parser);
extern void cmd_register_summary(ArgParser *parser);
extern void cmd_register_search(ArgParser *parser);
extern void cmd_register_list_tag(ArgParser *parser);
extern void cmd_register_remote(ArgParser *parser);
extern void cmd_register_last(ArgParser *parser);
extern void cmd_register_branch(ArgParser *parser);
extern void cmd_register_clean(ArgParser *parser);

void cmd_register_all(ArgParser *parser)
{
	LOG_TRACE("registering all commands");
	cmd_register_list(parser);
	cmd_register_add(parser);
	cmd_register_remove(parser);
	cmd_register_rename(parser);
	cmd_register_status(parser);
	cmd_register_info(parser);
	cmd_register_exec(parser);
	cmd_register_open(parser);
	cmd_register_doctor(parser);
	cmd_register_recent(parser);
	cmd_register_summary(parser);
	cmd_register_search(parser);
	cmd_register_list_tag(parser);
	cmd_register_remote(parser);
	cmd_register_last(parser);
	cmd_register_branch(parser);
	cmd_register_clean(parser);
	LOG_TRACE("all commands registered");
}
