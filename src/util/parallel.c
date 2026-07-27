/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * parallel.c — Per-repo parallel collection implementation
 */

#include "parallel.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "share.h"
#include "thread_pool.h"


typedef struct {
	CollectFn        fn;
	const RepoEntry *entry;
	void            *result;
} CollectTask;


static void collect_wrapper(void *arg)
{
	CollectTask *t = arg;
	t->fn(t->entry, t->result);
}


size_t parallel_thread_count(void)
{
	const char *env = getenv("GITM_THREADS");
	if (env) {
		long n = strtol(env, NULL, 10);
		if (n >= 1 && n <= TP_MAX_THREADS)
			return (size_t) n;
	}

	long n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (n_cpus < 1)
		n_cpus = 1;

	size_t n = (size_t) n_cpus;
	if (n > 8)
		n = 8;

	return n;
}

/* Reusable thread pool — created once, resized as needed */
static ThreadPool *g_pool       = NULL;
static size_t      g_pool_size  = 0;

static ThreadPool *get_pool(size_t need)
{
	if (g_pool && g_pool_size >= need)
		return g_pool;

	if (g_pool) {
		tp_destroy(g_pool);
		g_pool = NULL;
		g_pool_size = 0;
	}

	g_pool = tp_create(need);
	if (g_pool)
		g_pool_size = need;
	return g_pool;
}


int parallel_collect(const GitConfig *cfg,
                     const size_t    *indices,
                     size_t           count,
                     CollectFn        collect,
                     size_t           result_size,
                     void            *results_out)
{
	if (count == 0)
		return 0;

	/* Clamp to MAX_REPOS to prevent stack overflow in tasks[] array */
	if (count > MAX_REPOS)
		count = MAX_REPOS;

	size_t n_threads = parallel_thread_count();
	if (n_threads > count)
		n_threads = count;

	LOG_DEBUG("parallel_collect: %zu repos across %zu threads", count, n_threads);

	ThreadPool *tp = get_pool(n_threads);
	if (!tp)
		return -1;

	/* Stack-allocated task descriptors (count <= MAX_REPOS) */
	CollectTask tasks[MAX_REPOS];

	for (size_t i = 0; i < count; i++) {
		tasks[i].fn     = collect;
		tasks[i].entry  = &cfg->entries[indices[i]];
		tasks[i].result = (char *) results_out + i * result_size;
		tp_submit(tp, collect_wrapper, &tasks[i]);
	}

	tp_wait(tp);

	LOG_DEBUG("parallel_collect: done");
	return 0;
}

void parallel_cleanup(void)
{
	if (g_pool) {
		tp_destroy(g_pool);
		g_pool      = NULL;
		g_pool_size = 0;
	}
}
