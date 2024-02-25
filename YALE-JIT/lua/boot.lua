-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

--dofile("lua/HUD_test.lua")

tile_a = zones:tile{x=200,y=200,q=1,r=0,tile="hills"}
tile_b = zones:tile{x=400,y=200,q=0,r=0,tile="hills"}

meeple = pieces:meeple{x=300,y=200}

function moves(piece,zone)
	return {tile_a,tile_b}
end


--[[
dofile("lua/main_menu.lua")
dofile("lua/board.lua")

--play_button:left_click()
--hud_button:left_click()
--vfx_button:left_click()
--edit_button:left_click()
--]]

print("Boot Complete")

