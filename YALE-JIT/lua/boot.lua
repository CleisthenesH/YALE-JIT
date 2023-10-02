-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

function moves(piece,zone)
	local output = {}

	if zone == nil then
		for k,v in pairs(widgets) do
			if v.type == "tile" then
				output[#output+1] = v
			end
		end

		return output
	end

	local q = zone.q
	local r = zone.r
	
	for k,v in pairs(widgets) do
		if v.type == "tile" then
			local dq = v.q - q
			local dr = v.r - r
			if math.abs(dr+dq) <= 1 and math.abs(dr) <= 1 and math.abs(dq) <= 1 then
				output[#output+1] = v
			end
		end
	end

	return output
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

meeple{x=100, y=100}
meeple{x=100, y=100}

print("Boot Complete")

