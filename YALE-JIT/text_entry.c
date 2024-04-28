// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
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
	struct wg_hud;

	size_t input_size;
	char input[256];	

	void (*on_enter)(void*);

	char place_holder[];
};

static void draw(const struct wg_base* const wg)
{
	const struct text_entry* const text_entry = (const struct text_entry* const)wg;
	const struct widget_pallet* const pallet = text_entry->pallet;

	const double text_left_padding = 10;

	// Clear the stencil buffer channels
	glEnable(GL_STENCIL_TEST);
	glStencilMask(0x03);

	// Set Stencil Function to set stencil channels to 1 
	glStencilFunc(GL_ALWAYS, 1, 0x03);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	ALLEGRO_COLOR fill;

	if (text_entry->hud_state == HUD_IDLE)
		fill = pallet->recess;
	else if(text_entry->hud_state == HUD_HOVER)
		fill = pallet->main;
	else
		fill = pallet->highlight;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh,
		wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		fill);

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// Draw all the interior boarders (where stencil is 2)
	glStencilFunc(GL_EQUAL, 1, 0x03);

	if (text_entry->input_size == 0)
	{
		al_draw_text(pallet->font, pallet->deactivated,
			-wg->hw + text_left_padding,-16, 0,
			text_entry->place_holder);
	}
	else {
		const auto text_width = al_get_text_width(pallet->font, text_entry->input);

		if (text_width > 2 * wg->hw - text_left_padding)
			al_draw_text(pallet->font, pallet->activated,
				wg->hw - text_width, -16,
				0, text_entry->input);
		else
			al_draw_text(pallet->font, pallet->activated,
				-wg->hw + text_left_padding, -16,
				0, text_entry->input);
	}

	glDisable(GL_STENCIL_TEST);

	al_draw_rounded_rectangle(-wg->hw, -wg->hh,
		wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		pallet->edge, pallet->edge_width);
}

static void mask(const struct wg_base* const wg)
{
	const struct text_entry* const text_entry = (const struct text_entry* const)wg;
	const struct widget_pallet* const pallet = text_entry->pallet;

	al_draw_filled_rounded_rectangle(-wg->hw, -wg->hh, wg->hw, wg->hh,
		pallet->edge_radius, pallet->edge_radius,
		al_map_rgb(255, 0, 0));
}

static void drag_start(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;
	
	text_entry->hud_state = HUD_ACTIVE;
}

static void left_click(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	text_entry->hud_state = HUD_ACTIVE;
}

static void click_off(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	text_entry->hud_state = HUD_IDLE;
}

static void event_handler(struct wg_base* const wg)
{
	struct text_entry* const text_entry = (const struct text_entry* const)wg;

	if (text_entry->hud_state != HUD_ACTIVE)
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
	.type = "text_entry",

	.draw = draw,
	.mask = mask,
	.event_handler = event_handler,

	.left_click = left_click,
	.drag_start = drag_start,
	.click_off = click_off
};

int text_entry_new(lua_State* L)
{
	if (!lua_istable(L, -1))
		lua_newtable(L);

	// Set default text.
	// Also retreve paramaters for size allocation.
	size_t text_len = 21;
	char* text = "Click to enter text.";

	lua_getfield(L, -1, "text");

	if (lua_isstring(L, -1))
		text = lua_tolstring(L, -1, &text_len);
	else
	{
		lua_pushstring(L, "Click to enter text.");
		lua_setfield(L, -3, "text");
	}

	lua_pop(L, 1);

	// Set default hh.
	lua_getfield(L, -1, "hh");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 25);
		lua_setfield(L, -3, "hh");
	}

	lua_pop(L, 1);

	// Set default hw.
	lua_getfield(L, -1, "hw");

	if (!lua_isnumber(L, -1))
	{
		lua_pushnumber(L, 300);
		lua_setfield(L, -3, "hw");
	}

	lua_pop(L, 1);

	const size_t size = sizeof(struct text_entry) + sizeof(char) * (text_len + 1);
	struct text_entry* text_entry = (struct text_entry*)wg_alloc_hud(size, &text_entry_jumptable);

	strcpy_s(text_entry->place_holder, text_len + 1, text);

	text_entry->input[0] = '\0';
	text_entry->input_size = 0;

	return 1;
}
