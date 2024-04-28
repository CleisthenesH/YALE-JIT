// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "widget.h"

#include <lua.h>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "resource_manager.h"

struct button
{
	struct wg_hud;

	char text[];
};

static void draw(const struct wg_base* const wg)
{
	const struct button* const button = (const struct button* const)wg;
	const struct widget_pallet* const pallet = button->pallet;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		button->hud_state == HUD_IDLE ? pallet->main : pallet->highlight);

	if (button->text)
		al_draw_text(pallet->font, al_map_rgb_f(1, 1, 1),
			0, -0.5 * al_get_font_line_height(pallet->font),
			ALLEGRO_ALIGN_CENTRE, button->text);

	al_draw_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct button* const button = (const struct button* const)wg;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		button->pallet->edge_radius, button->pallet->edge_radius,
		al_map_rgb(255, 255, 255));
}

const struct wg_jumptable_hud button_jumptable =
{
	.type = "button",
		
	.draw = draw,
	.mask = mask,
};

int button_new(lua_State* L)
{
	if (!lua_istable(L, -1))
		lua_newtable(L);

	// Set default text.
	// Also retreve paramaters for size allocation.
	size_t text_len = 11;
	char* text = "Placeholder";

	lua_getfield(L, -1, "text");

	if (lua_isstring(L, -1))
		text = lua_tolstring(L, -1, &text_len);
	else
	{
		lua_pushstring(L, "Placeholder");
		lua_setfield(L, -3, "text");
	}

	lua_pop(L, 1);

	// Set default hh.
	lua_getfield(L, -1, "hh");

	if (!lua_isnumber(L, -1))
	{
		const double min_half_height = 25;

		lua_pushnumber(L, min_half_height);
		lua_setfield(L, -3, "hh");
	}

	lua_pop(L, 1);

	// Set default hw.
	lua_getfield(L, -1, "hw");

	if (!lua_isnumber(L, -1))
	{
		const double min_half_width = 8 + 0.5 * al_get_text_width(primary_pallet.font, text);

		lua_pushnumber(L, min_half_width);
		lua_setfield(L, -3, "hw");
	}

	lua_pop(L, 1);

	const size_t size = sizeof(struct button) + sizeof(char) * (text_len+1);
	struct button* button = (struct button*) wg_alloc_hud( size, &button_jumptable);

	strcpy_s(button->text, text_len + 1, text);

	return 1;
}