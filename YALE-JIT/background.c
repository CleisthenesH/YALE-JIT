// Copyright 2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// A simple background for testing
#include "material.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_color.h>

#include <string.h>

extern ALLEGRO_TRANSFORM identity_transform;

static union
{
	struct
	{
		ALLEGRO_BITMAP* bitmap;
		double x, y;
	} alex;
	struct
	{
		ALLEGRO_BITMAP* bitmap;
	} ruler;
} data;

static void soild_color_draw()
{
	al_clear_to_color(al_color_name("aliceblue"));
}

static void ruler_draw()
{
	al_draw_bitmap(data.ruler.bitmap, 0, 0, 0);
}

static void ruler_enter()
{
	ALLEGRO_DISPLAY* display = al_get_current_display();
	data.ruler.bitmap = al_create_bitmap(al_get_display_width(display), al_get_display_height(display));

	al_set_target_bitmap(data.ruler.bitmap);

	al_clear_to_color(al_color_name("aliceblue"));
	unsigned int i, j;

	const unsigned int height = al_get_display_height(display) / 10 + 10;
	const unsigned int width = al_get_display_width(display) / 10 + 10;

	for (i = 0; i <= width; i++)
		for (j = 0; j <= height; j++)
			if (i % 10 == 0 && j % 10 == 0)
				al_draw_circle(10 * i, 10 * j, 2, al_color_name("darkgray"), 2);
			else
				al_draw_circle(10 * i, 10 * j, 1, al_color_name("grey"), 0);
}

static void ruler_exit()
{
	al_destroy_bitmap(data.ruler.bitmap);
}

static void alex_draw()
{
	al_clear_to_color(al_map_rgb(255, 255, 255));
	al_draw_bitmap(data.alex.bitmap, data.alex.x, data.alex.y, 0);
}

static void alex_enter()
{
	ALLEGRO_DISPLAY* display = al_get_current_display();

	data.alex.bitmap = al_load_bitmap("res/alex.bmp");
	data.alex.x = 0.5 * (al_get_display_width(display) - al_get_bitmap_width(data.alex.bitmap));
	data.alex.y = 0.5 * (al_get_display_height(display) - al_get_bitmap_height(data.alex.bitmap));
}

static void alex_exit()
{
	al_destroy_bitmap(data.alex.bitmap);
}

enum background_mode
{
	BACKGROUND_SOLID_COLOR,
	BACKGROUND_RULER,
	BACKGROUND_ALEX,

	BACKGROUND_CNT,
	BACKGROUND_INVALID
} background_mode;

static struct {
	const char* name;
	void (*draw)();
	void (*enter)();
	void (*exit)();
} mode_table[BACKGROUND_CNT] = 
{
	{"solid_color",soild_color_draw,NULL,NULL},
	{"ruler",ruler_draw,ruler_enter,ruler_exit},
	{"alex",alex_draw,alex_enter,alex_exit}
};

static int lua_backgroundmode(lua_State* L)
{
	enum background_mode target_mode = BACKGROUND_INVALID;

	if (lua_isnumber(L, -1))
	{
		target_mode = lua_tointeger(L, -1);

		if (target_mode < 0 || target_mode > BACKGROUND_CNT)
			return -1;
	}
	else if (lua_isstring(L, -1))
	{
		const char* mode = lua_tostring(L, -1);

		for (enum background_mode i = 0; i < BACKGROUND_CNT; i++)
			if (strcmp(mode, mode_table[i].name) == 0)
			{
				target_mode = i;
				break;
			}
	}
	else
		return -1;

	if (target_mode == BACKGROUND_INVALID)
		return -1;

	if (target_mode == background_mode)
		return 0;

	if (mode_table[background_mode].exit)
		mode_table[background_mode].exit();

	if (mode_table[target_mode].enter)
		mode_table[target_mode].enter();

	background_mode = target_mode;

	return 0;
}

void background_init(lua_State* L, ALLEGRO_DISPLAY* display)
{
	background_mode = BACKGROUND_ALEX;

	if(mode_table[background_mode].enter)
		mode_table[background_mode].enter();

	lua_pushcfunction(L, lua_backgroundmode);
	lua_setglobal(L, "background");
}

void background_draw()
{
	al_use_transform(&identity_transform);
	material_apply(NULL);

	mode_table[background_mode].draw();
}
