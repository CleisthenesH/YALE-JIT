-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- defines all the board states

local function x(q,r)
	local size = 50
	
	return 1.1*size*math.sqrt(3)*(q+0.5*r),1.1*size*1.5*r
end

for q = -3,3 do
	for r = -3,3 do	
		if math.abs(q+r) < 4 then
			local dx, dy = x(q,r)
			random_tile = tile{q=q,r=r,
				x=800+dx,y=600+dy,
				team =  q < 0 
				and "red" 
				or "blue",
				tile = "hills"}
		end
	end
end

--[[
local tile_size = 50

local function x(q,r)	
	return 1.1*tile_size*math.sqrt(3)*(q+0.5*r)
end

local function y(q,r)	
	return 1.1*tile_size*1.5*r
end

tile_0_0 = tile{q= 0 ,r= 0, x=800+x( 0, 0), y=600+y( 0, 0), team = "blue", tile = "hills"}
tile_1_0 = tile{q= 1 ,r= 0, x=800+x( 1, 0), y=600+y( 1, 0), team = "blue", tile = "hills"}

function board_reveal(stage)
	if stage == 1 then
		tile_1_1 = tile{q= 1 ,r= 1, x=800+x( 1, 1), y=600+y( 1, 1), team = "blue", tile = "hills"}
	elseif stage == 2 then
		tile_m1_1 = tile{q= -1 ,r= 1, x=800+x( -1, 1), y=600+y( -1, 1), team = "blue", tile = "hills"}
	end
end
--]]

meeple{x=750, y=200, team = "red"}
meeple{x=850, y=200, team = "blue"}