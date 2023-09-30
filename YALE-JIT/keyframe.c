// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "keyframe.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

extern lua_State* lua_state;
extern double current_timestamp;

void keyframe_build_transform(const struct keyframe* const keyframe, ALLEGRO_TRANSFORM* const trans)
{
	al_build_transform(trans,
		keyframe->x, keyframe->y,
		keyframe->sx, keyframe->sy,
		keyframe->a);

	al_translate_transform(trans, keyframe->dx, keyframe->dy);
}

void keyframe_default(struct keyframe* const keyframe)
{
	*keyframe = (struct keyframe)
	{
		.sx = 1,
		.sy = 1,
	};
}

void keyframe_copy(struct keyframe* const dest, const struct keyframe* const src)
{
	memcpy_s(dest, sizeof(struct keyframe), src, sizeof(struct keyframe));
}

void keyframe_blend(struct keyframe* const dest, const struct keyframe* const a, const struct keyframe* const b, double blend)
{
	dest->t  = a->t  * blend + b->t  * (1 - blend);
	dest->x  = a->x  * blend + b->x  * (1 - blend);
	dest->y  = a->y  * blend + b->y  * (1 - blend);
	dest->sx = a->sx * blend + b->sx * (1 - blend);
	dest->sy = a->sy * blend + b->sy * (1 - blend);
	dest->a  = a->a  * blend + b->a  * (1 - blend);
	dest->t  = a->t  * blend + b->t  * (1 - blend);
	dest->dx = a->dx * blend + b->dx * (1 - blend);
	dest->dy = a->dy * blend + b->dy * (1 - blend);
}

// Read a transform from the top of the stack to a pointer
void lua_getkeyframe(int idx, struct keyframe* const keyframe)
{
	lua_getfield(lua_state, idx, "t");
	if(lua_type(lua_state,-1) == LUA_TNUMBER)
		keyframe->t = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-1, "x");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->x = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-2, "y");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->y = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-3, "sx");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->sx = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-4, "sy");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->sy = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-5, "a");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->a = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx - 6, "c");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->c = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-7, "dx");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->dx = lua_tonumber(lua_state, -1);

	lua_getfield(lua_state, idx-8, "dy");
	if (lua_type(lua_state, -1) == LUA_TNUMBER)
		keyframe->dy = lua_tonumber(lua_state, -1);

	keyframe->t += current_timestamp;

	lua_pop(lua_state, 9);
}

// Set the field of a table at idx to the keyframe memebers.
void lua_setkeyframe(int idx, const struct keyframe* const keyframe)
{
	lua_pushnumber(lua_state, keyframe->t);
	lua_setfield(lua_state, idx - 1, "t");
	lua_pushnumber(lua_state, keyframe->x);
	lua_setfield(lua_state, idx - 1, "x");
	lua_pushnumber(lua_state, keyframe->y);
	lua_setfield(lua_state, idx - 1, "y");
	lua_pushnumber(lua_state, keyframe->sx);
	lua_setfield(lua_state, idx - 1, "sx");
	lua_pushnumber(lua_state, keyframe->sy);
	lua_setfield(lua_state, idx - 1, "sy");
	lua_pushnumber(lua_state, keyframe->a);
	lua_setfield(lua_state, idx - 1, "a");
	lua_pushnumber(lua_state, keyframe->c);
	lua_setfield(lua_state, idx - 1, "c");
	lua_pushnumber(lua_state, keyframe->dx);
	lua_setfield(lua_state, idx - 1, "dx");
	lua_pushnumber(lua_state, keyframe->dy);
	lua_setfield(lua_state, idx - 1, "dy");
}

// Removes keyframe keys from table at idx.
void lua_cleankeyframe(int idx)
{
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "t");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "x");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "y");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "sx");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "sy");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "a");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "c");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "dx");
	lua_pushnil(lua_state);
	lua_setfield(lua_state, idx - 1, "dy");
}