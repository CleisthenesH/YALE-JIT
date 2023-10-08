-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

function moves(piece,zone)
	widgets.mask(true,false,false)

	local output = {}

	if zone == nil or zone.type ~= type_tile then
		return widgets.filter(function(wg) return wg.type == type_tile and #wg.pieces == 0 end)
	end

	local q = zone.q
	local r = zone.r

	local function is_neighbour(wg)
		if wg.type ~= type_tile or #wg.pieces > 0 then
			return false
		end

		local dq = wg.q - q
		local dr = wg.r - r

		return math.abs(dr+dq) <= 1 
			and math.abs(dr) <= 1 
			and math.abs(dq) <= 1
	end

	return widgets.filter(is_neighbour)
end

dofile("lua/board.lua")

wood_counter  = counter{x=70,  y=70, icon=2206, value = 234}
stone_counter = counter{x=70, y=190, icon=3455, value = 234}
gold_counter  = counter{x=70, y=310, icon=2562, value = 234}

local item = scheduler.push(5.0, function() print("test 5") end)
local item2 = scheduler.push(7.0, function() print("test 7") end)
local item3 = scheduler.push(6.0, function() print("test 6") end)

print("Boot Complete")

