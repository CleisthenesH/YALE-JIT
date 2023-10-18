// Copyright 2023 Kieran W Harvie. All rights reserved.
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

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		pallet->edge_radius, pallet->edge_radius,
		button->hud_state == HUD_IDLE ? pallet->main : pallet->highlight);

	if (button->text)
		al_draw_text(pallet->font, al_map_rgb_f(1, 1, 1),
			0, -0.5 * al_get_font_line_height(pallet->font),
			ALLEGRO_ALIGN_CENTRE, button->text);

	al_draw_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct button* const button = (const struct button* const)wg;

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
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
	// Get the text len so we know how munch memory to alloc
	size_t text_len = 11;
	char* text = NULL;

	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "text");

		if (lua_isstring(L, -1))
			text = lua_tolstring(L, -1, &text_len);

		lua_pop(L, 1);
	}

	const size_t size = sizeof(struct button) + sizeof(char) * (text_len+1);

	struct button* button = (struct button*) wg_alloc_hud( size, &button_jumptable);

	strcpy_s(button->text, text_len + 1, text ? text : "Placeholder");

	const double min_half_width = 8 + 0.5 * al_get_text_width(button->pallet->font, button->text);
	const double min_half_height = 25;

	button->half_width = min_half_width > button->half_width ? min_half_width : button->half_width;
	button->half_height = min_half_height > button->half_height ? min_half_height : button->half_height;

	return 1;
}