-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- defines the board state and utility functions

local function left_click(wg)
	if edit_mode then
		wg.tile_id = wg.tile_id + 1
	end
end

local function right_click(wg)
	if edit_mode then
		wg.tile_id = wg.tile_id - 1
	end
end

local size = 50

for q = -4,4 do
	for r = -4,4 do	
		if math.abs(q+r) < 5 then
			local dx = 1.1*size*math.sqrt(3)*(q+0.5*r)
			local dy = 1.1*size*1.5*r

			random_tile = tile{q=q,r=r,
				x=800+dx,y=600+dy,
				left_click = left_click,
				right_click = right_click,
				tile = "hills"}
		end
	end
end

function board_to_string()
	s = ""
	for k, v in pairs(widgets.filter(function(wg) return wg.type == type_tile end)) do
		s = s .. "tile{x=" .. v.x .. ",y=" .. v.y 
			.. ",q=" .. v.q .. ",r=" .. v.r
			..[[,tile="]] .. v.tile .. [["},]]
	end

	return s
end

print(board_to_string())

function enter_edit_mode()
	edit_mode = true
	edit_button.left_click = exit_edit_mode
end

function exit_edit_mode()
	edit_mode = nil
	edit_button.left_click = enter_edit_mode
end

function get_tile(q,r)
	local filter = widgets.filter(function(wg) return wg.type == type_tile and wg.q == q and wg.r == r end)

	print(#filter, filter[1])
	if #filter > 0 then
		return filter[1]
	end

	return nil
end

local red = meeple{x=750, y=200, team = "red"}
local blue = meeple{x=850, y=200, team = "blue"}

manual_move(red, get_tile(3,0))
manual_move(blue, get_tile(-3,0))
