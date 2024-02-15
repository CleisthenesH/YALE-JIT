-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Running this file transitions to the main game screen

function moves(piece,zone)
	local output = {}

	if zone == nil or zone.type ~= type_tile then
		return tiles_filter(function(x) return #x.pieces == 0 end)
	end

	local q = zone.q
	local r = zone.r

	return tiles_neighbours(q,r)
end

if not board_present then
	board_import("save/save.lua")
	board_present = true
end

camera_set{x = display_width*0.5, y = display_height*0.5}
camera_push{x = display_width*0.5, y = display_height*0.5,a=1,t=2}
