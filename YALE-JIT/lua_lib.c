// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

extern double current_timestamp;
extern double delta_timestamp;

extern int button_new(lua_State*);
extern const struct wg_jumptable_base button_jumptable;

extern int text_entry_new(lua_State*);
extern const struct wg_jumptable_base text_entry_jumptable;

extern int tile_new(lua_State*);
extern const struct wg_jumptable_zone tile_jumptable;

extern int meeple_new(lua_State*);
extern const struct wg_jumptable_piece meeple_jumptable;

// Simple stack dump for debugging
void stack_dump(lua_State* L)
{
	int top = lua_gettop(L);

	printf("Stack Dump (%d):\n", top);

	for (int i = 1; i <= top; i++) {
		printf("\t%d\t%s\t", i, lua_typename(L, lua_type(L, i)));
		switch (lua_type(L, i)) {
		case LUA_TNUMBER:
			printf("\t%g\n", lua_tonumber(L, i));
			break;
		case LUA_TSTRING:
			printf("\t%s\n", lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			printf("\t%s\n", (lua_toboolean(L, i) ? "true" : "false"));
			break;
		case LUA_TNIL:
			printf("\t%s\n", "nil");
			break;
		default:
			printf("\t%p\n", lua_topointer(L, i));
			break;
		}
	}
}

// Return the current time and delta
static int get_current_time(lua_State* L)
{
    lua_pushnumber(L, current_timestamp);
    lua_pushnumber(L, delta_timestamp);

    return 2;
}

// Set misc lua interface globals
void lua_openL_misc(lua_State* L)
{
	// Widget Initalizers
	lua_register(L, "button", button_new);
	lua_pushlightuserdata(L, &button_jumptable);
	lua_setglobal(L, "type_button");

	lua_register(L, "text_entry", text_entry_new);
	lua_pushlightuserdata(L, &text_entry_jumptable);
	lua_setglobal(L, "type_text_entry");

	lua_register(L, "tile", tile_new);
	lua_pushlightuserdata(L, &tile_jumptable);
	lua_setglobal(L, "type_tile");

	lua_register(L, "meeple", meeple_new);
	lua_pushlightuserdata(L, &meeple_jumptable);
	lua_setglobal(L, "type_meeple");

	// Misc Functions
	lua_pushcfunction(L, get_current_time);
	lua_setglobal(L, "current_time");
}