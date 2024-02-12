// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "resource_manager.h"
#include "meeple_tile_utility.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// 
enum TEAMS  lua_toteam(lua_State* L, int idx, struct tile* tile)
{
	enum TEAMS output = TEAM_NONE;
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* team_name = lua_tostring(L, idx);

		if (strcmp(team_name, "red") == 0)
			output= TEAM_RED;

		if (strcmp(team_name, "blue") == 0)
			output= TEAM_BLUE;

	}
	lua_pop(L, 1);

	return output;
}

// 
enum tile_id lua_toid(lua_State* L, int idx, struct tile* tile)
{
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* tile_id = lua_tostring(L, idx);

		for (size_t i = 0; i < TILE_CNT; i++)
			if (strcmp(tile_id, tile_to_string[i]) == 0)
			{
				lua_pop(L, 1);
				return i;
			}
	}

	lua_pop(L, 1);
	return 0;
}