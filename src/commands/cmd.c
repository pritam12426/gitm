/*
 * cmd.c — Command registration
 */

#include "cmd.h"

/* Registration functions (defined in each command file) */
extern void cmd_register_list(ArgParser *parser);
extern void cmd_register_add(ArgParser *parser);
extern void cmd_register_remove(ArgParser *parser);
extern void cmd_register_rename(ArgParser *parser);
extern void cmd_register_status(ArgParser *parser);
extern void cmd_register_info(ArgParser *parser);
extern void cmd_register_exec(ArgParser *parser);
extern void cmd_register_clone(ArgParser *parser);
extern void cmd_register_open(ArgParser *parser);
extern void cmd_register_doctor(ArgParser *parser);

void cmd_register_all(ArgParser *parser)
{
	cmd_register_list(parser);
	cmd_register_add(parser);
	cmd_register_remove(parser);
	cmd_register_rename(parser);
	cmd_register_status(parser);
	cmd_register_info(parser);
	cmd_register_exec(parser);
	cmd_register_clone(parser);
	cmd_register_open(parser);
	cmd_register_doctor(parser);
}
