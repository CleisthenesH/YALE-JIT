// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

extern double current_timestamp;
extern double delta_timestamp;

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

// Simple wrapper for allegro native file dialog.
static int native_file_dialog(lua_State* L)
{
	const char* title = NULL; 
	const char* inital_path = NULL;
	const char* patterns = NULL;
	int mode = 0;

	if (lua_type(L, -1) == LUA_TTABLE)
	{
		lua_pushnil(L);

		while (lua_next(L, -2))
		{
			if (lua_type(L, -1) != LUA_TSTRING)
			{
				lua_pop(L, 1);
				continue;
			}

			const char* value = lua_tostring(L, -1);
			
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				const char* key = lua_tostring(L, -2);

				if (!strcmp(key, "title"))
					title = value;
				else if (!strcmp(key, "inital_path"))
					inital_path = value;
				else if (!strcmp(key, "patterns"))
					patterns = value;

				lua_pop(L, 1);
				continue;
			}

			if (!strcmp(value, "file_must_exist"))
				mode |= ALLEGRO_FILECHOOSER_FILE_MUST_EXIST;
			else if (!strcmp(value, "save"))
				mode |= ALLEGRO_FILECHOOSER_SAVE;
			else if (!strcmp(value, "folder"))
				mode |= ALLEGRO_FILECHOOSER_FOLDER;
			else if (!strcmp(value, "pictures"))
				mode |= ALLEGRO_FILECHOOSER_PICTURES;
			else if (!strcmp(value, "show_hidden"))
				mode |= ALLEGRO_FILECHOOSER_SHOW_HIDDEN;
			else if (!strcmp(value, "multiple"))
				mode |= ALLEGRO_FILECHOOSER_MULTIPLE;

			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	ALLEGRO_FILECHOOSER* const dialog = al_create_native_file_dialog(
		inital_path, title, patterns, mode
	);

	al_show_native_file_dialog(al_get_current_display(), dialog);

	const int cnt = al_get_native_file_dialog_count(dialog);

	for (int i = 0; i < cnt; i++)
		lua_pushstring(L, al_get_native_file_dialog_path(dialog, i));

	al_destroy_native_file_dialog(dialog);

	return cnt;
}

// Set misc lua interface globals
void lua_openL_misc(lua_State* L)
{
	// Misc Functions
	lua_pushcfunction(L, get_current_time);
	lua_setglobal(L, "current_time");

	lua_pushcfunction(L, native_file_dialog);
	lua_setglobal(L, "native_file_dialog");
}