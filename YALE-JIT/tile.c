// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "widget.h"
#include "resource_manager.h"
#include "meeple_tile_utility.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

const struct wg_jumptable_zone tile_jumptable;

struct tile
{
	struct wg_zone;
	enum TEAMS team;
	enum tile_id id;
};

static inline enum TILE_PALLET state_to_pallet(struct wg_zone* zone)
{
	if (zone->nominated)
		return TILE_PALLET_NOMINATED;

	if (zone->highlighted)
		return TILE_PALLET_HIGHLIGHTED;

	return TILE_PALLET_IDLE;
}

static void draw(const struct wg_base* const wg)
{
	struct tile* const tile = (struct tile* const)wg;

	al_draw_scaled_bitmap(resource_manager_tile(TILE_EMPTY),
		0, 0, 300, 300,
		-wg->hw, -wg->hh, 2 * wg->hw, 2 * wg->hh,
		0);

	if (tile->id == TILE_EMPTY)
		return;

	ALLEGRO_COLOR tile_pallet[TEAM_CNT][TILE_PALLET_CNT] =
	{
		{al_color_name("white"),al_color_name("khaki"),al_color_name("gold")},
		{al_color_name("tomato"),al_color_name("crimson"),al_color_name("brown")},
		{al_color_name("lightblue"),al_color_name("royalblue"),al_color_name("steelblue")},
	};

	al_draw_tinted_scaled_bitmap(resource_manager_tile(tile->id),
		tile_pallet[tile->team][state_to_pallet(wg)],
		0, 0, 300, 300,
		-wg->hw, -wg->hh, 2 * wg->hw, 2 * wg->hh,
		0);
}

static void mask(const struct wg_base* const wg)
{
	al_draw_scaled_bitmap(resource_manager_tile(TILE_EMPTY),
		0, 0, 300, 300,
		-wg->hw, -wg->hh, 2 * wg->hw, 2 * wg->hh,
		0);
}

static int index(lua_State* L)
{
	struct tile* const tile = (struct tile* const) check_widget_lua(-2, &tile_jumptable);

	if (lua_type(L, -1) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "team") == 0)
		{
			if (tile->team == TEAM_RED)
				lua_pushstring(L, "red");
			else if (tile->team == TEAM_BLUE)
				lua_pushstring(L, "blue");
			else
				lua_pushstring(L, "none");

			return 1;
		}
		else if (strcmp(key, "tile") == 0)
		{
			lua_pushstring(L, tile_to_string[tile->id]);
			return 1;
		}
		else if (strcmp(key, "tile_id") == 0)
		{
			lua_pushinteger(L, tile->id);
			return 1;
		}
	}

	return -1;
}

static int newindex(lua_State* L)
{
	struct tile* const tile = (struct tile* const) check_widget_lua(-3, &tile_jumptable);

	if (lua_type(L, -2) == LUA_TSTRING)
	{
		const char* key = luaL_checkstring(L, -2);

		if (strcmp(key, "team") == 0)
		{
			tile->team = lua_toteam(L, -1);
			lua_pop(L, 1);
			return 0;
		}
		else if (strcmp(key, "tile") == 0)
		{
			tile->id = lua_toid(L, -1);
			lua_pop(L, 1);
			return 0;
		}
		else if (strcmp(key, "tile_id") == 0)
		{
			if (!lua_isnumber(L, -1))
			{
				lua_pop(L, 1);
				return 0;
			}
			
			tile->id = (int) lua_tointeger(L, -1);

			if (tile->id >= TILE_CNT)
				tile->id %= TILE_CNT;
			else while (tile->id < 0)
				tile->id += TILE_CNT;

			lua_pop(L, 1);
			return 0;
		}
	}

	return -1;
}

const struct wg_jumptable_zone tile_jumptable =
{
	.type = "tile",

	.draw = draw,
	.mask = mask,

	.index = index,
	.newindex = newindex
};

int tile_new(lua_State* L)
{
	if (!lua_istable(L, -1))
		lua_newtable(L);

	// Set default hh.
	lua_getfield(L, -1, "hh");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 50);
		lua_setfield(L, -3, "hh");
	}

	lua_pop(L, 1);

	// Set default hw.
	lua_getfield(L, -1, "hw");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 50);
		lua_setfield(L, -3, "hw");
	}

	lua_pop(L, 1);

	struct tile* tile = (struct tile*) wg_alloc_zone(sizeof(struct tile), &tile_jumptable);

	if (!tile)
		return 0;

	tile->team = TEAM_NONE;
	tile->id = TILE_EMPTY;

	if (lua_istable(L, -2))
	{
		lua_getfield(L, -2, "team");
		tile->team = lua_toteam(L, -1);

		lua_getfield(L, -2, "tile");
		tile->id = lua_toid(L, -1);
	}

	return 1;
}