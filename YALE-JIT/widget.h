// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include "keyframe.h"

enum wg_class
{
	WG_ZONE,
	WG_PIECE,
	WG_BASE,

	WG_CLASS_CNT
};

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

	size_t allocated;
	size_t used;
	struct wg_piece** pieces;
};

struct wg_piece
{
	struct wg_base;

	struct wg_zone* zone;
};

struct wg_jumptable_base
{
	const char* type;

	void (*draw)(const struct wg_base* const);
	void (*update)(struct wg_base* const);
	void (*event_handler)(struct wg_base* const);
	void (*mask)(const struct wg_base* const);

	void (*hover_start)(struct wg_base* const);
	void (*hover_end)(struct wg_base* const);

	void (*left_click)(struct wg_base* const);
	void (*left_click_end)(struct wg_base* const);
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

struct wg_base* wg_alloc_base(size_t, struct wg_jumptable_base*);
struct wg_zone* wg_alloc_zone(size_t, struct wg_jumptable_zone*);
struct wg_piece* wg_alloc_piece(size_t, struct wg_jumptable_piece*);

struct wg_base* check_widget_lua(int, const struct wg_jumptable_base* const);

// TODO: remove
void stack_dump(struct lua_State*);


// Standard Style
struct widget_pallet
{
	ALLEGRO_COLOR main;
	ALLEGRO_COLOR recess;
	ALLEGRO_COLOR highlight;

	ALLEGRO_COLOR edge;
	double edge_width, edge_radius;

	ALLEGRO_COLOR activated;
	ALLEGRO_COLOR deactivated;
};

struct widget_pallet primary_pallet, secondary_pallet;

struct ALLEGRO_FONT* primary_font;
