// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "resource_manager.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_color.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

//#define EMILY_OUTPUT
#define POOL_PARTY_WORKAROUND

#ifdef POOL_PARTY_WORKAROUND
const int padding = 10;
#endif

static const char* const font_names[] =
{
	"BasicPupBlack",
	"BasicPupWhite",
	"BoldBasic",
	"BoldBlocks",
	"BoldBubblegum",
	"BoldCheese",
	"BoldTwilight",
	"CakeIcing",
	"CleanBasic",
	"CleanCraters",
	"CleanPlate",
	"CleanVictory",
	"DigitalPup",
	"GoldPeaberry",
	"IndigoPaint",
	"IndigoPeaberry",
	"PaintBasic",
	"PoolParty",
	"RaccoonSerif-Base",
	"RaccoonSerif-Bold",
	"RaccoonSerif-Medium",
	"RaccoonSerif-Mini",
	"RaccoonSerif-Mono",
	"RedPeaberry",
	"ShinyPeaberry",
	"WhitePeaberry",
	"WhitePeaberryOutline"
};

static ALLEGRO_FONT* font_table[FONT_ID_COUNT];
static ALLEGRO_BITMAP* icon_table[ICON_ID_COUNT] = { NULL };
static ALLEGRO_BITMAP* tile_table[TILE_CNT] = { NULL };

// Turns the bitmap and lua file uploaded by Emily Huo to itch.io into a ALLEGRO_FONT
// Currently ignores kerling and xoffset
static ALLEGRO_FONT* emily_huo_font(lua_State* lua, const char* font_name)
{
	char file_name_buffer[256];

	strcpy_s(file_name_buffer, 256, "res/fonts/");
	strcat_s(file_name_buffer, 256, font_name);
	strcat_s(file_name_buffer, 256, ".png");

	ALLEGRO_BITMAP* bitmap = al_load_bitmap(file_name_buffer);

	if (!bitmap)
		return NULL;

	strcpy_s(file_name_buffer, 256, "res/fonts/");
	strcat_s(file_name_buffer, 256, font_name);
	strcat_s(file_name_buffer, 256, ".lua");

	luaL_dofile(lua, file_name_buffer);

	int max_height = 0;
	int total_width = 1;

	// create a table to transpose the Font.char entries into being indexed by their "letter" keys
	lua_newtable(lua);

	lua_pushstring(lua, "chars");
	lua_gettable(lua, -3);

	lua_pushnil(lua);
	while (lua_next(lua, -2) != 0)
	{
		// stack: font_table, transpose_table, src_table, key, value 
		lua_pushstring(lua, "letter");
		lua_gettable(lua, -2);
		lua_pushvalue(lua, -2);
		lua_settable(lua, -6);

		// stack: font_table, transpose_table, src_table, key, value 
		lua_pushstring(lua, "width");
		lua_gettable(lua, -2);
		const int width = luaL_checkinteger(lua, -1);

		total_width += width + 1;

		lua_pop(lua, 1);

		// stack: font_table, transpose_table, src_table, key, value 
		lua_pushstring(lua, "height");
		lua_gettable(lua, -2);
		const int height = luaL_checkinteger(lua, -1);

		lua_pop(lua, 1);

		lua_pushstring(lua, "yoffset");
		lua_gettable(lua, -2);
		const int yoffset = luaL_checkinteger(lua, -1);

		if (height + yoffset > max_height)
			max_height = height + yoffset;

		lua_pop(lua, 2);
	}

	lua_pop(lua, 1);
	// stack: font_table, transposed_table

	ALLEGRO_BITMAP* bitmap_font = NULL;
	const int spacewidth = 20;

#ifdef POOL_PARTY_WORKAROUND
	bitmap_font = al_create_bitmap(total_width + spacewidth, max_height + 2 + 2 * padding);
#else
	bitmap_font = al_create_bitmap(total_width + spacewidth, max_height + 2);
#endif

	if (!bitmap_font)
	{
		al_destroy_bitmap(bitmap);
		return NULL;
	}


	// Directly write to bitmap_font
	al_set_target_bitmap(bitmap_font);
	al_clear_to_color(al_color_name("pink"));
	al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO);
	al_set_render_state(ALLEGRO_ALPHA_TEST, 0);

	int x, y, w, h, yoffset, xadvance;

	char buffer[2] = { 'A','\0' };

	// write space seperatly

