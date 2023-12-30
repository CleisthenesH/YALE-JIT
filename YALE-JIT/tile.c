// Copyright 2023 Kieran W Harvie. All rights reserved.
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

static const char* tile_to_string[] = {
	"empty",
	"bridge",
	"camp",
	"castle",
	"city",
	"dungeon",
	"farm",
	"fort",
	"hills",
	"lake",
	"mine",
	"monolith",
	"mountains",
	"oak",
	"oaks",
	"pine",
	"pines",
	"poi",
	"quest",
	"ruins",
	"shipwreck",
	"skull",
	"swamp",
	"tower",
	"town"
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
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
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
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
		0);
}

static void mask(const struct wg_base* const wg)
{
	al_draw_scaled_bitmap(resource_manager_tile(TILE_EMPTY),
		0, 0, 300, 300,
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
		0);
}

// 
static void lua_toteam(lua_State* L, int idx, struct tile* tile)
{
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* team_name = lua_tostring(L, idx);

		if (strcmp(team_name, "red") == 0)
			tile->team = TEAM_RED;
		else if (strcmp(team_name, "blue") == 0)
			tile->team = TEAM_BLUE;

	}
	lua_pop(L, 1);
}

// 
static void lua_toid(lua_State* L, int idx, struct tile* tile)
{
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* tile_id = lua_tostring(L, idx);

		for (size_t i = 0; i < TILE_CNT; i++)
			if (strcmp(tile_id, tile_to_string[i]) == 0)
			{
				tile->id = i;
				break;
			}
	}

	lua_pop(L, 1);
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
			lua_toteam(L, -1, tile);
			lua_pop(L, 1);
			return 0;
		}
		else if (strcmp(key, "tile") == 0)
		{
			lua_toid(L, -1, tile);
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
	struct tile* tile = (struct tile*) wg_alloc_zone(sizeof(struct tile), &tile_jumptable);

	if (!tile)
		return 0;

	tile->team = TEAM_NONE;
	tile->id = TILE_EMPTY;

	if (lua_istable(L, -2))
	{
		lua_getfield(L, -2, "team");
		lua_toteam(L, -1, tile);

		lua_getfield(L, -2, "tile");
		lua_toid(L, -1, tile);
	}

	tile->half_width = 50;
	tile->half_height = 50;

	return 1;
}