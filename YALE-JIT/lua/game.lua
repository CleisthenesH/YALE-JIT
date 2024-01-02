-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- game logic main file

function moves(piece,zone)
	local output = {}

	if zone == nil or zone.type ~= type_tile then
		return tiles_filter(function(x) return #x.pieces == 0 end)
	end

	local q = zone.q
	local r = zone.r

	return tiles_neighbours(q,r)
end

dofile("lua/board.lua")

board_import("save/save.lua")
