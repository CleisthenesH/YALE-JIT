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
	struct wg_base;
	ALLEGRO_COLOR color;
	ALLEGRO_FONT* font;

	char text[];
};

static void draw(const struct wg_base* const wg)
{
	const struct button* const button = (const struct button* const)wg;

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		button->color);

	if (button->text)
		al_draw_text(button->font, al_map_rgb_f(1, 1, 1),
			0, -0.5 * al_get_font_line_height(button->font),
			ALLEGRO_ALIGN_CENTRE, button->text);

	al_draw_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		primary_pallet.edge, primary_pallet.edge_width);
}

static void mask(const struct wg_base* const wg)
{
	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		al_map_rgb(255, 0, 0));
}

static void hover_start(struct wg_base* const wg)
{
	((struct button* const) wg)->color = primary_pallet.highlight;
}

static void hover_end(struct wg_base* const wg)
{
	((struct button* const)wg)->color = primary_pallet.main;
}

const struct wg_jumptable_base button_jumptable =
{
	.draw = draw,
	.mask = mask,
	.hover_start = hover_start,
	.hover_end = hover_end,
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

	const size_t size = sizeof(struct button) + sizeof(char) * text_len;

	struct button* button = (struct button*) wg_alloc(WG_BASE, size, &button_jumptable);

	strcpy_s(button->text, text_len + 1, text ? text : "Placeholder");

	button->font = resource_manager_font(FONT_ID_SHINYPEABERRY);
	button->color = primary_pallet.main;

	const double min_half_width = 8 + 0.5 * al_get_text_width(button->font, button->text);
	const double min_half_height = 25;

	button->half_width = min_half_width > button->half_width ? min_half_width : button->half_width;
	button->half_height = min_half_height > button->half_height ? min_half_height : button->half_height;

	return 1;
}