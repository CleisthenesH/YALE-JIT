// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "widget.h"

#include <lua.h>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include <math.h>

#include "resource_manager.h"

extern double mouse_x;
extern double mouse_y;

struct drop_down
{
	struct wg_hud;

	size_t option;
	size_t option_cnt;
	size_t option_len;
};
const struct wg_jumptable_hud drop_down_jumptable;

static char* option(struct drop_down* drop_down,size_t i)
{
	return (char*)((char*)(drop_down)+sizeof(struct drop_down) + i * (drop_down->option_len+1));
}

static void draw(const struct wg_base* const wg)
{
	const struct drop_down* const drop_down = (const struct drop_down* const)wg;
	const struct widget_pallet* const pallet = drop_down->pallet;

	if (drop_down->hud_state == HUD_ACTIVE)
	{
		al_draw_filled_rounded_rectangle(-wg->hw+2, -wg->hh, wg->hw-2, wg->hh + drop_down->option_cnt * 50,
			drop_down->pallet->edge_radius, drop_down->pallet->edge_radius, pallet->recess);

		al_draw_rounded_rectangle(-wg->hw+2, -wg->hh, wg->hw-2, wg->hh + drop_down->option_cnt * 50,
			drop_down->pallet->edge_radius, drop_down->pallet->edge_radius,
			pallet->edge, pallet->edge_width);

		al_draw_text(pallet->font, al_map_rgb_f(1, 1, 1),
			0, -0.5 * al_get_font_line_height(pallet->font) + 50,
			ALLEGRO_ALIGN_CENTRE, option(drop_down, 0));

		for (size_t idx = 1; idx < drop_down->option_cnt; idx++)
		{
			al_draw_text(pallet->font, al_map_rgb_f(1, 1, 1),
				0, -0.5 * al_get_font_line_height(pallet->font) + 50 + 50 * idx,
				ALLEGRO_ALIGN_CENTRE, option(drop_down, idx));

			al_draw_line(-wg->hw+4, wg->hh+50*idx, wg->hw-4, wg->hh + 50 * idx,
				pallet->edge, pallet->edge_width);
		}
	}

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		drop_down->pallet->edge_radius, drop_down->pallet->edge_radius,
		drop_down->hud_state == HUD_IDLE ? pallet->main : pallet->highlight);

	al_draw_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);

	al_draw_text(pallet->font, al_map_rgb_f(1, 1, 1),
		0, -0.5 * al_get_font_line_height(pallet->font) ,
		ALLEGRO_ALIGN_CENTRE, option(drop_down, drop_down->option));

}

static void mask(const struct wg_base* const wg)
{
	const struct drop_down* const drop_down = (const struct drop_down* const)wg;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		drop_down->pallet->edge_radius, drop_down->pallet->edge_radius,
		al_map_rgb(255, 255, 255));

	if(drop_down->hud_state == HUD_ACTIVE)
		al_draw_filled_rounded_rectangle(-wg->hw+2, -wg->hh, wg->hw-2, wg->hh+ drop_down->option_cnt * 50,
			drop_down->pallet->edge_radius, drop_down->pallet->edge_radius,
			al_map_rgb(255, 255, 255));

}

static void left_click(struct wg_base* const wg)
{
	struct drop_down* const drop_down = (const struct drop_down* const)wg;

	if (drop_down->hud_state != HUD_ACTIVE)
	{
		drop_down->hud_state = HUD_ACTIVE;
		return;
	}

	double x = mouse_x;
	double y = mouse_y;
	widget_screen_to_local(wg, &x, &y);

	size_t option = floor(y * 0.02 - 0.5);

	if (option >= 0 && option < drop_down->option_cnt)
		drop_down->option = option;
}

static void click_off(struct wg_base* const wg)
{
	struct drop_down* const drop_down = (const struct drop_down* const)wg;

	drop_down->hud_state = HUD_IDLE;
}

static int index(lua_State* L)
{
	struct drop_down* const drop_down = (struct drop_down* const)check_widget_lua(-2, &drop_down_jumptable);

	if (lua_type(L, -1) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "option") == 0)
		{
			lua_pushstring(L, option(drop_down,drop_down->option));
			return 1;
		}

		if (strcmp(key, "option_id") == 0)
		{
			lua_pushnumber(L, drop_down->option+1);
			return 1;
		}
	}

	return -1;
}

static int newindex(lua_State* L)
{
	struct drop_down* const drop_down = (struct drop_down* const)check_widget_lua(-3, &drop_down_jumptable);

	if (lua_type(L, -2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -2);

		if (strcmp(key, "option_id") == 0)
		{
			const size_t option = luaL_checkinteger(L, -1)-1;

			if (option >= 0 && option < drop_down->option_cnt)
				drop_down->option = option;

			return 1;
		}
	}

	return -1;
}

const struct wg_jumptable_hud drop_down_jumptable =
{
	.type = "drop_down",
		
	.draw = draw,
	.mask = mask,
	.left_click = left_click,
	.click_off = click_off,

	.index = index,
	.newindex = newindex
};

int drop_down_new(lua_State* L)
{
	if (!lua_istable(L, -1))
		return 0;

	lua_getfield(L, -1, "options");

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}

	size_t options_cnt = 0;
	size_t options_len = 0;
	size_t options_len_max = 0;
	char** options = NULL;

	double text_width = 0;
	double text_width_max = 0;

	options_cnt = lua_objlen(L, -1);
	options = calloc(sizeof(char*),options_cnt);

	for (size_t idx = 0; idx < options_cnt; idx++)
	{
		lua_pushnumber(L, idx+1);
		lua_gettable(L, -2);
		options[idx] = lua_tolstring(L, -1, &options_len);

		if (options_len_max < options_len)
			options_len_max = options_len;

		text_width = al_get_text_width(primary_pallet.font, options[idx]);

		if (text_width > text_width_max)
			text_width_max = text_width;

		lua_pop(L, 1);
	}

	lua_pop(L, 1);

	// Set default hh.
	lua_getfield(L, -1, "hh");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 25);
		lua_setfield(L, -3, "hh");
	}

	lua_pop(L, 1);

	// Set default hw.
	lua_getfield(L, -1, "hw");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 8 + 0.5 * text_width_max);
		lua_setfield(L, -3, "hw");
	}

	lua_pop(L, 1);

	const size_t size = sizeof(struct drop_down) + sizeof(char) * (options_len_max + 1)*options_cnt;
	struct drop_down* drop_down = (struct drop_down*) wg_alloc_hud( size, &drop_down_jumptable);

	drop_down->option_cnt = options_cnt;
	drop_down->option_len = options_len_max;


	for (size_t idx = 0; idx < options_cnt; idx++)
	{
		char* dest = option(drop_down, idx);
		strcpy_s(dest, options_len_max + 1, options[idx]);

	}
	free(options);

	drop_down->option = 0;

	return 1;
}
