// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include "keyframe.h"
#include <allegro5/allegro_font.h>

/*********************************************/
/*            Widget Class structs           */
/*********************************************/

struct wg_base
{
	struct keyframe;
	double half_width, half_height;

	bool draggable;
	bool snappable;
};

struct wg_zone
{
	struct wg_base;

	bool valid_move;
	bool highlighted;
	bool nominated;
};

struct wg_piece
{
	struct wg_base;
};

struct wg_frame
{
	struct wg_base;

	struct widget_pallet* pallet;
};

struct wg_hud
{
	struct wg_base;

	struct widget_pallet* pallet;

	enum {
		HUD_IDLE,
		HUD_HOVER,
		HUD_ACTIVE
	} hud_state;
};

/*********************************************/
/*             Widget Jumptables             */
/*********************************************/

struct wg_jumptable_base
{
	const char* type;

	void (*draw)(const struct wg_base* const);
	void (*mask)(const struct wg_base* const);

	void (*event_handler)(struct wg_base* const);

	void (*hover_start)(struct wg_base* const);
	void (*hover_end)(struct wg_base* const);

	void (*left_click)(struct wg_base* const);
	void (*left_held)(struct wg_base* const);
	void (*left_release)(struct wg_base* const);
	void (*right_click)(struct wg_base* const);
	void (*click_off)(struct wg_base* const);

	void (*drag_start)(struct wg_base* const);
	void (*drag_end_no_drop)(struct wg_base* const);
	void (*drag_end_drop)(struct wg_base* const, struct wg_base* const);
	void (*drop_start)(struct wg_base* const, struct wg_base* const);
	void (*drop_end)(struct wg_base* const, struct wg_base* const);

	void (*gc)(struct wg_base* const);
	int (*index)(struct lua_State* L);
	int (*newindex)(struct lua_State* L);
};

struct wg_jumptable_zone
{
	struct wg_jumptable_base;

	void (*highlight_start)(struct wg_zone* const);
	void (*highlight_end)(struct wg_zone* const);

	void (*remove_piece)(struct wg_zone* const, struct wg_piece* const);
	void (*append_piece)(struct wg_zone* const, struct wg_piece* const);
};

struct wg_jumptable_piece
{
	struct wg_jumptable_base;
};

struct wg_jumptable_frame
{
	struct wg_jumptable_base;
};

struct wg_jumptable_hud
{
	struct wg_jumptable_base;
};

/*********************************************/
/*             Widget Allocations            */
/*********************************************/

struct wg_zone* wg_alloc_zone(size_t, struct wg_jumptable_zone*);
struct wg_piece* wg_alloc_piece(size_t, struct wg_jumptable_piece*);
struct wg_frame* wg_alloc_frame(size_t, struct wg_jumptable_frame*);
struct wg_hud* wg_alloc_hud(size_t, struct wg_jumptable_hud*);

/*********************************************/
/*              Widget HUD Pallets           */
/*********************************************/

struct widget_pallet
{
	ALLEGRO_COLOR main;
	ALLEGRO_COLOR recess;
	ALLEGRO_COLOR highlight;

	ALLEGRO_COLOR edge;
	double edge_width, edge_radius;

	ALLEGRO_COLOR activated;
	ALLEGRO_COLOR deactivated;

	ALLEGRO_FONT* font;
};

struct widget_pallet primary_pallet, secondary_pallet;

/*********************************************/
/*               Miscellaneous               */
/*********************************************/

struct wg_base* check_widget_lua(int, const struct wg_jumptable_base* const);
void widget_screen_to_local(const struct wg_base* const wg, double* x, double* y);

// TODO: remove
void stack_dump(struct lua_State*);
