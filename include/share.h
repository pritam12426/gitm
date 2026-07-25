/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * share.h — Shared constants
 */

#ifndef _SHARE_H_
#define _SHARE_H_


/* Tag and group limits */
#define MAX_TAGS   10
#define MAX_GROUPS 10

/* Buffer sizes */
#define MAX_PATH_LEN   512
#define MAX_NAME_LEN   256
#define MAX_COUNT_STR  32

/* Git */
#define GIT_BINARY     "git"
#define GIT_MAX_ARGS   32

/* Process capture */
#define PROCESS_BUF_SIZE 4096

/* Stats frequency map */
#define FREQ_MAP_MAX   512

/* Repeated user-facing messages */
#define MSG_NO_REPOS       "No repositories registered.\n"
#define MSG_CFG_PATH_ERR   "could not determine config path"
#define MSG_CFG_LOAD_ERR   "could not load config"
#define MSG_CFG_SAVE_ERR   "could not save config"


#endif  // _SHARE_H_
