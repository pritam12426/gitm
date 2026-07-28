/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * thread_pool.h — Fixed-size worker thread pool
 *
 * Generic task queue: submit void(*)(void*) callbacks, wait for completion.
 * Zero third-party dependencies. Requires pthreads.
 *
 * Usage:
 *   ThreadPool *tp = tp_create(8);
 *   for (int i = 0; i < n; i++)
 *       tp_submit(tp, worker_fn, &task_args[i]);
 *   tp_wait(tp);
 *   tp_destroy(tp);
 */

#ifndef _THREAD_POOL__H_
#define _THREAD_POOL__H_


#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


#define TP_MAX_THREADS 16
#define TP_MAX_TASKS   64

typedef void (*tp_task_fn)(void *arg);

typedef struct {
	pthread_t threads[TP_MAX_THREADS];
	size_t    thread_count;

	// Ring buffer task queue
	struct {
		tp_task_fn fn;
		void      *arg;
	} queue[TP_MAX_TASKS];

	size_t head;
	size_t tail;
	size_t count;

	pthread_mutex_t mutex;
	pthread_cond_t  has_task;
	pthread_cond_t  all_done;

	size_t pending;
	bool   shutdown;
} ThreadPool;


/*
 * Create a pool with `n` worker threads (clamped to [1, TP_MAX_THREADS]).
 * Returns NULL on failure.
 */
ThreadPool *tp_create(size_t n);

/*
 * Submit a task. Returns 0 on success, -1 if the pool is full or shut down.
 */
int tp_submit(ThreadPool *tp, tp_task_fn fn, void *arg);

/*
 * Block until every submitted task has completed.
 */
void tp_wait(ThreadPool *tp);

/*
 * Signal workers to exit, join all threads, free memory.
 * Implicitly waits for pending tasks first.
 */
void tp_destroy(ThreadPool *tp);


#ifdef __cplusplus
}
#endif


#endif // _THREAD_POOL__H_
