// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "widget.h"
#include "resource_manager.h"

#include <lua.h>
#include <math.h>

#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_color.h>

extern double mouse_x;
extern double mouse_y;

struct tile_selector {
	struct wg_hud;	

	enum tile_id selection;
	enum tile_id hover;

	float small;
	float large;

	float r;
};

static inline double draw_tile(struct tile_selector* selector, double x, int i, double r)
{
	r =  r* r* (3.0 - 2.0 * r);

	const float blend = selector->small + r * (selector->large - selector->small);

	al_draw_scaled_bitmap(resource_manager_tile(TILE_EMPTY),
		0, 0, 300, 300,
		x, -blend, 2 * blend, 2 * blend,
		0);

	al_draw_scaled_bitmap(resource_manager_tile(i),
		0, 0, 300, 300,
		x, -blend, 2 * blend, 2 * blend,
		0);

	return 2 * blend;
}

static void draw(const struct wg_base* const wg)
{
	struct tile_selector* selector = (struct tile_selector*)wg;
	const struct widget_pallet* const pallet = selector->pallet;

	al_draw_filled_rounded_rectangle(-wg->half_width, -selector->small, wg->half_width, selector->small,
		pallet->edge_radius, pallet->edge_radius,
		pallet->highlight);

	al_draw_rounded_rectangle(-wg->half_width, -selector->small, wg->half_width, selector->small,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);

	float x = -wg->half_width;

	// Draw all pre tiles
	for (int i = 0; i < selector->hover-1; i++)
		x += draw_tile(selector,x,i,0);

	// Draw the pre hover
	if (selector->hover > 0)
		x += draw_tile(selector, x, selector->hover - 1, (selector->r < 0.5) ? (0.5 - selector->r) : 0);

	// Draw the hover
	if ((selector->hover == 0 && selector->r < 0.5) || (selector->hover == TILE_CNT - 1 && selector->r > 0.5))
		x += draw_tile(selector, x, selector->hover, 1);
	else
		x += draw_tile(selector, x, selector->hover, (selector->r < 0.5) ? (selector->r + 0.5) : (1.5 - selector->r));

	// Draw the post hover
	if (selector->hover +1 < TILE_CNT)
		x += draw_tile(selector, x, selector->hover + 1, (selector->r > 0.5) ? (selector->r-0.5) : 0);

	// Draw all post tiles
	for (int i = selector->hover+2; i < TILE_CNT; i++)
		x += draw_tile(selector, x, i, 0);
}

static void mask(const struct wg_base* const wg)
{
	struct tile_selector* selector = (struct tile_selector*)wg;

	float x = -wg->half_width+2* selector->small *(selector->hover-1);

	al_draw_filled_rounded_rectangle(-wg->half_width, -selector->small, wg->half_width, selector->small,
		selector->pallet->edge_radius, selector->pallet->edge_radius,
		al_map_rgb(255, 255, 255));

	// This method needlessly draw the tile art, can be optimized.

	// Draw the pre hover
	if (selector->hover > 0)
		x += draw_tile(selector, x, selector->hover - 1, (selector->r < 0.5) ? (0.5 - selector->r) : 0);

	// Draw the hover
	if ((selector->hover == 0 && selector->r < 0.5) || (selector->hover == TILE_CNT - 1 && selector->r > 0.5))
		x += draw_tile(selector, x, selector->hover, 1);
	else
		x += draw_tile(selector, x, selector->hover, (selector->r < 0.5) ? (selector->r + 0.5) : (1.5 - selector->r));

	// Draw the post hover
	if (selector->hover + 1 < TILE_CNT)
		x += draw_tile(selector, x, selector->hover + 1, (selector->r > 0.5) ? (selector->r - 0.5) : 0);
}

static void update(struct wg_base* const wg)
{
	struct tile_selector* const selector = (const struct tile_selector* const)wg;

	if (selector->hud_state != HUD_HOVER)
		return;

	double x = mouse_x;
	double y = mouse_y;
	widget_screen_to_local(wg, &x, &y);

	x = 0.5*TILE_CNT *(x/selector->half_width+1.0);

	selector->r = modf(x,&y);
	selector->hover = (int) y;

	if (selector->hover >= TILE_CNT)
	{
		selector->hover = TILE_CNT - 1;
		selector->r = 1;
	}else if (selector->hover < 0)
	{
		selector->hover = 0;
		selector->r = 0;
	}
}

const struct wg_jumptable_hud tile_selector_jumptable =
{
	.type = "tile_selector",

	.draw = draw,
	.mask = mask,

	.update = update
};

int tile_selector_new(lua_State* L)
{
	float small = 20;
	float large = 100;

	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "small");

		if (lua_isnumber(L, -1))
			small = lua_tonumber(L, -1);

		lua_getfield(L, -2, "large");

		if (lua_isnumber(L, -1))
			large = lua_tonumber(L, -1);

		lua_pushnumber(L, 2*((TILE_CNT - 1) * small + large));
		lua_setfield(L, -4, "width");

		lua_pushnumber(L, 2 * large);
		lua_setfield(L, -4, "height");

		lua_pop(L, 2);
	}

	struct tile_selector* selector = (struct tile_selector*)wg_alloc_hud(sizeof(struct tile_selector), &tile_selector_jumptable);

	if (!selector)
		return 0;

	selector->selection = TILE_HILLS;

	selector->hover = selector->selection;
	selector->r = 0.5;

	selector->small = small;
	selector->large = large;

	return 1;
}