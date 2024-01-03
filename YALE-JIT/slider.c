// Copyright 2024 Kieran W Harvie. All rights reserved.
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

extern double mouse_x;
extern double mouse_y;

struct slider
{
	struct wg_hud;

	double start, end, progress;
};

const struct wg_jumptable_hud slider_jumptable;

const double slider_padding = 16;	

static inline void clap(struct slider* const slider)
{
	if (slider->progress < 0)
		slider->progress = 0;
	else if (slider->progress > 1)
		slider->progress = 1;
}

static inline ALLEGRO_COLOR holder_color(const struct slider* const slider)
{
	switch (slider->hud_state)
	{
	default:
	case HUD_IDLE:
		return primary_pallet.edge;
	case HUD_HOVER:
		return primary_pallet.highlight;
	case HUD_ACTIVE:
		return primary_pallet.activated;
	}
}

static void draw(const struct wg_base* const wg)
{
	const struct slider* const slider = (const struct slider* const)wg;

	al_draw_filled_rounded_rectangle(-slider->half_width, -slider->half_height, slider->half_width, slider->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		primary_pallet.main);

	al_draw_line(-slider->half_width + slider_padding, 0, slider->half_width - slider_padding, 0,
		primary_pallet.edge, primary_pallet.edge_width);

	const double center = -slider->half_width + slider_padding + slider->progress * 2 * (slider->half_width - slider_padding);
	al_draw_filled_rectangle(center - 4, -4, center + 4, 4,
		holder_color(slider));

	al_draw_rounded_rectangle(-slider->half_width, -slider->half_height, slider->half_width, slider->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		primary_pallet.edge, primary_pallet.edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct slider* const slider = (const struct slider* const)wg;

	al_draw_filled_rounded_rectangle(-slider->half_width, -slider->half_height, slider->half_width, slider->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		al_map_rgb(255,255,255));
}

static void update(struct wg_base* const wg)
{
	struct slider* const slider = (const struct slider* const)wg;

	if (slider->hud_state != HUD_ACTIVE)
		return;

	double x = mouse_x;
	double y = mouse_y;
	widget_screen_to_local(wg, &x, &y);

	slider->progress = x / (2 * (slider->half_width - slider_padding)) + 0.5;

	clap(slider);
}

static void left_click(struct wg_base* const wg)
{
	struct slider* const slider = (const struct slider* const)wg;

	slider->hud_state = HUD_ACTIVE;
}

static void left_click_end(struct wg_base* const wg)
{
	struct slider* const slider = (const struct slider* const)wg;

	slider->hud_state = HUD_IDLE;
}

static int index(lua_State* L)
{
	struct slider* const slider = (struct slider* const)check_widget_lua(-2, &slider_jumptable);

	if (lua_type(L, -1) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "value") == 0)
		{
			const double value = slider->start + slider->progress * (slider->end - slider->start);

			lua_pushnumber(L, value);
			return 1;
		}

		if (strcmp(key, "start") == 0)
		{
			lua_pushnumber(L, slider->start);
			return 1;
		}

		if (strcmp(key, "end") == 0)
		{
			lua_pushnumber(L, slider->end);
			return 1;
		}
	}

	return -1;
}

static int newindex(lua_State* L)
{
	struct slider* const slider = (struct slider* const)check_widget_lua(-2, &slider_jumptable);

	if (lua_type(L, -2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -2);

		if (strcmp(key, "value") == 0)
		{
			slider->progress = (luaL_checknumber(L, -1) -slider->start)/ (slider->end - slider->start);
			clap(slider);

			return 1;
		}

		if (strcmp(key, "progress") == 0)
		{
			slider->progress = luaL_checknumber(L, -1);
			clap(slider);

			return 1;
		}

		if (strcmp(key, "start") == 0)
		{
			slider->start = luaL_checknumber(L, -1);

			return 1;
		}

		if (strcmp(key, "end") == 0)
		{
			slider->end = luaL_checknumber(L, -1);

			return 1;
		}
	}

	return -1;
}

const struct wg_jumptable_hud slider_jumptable =
{
	.type = "slider",

	.draw = draw,
	.mask = mask,
	.update = update,

	.left_click = left_click,
	.left_click_end = left_click_end,

	.index = index,
	.newindex = newindex,
};

int slider_new(lua_State* L)
{
	double start = 0;
	double end = 100;
	double progress = 0.2;

	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "start");

		if (lua_isnumber(L, -1))
			start = lua_tonumber(L, -1);

		lua_getfield(L, -2, "end");

		if (lua_isnumber(L, -1))
			end = lua_tonumber(L, -1);

		lua_getfield(L, -3, "progress");

		if (lua_isnumber(L, -1))
			progress = lua_tonumber(L, -1);		
		
		lua_pop(L, 3);
	}

	struct slider* slider = (struct slider*)wg_alloc_hud(sizeof(struct slider), &slider_jumptable);

	slider->progress = progress;
	slider->start = start;
	slider->end = end;

	clap(slider);

	const double min_half_width = 175;
	const double min_half_height = 16;

	slider->half_width = min_half_width > slider->half_width ? min_half_width : slider->half_width;
	slider->half_height = min_half_height > slider->half_height ? min_half_height : slider->half_height;

	return 1;
}
