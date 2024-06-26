// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "widget.h"

#include <lua.h>
#include <lauxlib.h>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "material.h"

extern double mouse_x, mouse_y;

struct material_test
{
	struct wg_base;

	struct material* material;

	enum MATERIAL_ID effect_id;
	enum SELECTION_ID selection_id;

	ALLEGRO_BITMAP* bitmap;
};
const struct wg_jumptable_base material_test_jumptable;

static enum MATERIAL_ID lua_toeffect(struct lua_State* L, int idx)
{
	if (!lua_isnumber(L, -1))
		return 0;

	enum MATERIAL_ID effect_id = luaL_checkint(L, -1);

	if (effect_id >= MATERIAL_ID_MAX || effect_id < 0)
		effect_id = 0;

	return effect_id;
}

static enum SELECTION_ID lua_toselection(struct lua_State* L, int idx)
{
	if (!lua_isnumber(L, -1))
		return 0;

	enum SELECTION_ID selection_id = luaL_checkint(L, -1);

	if (selection_id >= SELECTION_ID_MAX || selection_id < 0)
		selection_id = 0;

	return selection_id;
}

static void draw(const struct wg_base* const wg)
{
	const struct material_test* const material_test = (const struct material_test* const)wg;

	material_point(material_test->material, mouse_x, mouse_y);
	material_apply(material_test->material);

	if (material_test->bitmap)
		al_draw_bitmap(material_test->bitmap, -wg->hw, -wg->hh, 0);
	else
		al_draw_filled_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
			al_map_rgb(0, 255, 0));

	material_apply(NULL);
}

static void mask(const struct wg_base* const wg)
{
	al_draw_filled_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		al_map_rgb(0, 255, 0));
}

static int newindex(lua_State* L)
{
	struct material_test* const material_test = (struct material_test* const)check_widget_lua(-3, &material_test_jumptable);

	if (lua_type(L, -2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(L, -2);

		if (strcmp(key, "effect") == 0)
		{
			material_test->effect_id = luaL_checkinteger(L, -1)-1;

			free(material_test->material);
			material_test->material = material_new(material_test->effect_id, material_test->selection_id);

			return 0;
		}

		if (strcmp(key, "selection") == 0)
		{
			material_test->selection_id = luaL_checkinteger(L, -1)-1;

			free(material_test->material);
			material_test->material = material_new(material_test->effect_id, material_test->selection_id);

			return 0;
		}

		if (strcmp(key, "bitmap") == 0)
		{
			if (material_test->bitmap)
				al_destroy_bitmap(material_test);

			material_test->bitmap = al_load_bitmap(lua_tostring(L, -1));
			material_test->hh = 0.5 * al_get_bitmap_height(material_test->bitmap);
			material_test->hw = 0.5 * al_get_bitmap_width(material_test->bitmap);

			return 0;
		}
	}

	return -1;
}

const struct wg_jumptable_base material_test_jumptable =
{
	.type = "material_test",

	.draw = draw,
	.mask = mask,

	.newindex = newindex
};

int material_test_new(lua_State* L)
{
	struct material_test* material_test = (struct material_test*)wg_alloc_hud(sizeof(struct material_test), &material_test_jumptable);
	
	if (lua_istable(L, -2))
	{
		lua_getfield(L, -1, "effect");

		material_test->effect_id = lua_toeffect(L, -1);

		lua_getfield(L, -2, "selection");

		material_test->selection_id = lua_toselection(L, -1);

		lua_pop(L, 2);
	}

	material_test->bitmap = NULL;

	material_test->material = material_new(material_test->effect_id, material_test->selection_id);

	return 1;
}
