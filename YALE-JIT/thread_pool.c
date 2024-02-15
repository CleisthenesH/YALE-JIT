// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "thread_pool.h"
#include <allegro5/allegro.h>

#include <stdio.h>

struct work
{
	void (*funct)(void*);
	void* arg;
	struct work* next;
};

static struct work* work_create(void (*funct)(void*), void* arg)
{
	if (!funct)
		return NULL;

	struct work* const work = malloc(sizeof(struct work));

	*work = (struct work)
	{
		.funct = funct,
		.arg = arg,
		.next = NULL
	};

	return work;
}

struct work_queue
{
	struct work* first;
	struct work* last;
};

// Create an empty work queue
struct work_queue* work_queue_create()
{
	struct work_queue* const output = malloc(sizeof(struct work_queue));
	output->first = NULL;
	output->last = NULL;

	return output;
}

// Create and push a work object to the end of a work queue
void work_queue_push(struct work_queue* queue, void(*funct)(void*), void* arg)
{
	if (!queue)
		return;

	struct work* const work = work_create(funct, arg);

	if (!work)
		return;

	if (queue->first)
		queue->last->next = work;
	else
		queue->first = work;

	queue->last = work;
}

// Pop a work object from the queue
static struct work* work_queue_pop(struct work_queue* queue)
{
	if (!queue || !queue->first)
		return NULL;

	struct work* const work = queue->first;

	if (!(queue->first = work->next))
		queue->last = NULL;

	return work;
}

// Free a work queue and all its constituent work objects.
// Not used in normal workflow as work queues are meant to concatenated to thread pools.
void work_queue_destroy(struct work_queue* queue)
{
	for (struct work* a = queue->first, *b; a; a = b)
	{
		b = a->next;
		free(a);
	}

	free(queue);
}

struct thread_pool
{
	struct work_queue queue;			// Current work queue

	ALLEGRO_MUTEX* read_write_mutex;	// Read/Write lock
	ALLEGRO_COND* pending_work_cond;	// Signal to threads that there is work to do.
	ALLEGRO_COND* idle_cond;			// Signal that there is no thread working.	

	size_t working_cnt;					// Number of workering threads
	size_t thread_cnt;					// Number of total threads

	bool shutting_down;					// The thread pool is signaled to be destroyed
};

static struct thread_pool thread_pool;

static void* worker_function(void* arg)
{
	while (1)
	{
		al_lock_mutex(thread_pool.read_write_mutex);

		while (!thread_pool.queue.first && !thread_pool.shutting_down)
			al_wait_cond(thread_pool.pending_work_cond, thread_pool.read_write_mutex);

		if (thread_pool.shutting_down)
		{
			thread_pool.thread_cnt--;
			al_signal_cond(thread_pool.idle_cond);
			al_unlock_mutex(thread_pool.read_write_mutex);

			return NULL;
		}

		struct work* const work = work_queue_pop(&thread_pool.queue);
		thread_pool.working_cnt++;
		al_unlock_mutex(thread_pool.read_write_mutex);

		if (work)
		{
			work->funct(work->arg);
			free(work);
		}

		al_lock_mutex(thread_pool.read_write_mutex);
		thread_pool.working_cnt--;

		if (!thread_pool.shutting_down && thread_pool.working_cnt == 0 && !thread_pool.queue.first)
			al_signal_cond(thread_pool.idle_cond);

		al_unlock_mutex(thread_pool.read_write_mutex);
	}
}

// Create and detach the thread pool
// Only visable to the main thread
void thread_pool_create(int thread_cnt)
{
	if (thread_cnt <= 0)
		thread_cnt = 8;

	thread_pool = (struct thread_pool)
	{
		.queue.first = NULL,
		.queue.last = NULL,

		.thread_cnt = thread_cnt,
		.working_cnt = 0,

		.shutting_down = false,

		.read_write_mutex = al_create_mutex(),
		.pending_work_cond = al_create_cond(),
		.idle_cond = al_create_cond()
	};

	for (int i = 0; i < thread_cnt; i++)
		al_run_detached_thread(worker_function, NULL);
}

// Destroy the thread pool.
// Destroys any work objects that are in the queue but not being executed.
// Only visable to the main thread.
void thread_pool_destroy()
{
	al_lock_mutex(thread_pool.read_write_mutex);

	work_queue_destroy(&thread_pool.queue);
	thread_pool.shutting_down = true;

	al_signal_cond(thread_pool.pending_work_cond);
	al_unlock_mutex(thread_pool.read_write_mutex);

	thread_pool_wait();

	al_destroy_mutex(thread_pool.read_write_mutex);
	al_destroy_cond(thread_pool.pending_work_cond);
	al_destroy_cond(thread_pool.idle_cond);
}

// Create and push a work object to the thread pool.
void thread_pool_push(void (*funct)(void*), void* arg)
{
	al_lock_mutex(thread_pool.read_write_mutex);
	work_queue_push(&thread_pool.queue, funct, arg);
	al_signal_cond(thread_pool.pending_work_cond);
	al_unlock_mutex(thread_pool.read_write_mutex);
}

// Wait for the queue to be empty and all workers be idle.
void thread_pool_wait()
{
	al_lock_mutex(thread_pool.read_write_mutex);

	while (1)
		if ((!thread_pool.shutting_down && thread_pool.queue.first) ||
			(thread_pool.shutting_down && thread_pool.thread_cnt != 0))
			al_wait_cond(thread_pool.idle_cond, thread_pool.read_write_mutex);
		else
			break;

	al_unlock_mutex(thread_pool.read_write_mutex);
}

// Concatenate a work queue on to the thread pool.
// Frees the non-work object memory associated with the queue.
// The work objects memory is now managed by the thread pool.
void thread_pool_concatenate(struct work_queue* queue)
{
	if (queue->first)
	{
		al_lock_mutex(thread_pool.read_write_mutex);

		if (thread_pool.queue.last)
		{
			thread_pool.queue.last->next = queue->first;
		}
		else
		{
			thread_pool.queue.first = queue->first;
		}

		thread_pool.queue.last = queue->last;

		al_signal_cond(thread_pool.pending_work_cond);
		al_unlock_mutex(thread_pool.read_write_mutex);
	}

	free(queue);
}