// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// #define SCHEDULER_TESTING

#include "scheduler.h"

#include <stdlib.h>

#include "allegro5/allegro.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

//#define SCHEDULER_TESTING
#ifdef SCHEDULER_TESTING
void stack_dump(lua_State*);
#endif

void stack_dump(lua_State*);

struct scheduler_interface
{
	double timestamp;
	void (*funct)(void*);
	void* data;
};

static struct scheduler_interface** heap;
static size_t allocated;
static size_t used;

static ALLEGRO_EVENT_SOURCE scheduler_event_source;

extern double current_timestamp;
extern lua_State* lua_state;

// Heap Operations

// Get a node's children
static inline void heap_children(size_t parent, size_t* a, size_t* b)
{
	*a = 2 * parent + 1;
	*b = 2 * parent + 2;
}

// Get a node's parent
static inline size_t heap_parent(size_t child)
{
	return (child - 1) >> 1;
}

// Swap two nodes, doesn't maintain the heap property
static inline void heap_swap(size_t i, size_t j)
{
	struct scheduler_interface* tmp;

	tmp = heap[i];
	heap[i] = heap[j];
	heap[j] = tmp;
}

// The node has potentially decreased, maintain the heap property by moving it up
static inline void heap_heapify_up(size_t node)
{
	while (node > 0)
	{
		size_t parent = heap_parent(node);

		if (heap[node]->timestamp > heap[parent]->timestamp)
			break;

		heap_swap(node, parent);
		node = parent;
	}
}

// The node has potentially increased, maintain the heap property by moving it down
static inline void heap_heapify_down(size_t node)
{
	size_t parent, child_left, child_right;

	parent = node;
	heap_children(parent, &child_left, &child_right);

	while (child_right <= used - 1)
	{
		size_t child_min = heap[child_left]->timestamp < heap[child_right]->timestamp ? child_left : child_right;

		if (heap[child_min]->timestamp > heap[parent]->timestamp)
			return;

		heap_swap(child_min, parent);
		parent = child_min;
		heap_children(parent, &child_left, &child_right);
	}

	if (child_left == used - 1 && heap[child_left]->timestamp < heap[parent]->timestamp)
		heap_swap(child_left, parent);

}

// The node has potentially changed, maintain the heap property by moving it
static inline void heap_heapify(size_t node)
{
	if (node == 0)
	{
		heap_heapify_down(node);
		return;
	}

	const size_t parent = heap_parent(node);

	if (heap[parent]->timestamp > heap[node]->timestamp)
	{
		heap_heapify_up(node);
		return;
	}

	heap_heapify_down(node);
}

// Insert an item into the heap
static inline void heap_insert(struct scheduler_interface* item)
{
	heap[used++] = item;

	heap_heapify_up(used - 1);
}

// Remove and item from the heap, it will be located at heap[used]
static inline void heap_remove(size_t node)
{
	heap_swap(node, --used);

	// TODO: We could check the timestamp change here and call heap_heapify_{left,right} directly.
	// Saving comparison in heap_heapify.
	heap_heapify(node);
}

// Remove the minimum item from the heap, it will be located at heap[used]
static inline void heap_pop()
{
	if (used <= 1)
	{
		used--;
		return;
	}

	heap_swap(0, --used);

	heap_heapify_down(0);
}

// Find and item in the heap
static size_t heap_find(size_t node, struct scheduler_interface* item)
{
	if (item == heap[node])
		return node;

	if (item->timestamp < heap[node]->timestamp)
		return 0;

	size_t child_left, child_right;

	heap_children(node, &child_left, &child_right);

	if (child_right >= used)
		return 0;

	if (child_left == used - 1)
		return heap[child_left] == item ? child_left : 0;

	const size_t idx = heap_find(child_left, item);

	if (idx)
		return idx;

	return heap_find(child_right, item);
}

#ifdef SCHEDULER_TESTING
static void scheduler_dump()
{
	for (size_t i = 0; i < used; i++)
	{
		struct scheduler_interface* item = heap[i];
		printf("%zu\t%lf\t%p\t%p\n", i, item->timestamp, item->funct, item->data);
	}
}
#endif

// Lua Interface

struct scheduler_item_lua
{
	struct scheduler_interface* scheduler_interface;
};

static void scheduler_call_wapper(void* data)
{
	const struct scheduler_item_lua* item = (struct scheduler_item_lua*)data;

	lua_getglobal(lua_state, "scheduler");
	lua_pushlightuserdata(lua_state, item);
	lua_gettable(lua_state, -2);
	
	lua_getfenv(lua_state, -1);
	lua_getfield(lua_state, -1, "callback");

	lua_call(lua_state, 0, 0);

	lua_pushlightuserdata(lua_state, item);
	lua_pushnil(lua_state);
	lua_settable(lua_state, -5);

	lua_pop(lua_state, 3);
}

static int scheduler_push_lua(lua_State* L)
{
	const double timestamp = luaL_checknumber(L, -2);

	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}

	struct scheduler_item_lua* item = lua_newuserdata(L, sizeof(struct scheduler_item_lua));

	if (!item)
	{
		lua_pop(L, 2);
		return 0;
	}

	lua_createtable(L, 1, 0);
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "callback");
	lua_setfenv(L, -2);

	item->scheduler_interface = scheduler_push(timestamp, scheduler_call_wapper, item);

	// Set metatable
	luaL_getmetatable(L, "scheule_item_mt");
	lua_setmetatable(L, -2);

	// Add to scheduuler table
	lua_getglobal(L, "scheduler");
	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, -3);
	lua_settable(L, -3);

	lua_pop(L, 1);
	lua_replace(L, -3);
	lua_pop(L, 1);

	return 1;
}

