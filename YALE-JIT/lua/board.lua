-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- defines board utility functions

function board_export(filename)
	local file = io.open("save/save.lua","w")
	io.output(file)

	io.write("tiles = {[-4]={},[-3]={},[-2]={},[-1]={},[0]={},{},{},{},{}}".. string.char(10))
	for k, v in pairs(widgets.filter(function(wg) return wg.type == type_tile end)) do
		io.write("tiles[" .. v.q .. "][" .. v.r .. "] = ")
		io.write("tile{x=" .. v.x .. ",y=" .. v.y)
		io.write(",q=" .. v.q .. ",r=" .. v.r)
		io.write([[,tile="]] .. v.tile .. [["}]] .. string.char(10))
	end

	io.write(string.char(10))
	
	io.write(string.char(10))
	io.write("red_meeple = meeple{x=" .. red_meeple.x .. ",y= " .. red_meeple.y ..[[,team="red"}]]..string.char(10))
	io.write("blue_meeple = meeple{x=" .. blue_meeple.x .. ",y= " .. blue_meeple.y ..[[,team="blue"}]]..string.char(10))

	io.write(string.char(10))
	io.write("manual_move(red_meeple, get_tile(" .. red_meeple.zone.q .. ", " .. red_meeple.zone.r .."))"..string.char(10))
	io.write("manual_move(blue_meeple, get_tile(" .. blue_meeple.zone.q .. ", " .. blue_meeple.zone.r .."))"..string.char(10))

	io.write(string.char(10))
	io.write("wood_counter  = counter{x=70,  y=70, icon=2206, value = " .. wood_counter.value .. "}"..string.char(10))
	io.write("stone_counter = counter{x=70, y=190, icon=3455, value = " .. stone_counter.value .. "}"..string.char(10))
	io.write("gold_counter  = counter{x=70, y=310, icon=2562, value = " .. gold_counter.value .. "}"..string.char(10))
	
	io.close()
end

function default_board()
	local size = 50

	tiles = {}

	for q = -4,4 do
		tiles[q] = {}

		for r = -4,4 do	
			if math.abs(q+r) < 5 then
				local dx = 1.1*size*math.sqrt(3)*(q+0.5*r)
				local dy = 1.1*size*1.5*r

				 tiles[q][r]= tile{q=q,r=r,
					x=800+dx,y=600+dy,
					tile = "hills"}
			end
		end
	end

	red_meeple = meeple{x=750, y=200, team = "red"}
	blue_meeple = meeple{x=850, y=200, team = "blue"}

	manual_move(red_meeple, get_tile(3,0))
	manual_move(blue_meeple, get_tile(-3,0))

	wood_counter  = counter{x=70,  y=70, icon=2206, value = 0}
	stone_counter = counter{x=70, y=190, icon=3455, value = 0}
	gold_counter  = counter{x=70, y=310, icon=2562, value = 0}
end

function board_import(filename)
	local file = io.open(filename,"r")

	if true and file then
		dofile(filename)
		io.close(file)	
	else
		default_board()
	end
end

function get_tile(q,r)
	return tiles[q][r]
end

function tiles_flatten()
	local output = {}
	for q = -4,4 do
		for r = -4,4 do	
			if math.abs(q+r) <= 4 then
				output[#output+1] = tiles[q][r]
			end
		end
	end

	return output
end

function tiles_filter(funct)
	local output = {}

	for q = -4,4 do
		for r = -4,4 do	
			if  math.abs(q+r) <= 4 and funct(tiles[q][r]) then
				output[#output+1] = tiles[q][r]
			end
		end
	end

	return output	
end

function tiles_neighbours(q,r)
	local output = {}

	for dq = -1,1 do
		for dr = -1,1 do	
			if math.abs(dq+dr) <= 1 and math.abs(r+dr+q+dq) <= 4 then
				output[#output+1] = tiles[q+dq][r+dr]
			end
		end
	end

	return output
end

