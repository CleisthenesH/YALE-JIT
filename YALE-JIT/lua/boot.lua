-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

local function exit_main_menu()
	widgets.remove(play_button)
	play_button = nil

	widgets.remove(edit_button)
	edit_button = nil

	widgets.remove(hud_button)
	hud_button = nil
end

play_button = button{x=500,y=500,text = "Play",left_click = function(wg) exit_main_menu() dofile("lua/game.lua") end}
edit_button = button{x=500,y=600,text = "Edit",left_click = function(wg) exit_main_menu() dofile("lua/edit.lua") end}
hud_button = button{x=500,y=700,text = "HUD test",left_click = function(wg) exit_main_menu() dofile("lua/HUD_test.lua") end}

print("Boot Complete")