static int scheduler_remove_lua(lua_State* L)
{
	const struct scheduler_item_lua* item = (struct scheduler_item_lua*)luaL_checkudata(L, -1, "scheule_item_mt");
	scheduler_pop(item->scheduler_interface);

	lua_getglobal(L, "scheduler");
	lua_pushlightuserdata(L, item);
	lua_pushnil(L);
	lua_settable(L, -3);

	lua_pop(L, 2);

	return 0;
}

static int scheduler_change_lua(lua_State* L)
{
	struct scheduler_item_lua* item = (struct scheduler_item_lua*)luaL_checkudata(L, -2, "scheule_item_mt");
	const double time = luaL_checknumber(L, -1);

	scheduler_change_timestamp(item->scheduler_interface, time, 0);

	lua_pop(L, 2);

	return 0;
}


// Private Scheduler Interface

// Free a schedyler node, doesn't maintain heap property
static inline void scheduler_free_node(size_t node)
{
	free(heap[node]);
	heap[node] = NULL;
}

#ifdef SCHEDULER_TESTING
static void test(void* _)
{
	printf("%lf\n", current_timestamp);
}
#endif

// Initalize the scheduler
ALLEGRO_EVENT_SOURCE* scheduler_init()
{
	heap = malloc(sizeof(struct scheduler_interface*));

	if (!heap)
	{
		printf("Unable to initalize scheduler heap\n");
		return NULL;
	}

	heap[0] = NULL;

	allocated = 1;
	used = 0;

	lua_newtable(lua_state);

	// Set metatable
	lua_newtable(lua_state);

	lua_pushvalue(lua_state, -1);
	lua_pushcfunction(lua_state, scheduler_push_lua);
	lua_setfield(lua_state, -2, "push");
	lua_register(lua_state, "push", scheduler_push_lua);
	lua_setfield(lua_state, -2, "__index");

	// Set mode
	lua_pushstring(lua_state, "k");
	lua_setfield(lua_state, -2, "__mode");

	lua_setmetatable(lua_state, -2);

	lua_setglobal(lua_state, "scheduler");

	// Make schedule item metable
	luaL_newmetatable(lua_state, "scheule_item_mt");
	lua_pushvalue(lua_state, -1);
	lua_setfield(lua_state, -2, "__index");
	lua_pushcfunction(lua_state, scheduler_remove_lua);
	lua_setfield(lua_state, -2, "remove");
	lua_pushcfunction(lua_state, scheduler_change_lua);
	lua_setfield(lua_state, -2, "change");

#ifdef SCHEDULER_TESTING
	printf("Stack after scheduler init: ");
	stack_dump(lua_state);

	struct scheduler_interface* handle = scheduler_push(2, test, NULL);
	scheduler_push(4, test, NULL);
	scheduler_push(6, test, NULL);

	scheduler_change_timestamp(handle, 10, 0);
#endif

	al_init_user_event_source(&scheduler_event_source);

	return &scheduler_event_source;
}

// Public Scheduler Interface

// Push an item into the scheduler, maintains the heap property
struct scheduler_interface* scheduler_push(double timestamp, void(*funct)(void*), void* data)
{
	if (allocated <= used)
	{
		const size_t new_cnt = 2 * allocated + 1;

		struct scheduler_interface** memsafe_hande = realloc(heap, new_cnt * sizeof(struct scheduler_interface*));

		if (!memsafe_hande)
			return NULL;

		heap = memsafe_hande;
		allocated = new_cnt;
	}

	struct scheduler_interface* item = malloc(sizeof(struct scheduler_interface));

	if (!item)
		return NULL;

	*item = (struct scheduler_interface)
	{
		.timestamp = timestamp + current_timestamp,
		.funct = funct,
		.data = data
	};

	heap_insert(item);

	return item;
}

// Remove an item from the scheudler, maintains the heap property
void scheduler_pop(struct scheduler_interface* item)
{
	const size_t node = heap_find(0, item);

	if (node == 0)
	{
		if (used >= 1 && item == heap[0])
		{
			scheduler_free_node(0);
			heap_pop();
		}

		return;
	}

	heap_remove(node);
	scheduler_free_node(used);
}

// Change the timestap of an item, maintains the heap property
void scheduler_change_timestamp(struct scheduler_interface* item, double time, int flag)
{
	const size_t node = heap_find(0, item);

	if (node == 0 && (used == 0 || item != heap[0]))
		return;

	item->timestamp += time;
	heap_heapify(node);
}

// Check the current timers and generate any relevent events
void scheduler_generate_events()
{
	const double _current_time = al_current_time();

#ifdef SCHEDULER_TESTING
	printf("Scheduler_event: time %f %zd\n", _current_time, used);
	scheduler_dump();
#endif

	while (heap[0] && heap[0]->timestamp < _current_time)
	{
		ALLEGRO_EVENT ev;
		ev.type = ALLEGRO_GET_EVENT_TYPE('T', 'I', 'M', 'E');
		ev.user.data1 = (intptr_t)heap[0]->funct;
		ev.user.data2 = (intptr_t)heap[0]->data;

		al_emit_user_event(&scheduler_event_source, &ev, NULL);

		scheduler_free_node(0);

		heap_pop();
	}

#ifdef SCHEDULER_TESTING
	printf("\n");
#endif
}
