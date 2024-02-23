// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// TODO: Add some type of batching of work based on how much work there is to do to people to do it

#pragma once

struct work_queue;

struct work_queue* work_queue_create();
void work_queue_push(struct work_queue*, void(*)(void*), void*);
void work_queue_concatenate(struct work_queue*, struct work_queue*);
void work_queue_destroy(struct work_queue*);
void work_queue_run(struct work_queue*);

void thread_pool_push(void (*)(void*), void*);
void thread_pool_concatenate(struct work_queue*);
void thread_pool_wait();
