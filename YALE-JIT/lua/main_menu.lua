-- Copyright 2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Running this file transitions to the main menu screen

local function exit_main_menu()
	widgets.remove(play_button)
	play_button = nil

	widgets.remove(edit_button)
	edit_button = nil

	widgets.remove(hud_button)
	hud_button = nil

	widgets.remove(vfx_button)
	vfx_button = nil
end



play_button = button{x=display_width*0.5,y=500,text = "Play",left_click = function(wg) exit_main_menu() return dofile("lua/game.lua") end}
edit_button = button{x=display_width*0.5,y=600,text = "Edit",left_click = function(wg) exit_main_menu() return dofile("lua/edit.lua") end}
hud_button = button{x=display_width*0.5,y=700,text = "HUD test",left_click = function(wg) exit_main_menu() return dofile("lua/HUD_test.lua") end}
vfx_button = button{x=display_width*0.5,y=800,text = "vfx test",left_click = function(wg) exit_main_menu() return dofile("lua/vfx_test.lua") end}
