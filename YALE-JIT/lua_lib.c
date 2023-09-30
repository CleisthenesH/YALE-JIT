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
extern int text_entry_new(lua_State*);

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
    lua_pushcfunction(L, get_current_time);
    lua_setglobal(L, "current_time");

	lua_pushcfunction(L, button_new);
	lua_setglobal(L, "button");

	lua_pushcfunction(L, text_entry_new);
	lua_setglobal(L, "text_entry");
}