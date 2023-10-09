// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "widget.h"
#include "resource_manager.h"
#include "meeple_tile_utility.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "particle.h"
#include <allegro5/allegro_primitives.h>
struct particle_bin* bin;

static void function(void* data, double time)
{
	al_draw_filled_circle(10 + 10 * time, 10, 10, al_map_rgb(255, 255, 255));
}

struct meeple
{
	struct wg_piece;
	enum TEAMS team;
};

const struct wg_jumptable_piece meeple_jumptable;

static void draw(const struct wg_base* const wg)
{
	struct meeple* meeple = (struct meeple*)wg;

	ALLEGRO_COLOR color[TEAM_CNT] = {
		al_color_name("pink"),
		al_color_name("darkred"),
		al_color_name("navy")
	};

	al_draw_tinted_scaled_bitmap(resource_manager_icon(ICON_ID_MEEPLE),
		color[meeple->team],
		0, 0, 512, 512,
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
		0);

	particle_bin_callback(bin);
}

static void mask(const struct wg_base* const wg)
{
	al_draw_tinted_scaled_bitmap(resource_manager_icon(ICON_ID_MEEPLE),
		al_map_rgb(255, 255, 255),
		0, 0, 512, 512,
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
		0);
}

static int index(lua_State* L)
{
	struct meeple* const meeple = (struct meeple* const)check_widget_lua(-2, &meeple_jumptable);

	if (lua_type(L, -1) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "team") == 0)
		{
			if (meeple->team == TEAM_RED)
				lua_pushstring(L, "red");
			else if (meeple->team == TEAM_BLUE)
				lua_pushstring(L, "blue");
			else
				lua_pushstring(L, "none");

			return 1;
		}
	}

	return -1;
}

// 
static void lua_toteam(lua_State* L, int idx, struct meeple* meeple)
{
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		const char* team_name = lua_tostring(L, idx);

		if (strcmp(team_name, "red") == 0)
			meeple->team = TEAM_RED;
		else if (strcmp(team_name, "blue") == 0)
			meeple->team = TEAM_BLUE;

	}
	lua_pop(L, 1);
}

const struct wg_jumptable_piece meeple_jumptable =
{
	.type = "meeple",

	.draw = draw,
	.mask = mask,
	.index = index,
};

int meeple_new(lua_State* L)
{
	struct meeple* meeple = (struct meeple*)wg_alloc_piece( sizeof(struct meeple), &meeple_jumptable);

	if (!meeple)
		return 0;

	meeple->team = TEAM_NONE;
	meeple->half_width = 40;
	meeple->half_height = 40;

	if (lua_istable(L, -2))
	{
		lua_getfield(L, -2, "team");
		lua_toteam(L, -1, meeple);
	}

	if (!bin)
	{
		bin = particle_bin_new(1);
		particle_bin_append(bin, function, NULL, NULL, 2);
	}

	return 1;
}