/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * error.h — Error reporting for argparse
 */

#ifndef _ARGPARSE_ERROR_H_
#define _ARGPARSE_ERROR_H_


void arg_error_unknown_option(const char        *program,
                              const char        *option,
                              const char *const *known,
                              int                known_count);

void arg_error_missing_value(const char *program, const char *option);

void arg_error_missing_argument(const char *program, const char *arg_name);

void arg_error_unknown_command(const char        *program,
                               const char        *command,
                               const char *const *known,
                               int                known_count);


#endif /* _ARGPARSE_ERROR_H_ */
