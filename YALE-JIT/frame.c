// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "widget.h"

#include <lua.h>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_primitives.h>

struct frame
{
	struct wg_frame;
};

static void draw(const struct wg_base* const wg)
{
	const struct frame* const frame = (const struct frame* const)wg;
	const struct widget_pallet* const pallet = frame->pallet;

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		pallet->edge_radius, pallet->edge_radius,
		pallet->main);

	al_draw_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct frame* const frame = (const struct frame* const)wg;

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height,
		frame->pallet->edge_radius, frame->pallet->edge_radius,
		al_map_rgb(255, 255, 255));
}

const struct wg_jumptable_hud frame_jumptable =
{
	.type = "frame",

	.draw = draw,
	.mask = mask,
};

int frame_new(lua_State* L)
{
	struct frame* frame = (struct frame*)wg_alloc_frame(sizeof(struct frame), &frame_jumptable);

	return 1;
}