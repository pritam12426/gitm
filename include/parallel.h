/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * parallel.h — Per-repo parallel collection helper
 *
 * Runs a "collect" function for each filtered repo in parallel
 * using a thread pool, then returns. The caller iterates the
 * results array sequentially for display.
 *
 * Usage:
 *   StatusResult results[MAX_REPOS] = { 0 };
 *   parallel_collect(&cfg, indices, filtered,
 *                    status_collect, sizeof(StatusResult), results);
 *   for (size_t i = 0; i < filtered; i++)
 *       display_result(&results[i]);
 */

#ifndef _PARALLEL__H_
#define _PARALLEL__H_


#include <stddef.h>
#include <sys/types.h>

#include "config.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Per-repo collect callback (runs in worker thread).
 *   entry      — the RepoEntry to process (read-only)
 *   result_out — pointer to a command-specific result struct
 */
typedef void (*CollectFn)(const RepoEntry *entry, void *result_out);

/*
 * Get thread count from GITM_THREADS env var or default.
 * Default = min(sysconf(_SC_NPROCESSORS_ONLN), 8), clamped [1, 16].
 */
size_t parallel_thread_count(void);

/*
 * Run `collect(entry, &results[i])` in parallel for each index.
 *
 *   indices     — array of indices into cfg->entries
 *   count       — number of indices
 *   collect     — per-repo callback (called from worker threads)
 *   result_size — sizeof one result element
 *   results_out — caller-allocated buffer, >= count * result_size bytes
 *
 * Blocks until all collections complete. Returns 0 on success.
 */
int parallel_collect(const GitConfig *cfg,
                     const size_t *indices, size_t count,
                     CollectFn collect,
                     size_t result_size,
                     void *results_out);


#ifdef __cplusplus
}
#endif


#endif  /* _PARALLEL__H_ */
