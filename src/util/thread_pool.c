/*
 * Copyright (c) 2026 Pritam
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * thread_pool.c — Fixed-size worker thread pool implementation
 */

#include "thread_pool.h"

#include <stdlib.h>
#include <string.h>


static void *worker_fn(void *arg)
{
	ThreadPool *tp = arg;

	pthread_mutex_lock(&tp->mutex);
	for (;;) {
		// Wait for work or shutdown
		while (tp->count == 0 && !tp->shutdown)
			pthread_cond_wait(&tp->has_task, &tp->mutex);

		if (tp->shutdown && tp->count == 0)
			break;

		// Dequeue one task
		tp_task_fn fn   = tp->queue[tp->head].fn;
		void      *farg = tp->queue[tp->head].arg;
		tp->head        = (tp->head + 1) % TP_MAX_TASKS;
		tp->count--;

		pthread_mutex_unlock(&tp->mutex);

		fn(farg);

		pthread_mutex_lock(&tp->mutex);
		tp->pending--;
		if (tp->pending == 0)
			pthread_cond_broadcast(&tp->all_done);
	}
	pthread_mutex_unlock(&tp->mutex);
	return NULL;
}


ThreadPool *tp_create(size_t n)
{
	ThreadPool *tp = calloc(1, sizeof(ThreadPool));
	if (!tp)
		return NULL;

	pthread_mutex_init(&tp->mutex, NULL);
	pthread_cond_init(&tp->has_task, NULL);
	pthread_cond_init(&tp->all_done, NULL);

	if (n < 1)
		n = 1;
	if (n > TP_MAX_THREADS)
		n = TP_MAX_THREADS;
	tp->thread_count = n;

	for (size_t i = 0; i < n; i++) {
		if (pthread_create(&tp->threads[i], NULL, worker_fn, tp) != 0) {
			// Signal existing workers to shut down
			pthread_mutex_lock(&tp->mutex);
			tp->shutdown = true;
			pthread_cond_broadcast(&tp->has_task);
			pthread_mutex_unlock(&tp->mutex);
			for (size_t j = 0; j < i; j++)
				pthread_join(tp->threads[j], NULL);
			pthread_mutex_destroy(&tp->mutex);
			pthread_cond_destroy(&tp->has_task);
			pthread_cond_destroy(&tp->all_done);
			free(tp);
			return NULL;
		}
	}

	return tp;
}


int tp_submit(ThreadPool *tp, tp_task_fn fn, void *arg)
{
	pthread_mutex_lock(&tp->mutex);

	if (tp->shutdown || tp->count >= TP_MAX_TASKS) {
		pthread_mutex_unlock(&tp->mutex);
		return -1;
	}

	tp->queue[tp->tail].fn  = fn;
	tp->queue[tp->tail].arg = arg;
	tp->tail                = (tp->tail + 1) % TP_MAX_TASKS;
	tp->count++;
	tp->pending++;

	pthread_cond_signal(&tp->has_task);
	pthread_mutex_unlock(&tp->mutex);
	return 0;
}


void tp_wait(ThreadPool *tp)
{
	pthread_mutex_lock(&tp->mutex);
	while (tp->pending > 0)
		pthread_cond_wait(&tp->all_done, &tp->mutex);
	pthread_mutex_unlock(&tp->mutex);
}


void tp_destroy(ThreadPool *tp)
{
	if (!tp)
		return;

	tp_wait(tp);

	pthread_mutex_lock(&tp->mutex);
	tp->shutdown = true;
	pthread_cond_broadcast(&tp->has_task);
	pthread_mutex_unlock(&tp->mutex);

	for (size_t i = 0; i < tp->thread_count; i++)
		pthread_join(tp->threads[i], NULL);

	pthread_mutex_destroy(&tp->mutex);
	pthread_cond_destroy(&tp->has_task);
	pthread_cond_destroy(&tp->all_done);
	free(tp);
}
