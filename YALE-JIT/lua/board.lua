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

function board_export(filename)
	local file = io.open("save/save.lua","w")
	io.output(file)

	io.write("tiles = {".. string.char(10))
	for k, v in pairs(widgets.filter(function(wg) return wg.type == type_tile end)) do
		io.write("   tile{x=" .. v.x .. ",y=" .. v.y)
		io.write(",q=" .. v.q .. ",r=" .. v.r)
		io.write([[,tile="]] .. v.tile .. [["},]] .. string.char(10))
	end

	io.write("}".. string.char(10))
	
	io.write(string.char(10))
	io.write("red_meeple = meeple{x=" .. red_meeple.x .. ",y= " .. red_meeple.y ..[[,team="red"}]]..string.char(10))
	io.write("blue_meeple = meeple{x=" .. blue_meeple.x .. ",y= " .. blue_meeple.y ..[[,team="blue"}]]..string.char(10))

	io.write(string.char(10))
	io.write("manual_move(red_meeple, get_tile(" .. red_meeple.zone.q .. ", " .. red_meeple.zone.r .."))"..string.char(10))
	io.write("manual_move(blue_meeple, get_tile(" .. blue_meeple.zone.q .. ", " .. blue_meeple.zone.r .."))")
	
	io.close()
end

function enter_edit_mode()
	edit_mode = true
	edit_button.left_click = exit_edit_mode

	save_button = button{x = edit_button.x, y = edit_button.y+ 64, text="Save"}
	save_button.left_click = function(wg) board_export("save/save.lua") end
end

function exit_edit_mode()
	edit_mode = nil
	save_button = nil
	collectgarbage()
	edit_button.left_click = enter_edit_mode
end

function get_tile(q,r)
	local filter = widgets.filter(function(wg) return wg.type == type_tile and wg.q == q and wg.r == r end)

	return #filter > 0 and filter[1]
end

local file = io.open("save/save.lua","r")

if file then
	dofile("save/save.lua")
	io.close(file)	
else
	local size = 50

	tiles = {}

	for q = -4,4 do
		for r = -4,4 do	
			if math.abs(q+r) < 5 then
				local dx = 1.1*size*math.sqrt(3)*(q+0.5*r)
				local dy = 1.1*size*1.5*r

				 tiles[#tiles+1]= tile{q=q,r=r,
					x=800+dx,y=600+dy,
					tile = "hills"}
			end
		end
	end

	red_meeple = meeple{x=750, y=200, team = "red"}
	blue_meeple = meeple{x=850, y=200, team = "blue"}

	manual_move(red_meeple, get_tile(3,0))
	manual_move(blue_meeple, get_tile(-3,0))
end

for k, v in pairs(tiles) do
	v.left_click = left_click
	v.right_click = right_click
end
