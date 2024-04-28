-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

--dofile("lua/HUD_test.lua")

frame = hud:frame{x=200,y=200,hw=50,hh=50}

button = frame:button{x=200,y=200, text="Test"}
slider = frame:slider{x=500,y=200}
text_entry = frame:text_entry{x=700,y=300}

tile_a = board:tile{x=200,y=200,tile="hills"}
tile_b = board:tile{x=200,y=300,tile="hills"}
tile_c = board:tile{x=200,y=400,tile="hills"}

tile_a:meeple{x=300,y=200}

function board.moves(piece,zone)
	return {tile_a,tile_b}
end

--[[
tile_a = board:tile{x=200,y=200,q=1,r=0,tile="hills"}
tile_b = board:tile{x=400,y=200,q=0,r=0,tile="hills"}
meeple = board:meeple{x=300,y=200}

button = hud:button{x=500,y=500}

sub_meeple = tile_a:meeple{x=200,y=200}

function moves(piece,zone)
	return {tile_a,tile_b}
end

function button:left_click()
	tile_b:push_keyframe{x=400,y=400,t=2,c=1}
end
--]]


--[[
dofile("lua/main_menu.lua")
dofile("lua/board.lua")

--play_button:left_click()
--hud_button:left_click()
--vfx_button:left_click()
--edit_button:left_click()
--]]

print("Boot Complete")

