// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "widget.h"
#include "resource_manager.h"
#include "meeple_tile_utility.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

struct meeple
{
	struct wg_piece;
	enum TEAMS team;
};

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
}

static void mask(const struct wg_base* const wg)
{
	al_draw_tinted_scaled_bitmap(resource_manager_icon(ICON_ID_MEEPLE),
		al_map_rgb(255, 255, 255),
		0, 0, 512, 512,
		-wg->half_width, -wg->half_height, 2 * wg->half_width, 2 * wg->half_height,
		0);
}

static struct wg_jumptable_piece meeple_table =
{
	.type = "meeple",

	.draw = draw,
	.mask = mask,
};

int meeple_new(lua_State* L)
{
	struct meeple* meeple = (struct meeple*)wg_alloc(WG_PIECE, sizeof(struct meeple), &meeple_table);

	if (!meeple)
		return 0;

	meeple->team = TEAM_BLUE;
	meeple->half_width = 40;
	meeple->half_height = 40;

	return 1;
}