#ifdef POOL_PARTY_WORKAROUND
	al_draw_filled_rectangle(1, 1 + padding, 1 + spacewidth, 1 + max_height, al_map_rgba(0, 0, 0, 0));
#else
	al_draw_filled_rectangle(1, 1, 1 + spacewidth, 1 + max_height, al_map_rgba(0, 0, 0, 0));
#endif
	xadvance = 2 + spacewidth;

	for (size_t i = 33; i <= 126; i++)
	{
		buffer[0] = (char)i;
		lua_pushstring(lua, buffer);
		lua_gettable(lua, -2);

		lua_pushstring(lua, "x");
		lua_gettable(lua, -2);
		x = luaL_checkinteger(lua, -1);

		lua_pushstring(lua, "y");
		lua_gettable(lua, -3);
		y = luaL_checkinteger(lua, -1);

		lua_pushstring(lua, "width");
		lua_gettable(lua, -4);
		w = luaL_checkinteger(lua, -1);

		lua_pushstring(lua, "height");
		lua_gettable(lua, -5);
		h = luaL_checkinteger(lua, -1);

		lua_pushstring(lua, "yoffset");
		lua_gettable(lua, -6);
		yoffset = luaL_checkinteger(lua, -1);

#ifdef POOL_PARTY_WORKAROUND
		al_draw_filled_rectangle(xadvance, 1 + padding, xadvance + w, 1 + max_height + yoffset, al_map_rgba(0, 0, 0, 0));
		al_draw_bitmap_region(bitmap, x, y, w, h, xadvance, 1 + yoffset + padding, 0);
#else
		al_draw_filled_rectangle(xadvance, 1, xadvance + w, 1 + max_height, al_map_rgba(0, 0, 0, 0));
		al_draw_bitmap_region(bitmap, x, y, w, h, xadvance, 1 + yoffset, 0);
#endif

		xadvance += w + 1;

		lua_pop(lua, 6);
	}

#ifdef EMILY_OUTPUT
	strcpy_s(file_name_buffer, 256, "res/fonts/error/");
	strcat_s(file_name_buffer, 256, font_name);
	strcat_s(file_name_buffer, 256, ".bmp");

	al_save_bitmap(file_name_buffer, bitmap_font);
#endif

	int ranges[2] = { 32,126 };

	ALLEGRO_FONT* output = al_grab_font_from_bitmap(bitmap_font, 1, ranges);

	al_destroy_bitmap(bitmap);
	al_destroy_bitmap(bitmap_font);

	return output;
}

void resource_manager_init()
{
	// I feel like we will eventually want differnt options for text versus icons.
	// But such problems aren't manifesting yet.
	al_set_new_bitmap_flags(ALLEGRO_MIN_LINEAR | ALLEGRO_MAG_LINEAR | ALLEGRO_MIPMAP | ALLEGRO_VIDEO_BITMAP);

#ifdef EMILY_OUTPUT
	al_make_directory("res/fonts/error/");
#endif
	lua_State* lua = luaL_newstate();
	luaL_openlibs(lua);

	for (size_t i = 0; i < FONT_ID_COUNT; i++)
		font_table[i] = emily_huo_font(lua, font_names[i]);

	lua_close(lua);
}

ALLEGRO_FONT* resource_manager_font(enum font_id id)
{
	return font_table[id];
}

ALLEGRO_BITMAP* resource_manager_icon(enum icon_id id)
{
	if (!icon_table[id])
	{
		char file_name_buffer[256];
		sprintf_s(file_name_buffer, 256, "res/icons/%d.png", id);

		icon_table[id] = al_load_bitmap(file_name_buffer);
	}

	return icon_table[id];
}

ALLEGRO_BITMAP* resource_manager_tile(enum tile_id id)
{
	if (!tile_table[id])
	{
		char file_name_buffer[256];
		sprintf_s(file_name_buffer, 256, "res/tiles/%d.png", id);

		tile_table[id] = al_load_bitmap(file_name_buffer);
	}

	return tile_table[id];
}
