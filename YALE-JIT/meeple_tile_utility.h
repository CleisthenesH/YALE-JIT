// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <allegro5/allegro_color.h>

static const char* tile_to_string[] = {
	"empty",
	"bridge",
	"camp",
	"castle",
	"city",
	"dungeon",
	"farm",
	"fort",
	"hills",
	"lake",
	"mine",
	"monolith",
	"mountains",
	"oak",
	"oaks",
	"pine",
	"pines",
	"poi",
	"quest",
	"ruins",
	"shipwreck",
	"skull",
	"swamp",
	"tower",
	"town"
};

enum TEAMS
{
	TEAM_NONE,
	TEAM_RED,
	TEAM_BLUE,

	TEAM_CNT
};

enum TILE_PALLET
{
	TILE_PALLET_IDLE,
	TILE_PALLET_HIGHLIGHTED,
	TILE_PALLET_NOMINATED,

	TILE_PALLET_CNT
};

enum TEAMS lua_toteam(struct lua_State* L, int idx);
enum tile_id lua_toid(struct lua_State* L, int idx);
