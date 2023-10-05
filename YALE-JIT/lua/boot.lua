-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

function moves(piece,zone)
	widgets.mask(false,true,false)

	local output = {}

	if zone == nil or zone.type ~= type_tile then
		return widgets.filter(function(wg) return wg.type == type_tile and #wg.pieces == 0 end)
	end

	local q = zone.q
	local r = zone.r

	local function is_neighbour(wg)
		print(wg.pieces,#wg.pieces)
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

local function axial_to_world(q,r)
	local size = 50
	
	return 1.1*size*math.sqrt(3)*(q+0.5*r),1.1*size*1.5*r
end

for q = -3,3 do
	for r = -3,3 do	
		if math.abs(q+r) < 4 then
			local dx, dy = axial_to_world(q,r)
			tile{q=q,r=r,
				x=800+dx,y=600+dy,
				team =  q < 0 
				and "red" 
				or "blue",
				tile = "hills"}
		end
	end
end

meeple{x=100, y=100, team = "red"}
meeple{x=200, y=100, team = "blue"}

handle = counter{x=100,y=500,icon=2206}

handle:set(1)
handle:add(1)

handle.value = 3

print("Boot Complete")

