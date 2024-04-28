// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "widget.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "resource_manager.h"

struct counter
{
	struct wg_hud;

	enum icon_id icon;
	int value;
};

const struct wg_jumptable_hud counter_jumptable;

static void draw(const struct wg_base* const wg)
{
	const struct counter* const counter = (const struct counter* const)wg;
	const struct widget_pallet* const pallet = counter->pallet;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->main);

	al_draw_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);

	if(counter->icon != ICON_ID_NULL)
		al_draw_scaled_bitmap(resource_manager_icon(counter->icon),
			0, 0, 512, 512,
			-50,-50,100,100,
			0);

	al_draw_textf(pallet->font, al_map_rgb_f(1, 1, 1),
		0, -0.5 * al_get_font_line_height(pallet->font),
		ALLEGRO_ALIGN_CENTRE, "%d",
		counter->value);

	al_draw_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct counter* const counter = (const struct counter* const)wg;
	const struct widget_pallet* const pallet = counter->pallet;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		al_map_rgb(255, 0, 0));
}

static int set(lua_State* L)
{
	struct counter* const counter = (struct counter* const)check_widget_lua(-2, &counter_jumptable);

	counter->value = luaL_checkinteger(L, -1);

	return 0;
}

static int add(lua_State* L)
{
	struct counter* const counter = (struct counter* const)check_widget_lua(-2, &counter_jumptable);

	counter->value += luaL_checkinteger(L, -1);

	return 0;
}

static int index(lua_State* L)
{
	struct counter* const counter = (struct counter* const)check_widget_lua(-2, &counter_jumptable);

	if (lua_type(L, -1) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "set") == 0)
		{
			lua_pushcfunction(L, set);

			return 1;
		}

		if (strcmp(key, "add") == 0)
		{
			lua_pushcfunction(L, add);

			return 1;
		}

		if (strcmp(key, "value") == 0)
		{
			lua_pushinteger(L, counter->value);

			return 1;
		}
	}

	return -1;
}

static int newindex(lua_State* L)
{
	struct counter* const counter = (struct counter* const)check_widget_lua(-3, &counter_jumptable);

	if (lua_type(L, -2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -2);

		if (strcmp(key, "value") == 0)
		{
			counter->value = luaL_checkinteger(L, -1);

			return 1;
		}
	}

	return -1;
}

const struct wg_jumptable_hud counter_jumptable =
{
	.type = "counter",

	.draw = draw,
	.mask = mask,

	.index = index,
	.newindex = newindex,
};

int counter_new(lua_State* L)
{
	if (!lua_istable(L, -1))
		lua_newtable(L);

	// Set default hh.
	lua_getfield(L, -1, "hh");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 55);
		lua_setfield(L, -3, "hh");
	}

	lua_pop(L, 1);

	// Set default hw.
	lua_getfield(L, -1, "hw");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 55);
		lua_setfield(L, -3, "hw");
	}

	lua_pop(L, 1);

	struct counter* counter = (struct counter*)wg_alloc_hud(sizeof(struct counter), &counter_jumptable);

	counter->value = 0;
	counter->icon = ICON_ID_NULL;

	if (lua_istable(L, -2))
	{
		lua_getfield(L, -2, "icon");

		if (lua_isnumber(L, -1))
			counter->icon = lua_tointeger(L, -1);

		lua_getfield(L, -3, "value");

		if (lua_isnumber(L, -1))
			counter->value = lua_tointeger(L, -1);

		lua_pop(L, 2);
	}

	return 1;
}