// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "widget.h"
#include "resource_manager.h"

#include <lua.h>

#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_opengl.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_color.h>

extern ALLEGRO_EVENT current_event;

#define WIDGET_TYPE text_entry

struct text_entry {
	struct wg_base;

	enum {
		TEXT_ENTRY_IDLE,
		TEXT_ENTRY_HOVER,
		TEXT_ENTRY_ACTIVE
	} state;

	ALLEGRO_FONT* font;

	size_t input_size;
	char input[256];	

	void (*on_enter)(void*);

	char place_holder[];
};

static void draw(const struct wg_base* const wg)
{
	const struct text_entry* const text_entry = (const struct text_entry* const)wg;

	const double text_left_padding = 10;

	// Clear the stencil buffer channels
	glEnable(GL_STENCIL_TEST);
	glStencilMask(0x03);

	// Set Stencil Function to set stencil channels to 1 
	glStencilFunc(GL_ALWAYS, 1, 0x03);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	ALLEGRO_COLOR fill;

	if (text_entry->state == TEXT_ENTRY_IDLE)
		fill = primary_pallet.recess;
	else if(text_entry->state == TEXT_ENTRY_HOVER)
		fill = primary_pallet.main;
	else
		fill = primary_pallet.highlight;

	al_draw_filled_rounded_rectangle(-wg->half_width, -wg->half_height,
		wg->half_width, wg->half_height,
		primary_pallet.edge_radius, primary_pallet.edge_radius,
		fill);

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Draw all the interior boarders (where stencil is 2)
	glStencilFunc(GL_EQUAL, 1, 0x03);

	if (text_entry->input_size == 0)
	{
		al_draw_text(text_entry->font, primary_pallet.deactivated,
			-wg->half_width + text_left_padding,-16, 0,
			text_entry->place_holder);
	}
	else {
		const auto text_width = al_get_text_width(text_entry->font, text_entry->input);

		if (text_width > 2 * wg->half_width - text_left_padding)
			al_draw_text(text_entry->font, primary_pallet.activated,
				wg->half_width - text_width, -16,
				0, text_entry->input);
		else
			al_draw_text(text_entry->font, primary_pallet.activated,
				-wg->half_width + text_left_padding, -16,
				0, text_entry->input);
	}

	glDisable(GL_STENCIL_TEST);

	al_draw_rounded_rectangle(-wg->half_width, -wg->half_height,
		wg->half_width, wg->half_height,
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
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	if (text_entry->state == TEXT_ENTRY_IDLE)
		text_entry->state = TEXT_ENTRY_HOVER;
}

static void hover_end(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;
	
	if (text_entry->state == TEXT_ENTRY_HOVER)
		text_entry->state = TEXT_ENTRY_IDLE;
}

static void drag_start(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;
	
	text_entry->state = TEXT_ENTRY_ACTIVE;
}

static void left_click(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	text_entry->state = TEXT_ENTRY_ACTIVE;
}

static void click_off(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	text_entry->state = TEXT_ENTRY_IDLE;
}

static void event_handler(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	if (text_entry->state != TEXT_ENTRY_ACTIVE)
		return;

	if (current_event.type == ALLEGRO_EVENT_KEY_CHAR)
	{
		if (current_event.keyboard.keycode == ALLEGRO_KEY_ENTER)
		{
			if (text_entry->on_enter)
				text_entry->on_enter(text_entry);
			return;
		}

		if (current_event.keyboard.keycode == ALLEGRO_KEY_BACKSPACE)
		{
			if (text_entry->input_size > 0)
				text_entry->input[--text_entry->input_size] = '\0';

			return;
		}

		if (text_entry->input_size < 255)
		{
			int append_char = -1;

			if (current_event.keyboard.keycode >= ALLEGRO_KEY_A && 
				current_event.keyboard.keycode <= ALLEGRO_KEY_Z)
				append_char = ((current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? 'A' : 'a') +
				(current_event.keyboard.keycode - ALLEGRO_KEY_A);

			if (current_event.keyboard.keycode >= ALLEGRO_KEY_0 && 
				current_event.keyboard.keycode <= ALLEGRO_KEY_9 &&
				!(current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT))
				append_char = '0' + (current_event.keyboard.keycode - ALLEGRO_KEY_0);

			if (current_event.keyboard.keycode == ALLEGRO_KEY_SPACE)
				append_char = ' ';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_EQUALS)
				append_char = '=';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_OPENBRACE)
				append_char = (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? '[' : '{';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_CLOSEBRACE)
				append_char = (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? ']' : '}';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_SEMICOLON)
				append_char = ':';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_FULLSTOP)
				append_char = '.';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_COMMA)
				append_char = ',';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_QUOTE)
				append_char = '"';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_MINUS)
				append_char = (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? '_' : '-';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_9)
				append_char = (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? '(' : '9';

			if (current_event.keyboard.keycode == ALLEGRO_KEY_0)
				append_char = (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? ')' : '0';

			if (append_char != -1)
			{
				text_entry->input[text_entry->input_size++] = append_char;
				text_entry->input[text_entry->input_size] = '\0';
			}
		}
	}
}

const struct wg_jumptable_base text_entry_jumptable =
{
	.draw = draw,
	.mask = mask,
	.event_handler = event_handler,

	.left_click = left_click,
	.hover_start = hover_start,
	.hover_end = hover_end,
	.drag_start = drag_start,
	.click_off = click_off
};

int text_entry_new(lua_State* L)
{
	// Get the text len so we know how munch memory to alloc
	size_t text_len = 21;
	char* text = NULL;

	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "text");

		if (lua_isstring(L, -1))
			text = lua_tolstring(L, -1, &text_len);

		lua_pop(L, 1);
	}

	const size_t size = sizeof(struct text_entry) + sizeof(char) * (text_len + 1);

	struct text_entry* text_entry = (struct text_entry*)wg_alloc(WG_BASE, size, &text_entry_jumptable);

	strcpy_s(text_entry->place_holder, text_len + 1, text ? text : "Click to enter text.");

	text_entry->input[0] = '\0';
	text_entry->input_size = 0;
	text_entry->state = TEXT_ENTRY_IDLE;
	text_entry->font = resource_manager_font(FONT_ID_SHINYPEABERRY);

	text_entry->half_width = 300 > text_entry->half_width ? 300 : text_entry->half_width;
	text_entry->half_height = 20 > text_entry->half_height ? 20 : text_entry->half_height;

	return 1;
}
