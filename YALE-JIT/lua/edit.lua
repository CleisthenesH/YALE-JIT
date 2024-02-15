-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Running this file transitions to the edit mode screen

if not board_present then
	default_board()
	board_present = true
end

camera_set{x = display_width*0.5, y = display_height*0.5}

function exit_edit_mode()
	widgets.remove(save_button)
	widgets.remove(load_button)
	widgets.remove(tile_selector)

	save_button = nil
	load_button = nil
	tile_selector = nil
end

save_button = button{x = display_width - 64, y = display_height - 32, text="Save"}
load_button = button{x = display_width - 64, y = display_height - 96, text="Load"}
tile_selector = tile_selector{x=display_width*0.5, y = display_height-100.0}

function save_button:left_click()
	local file_path = native_file_dialog{"save",patterns="*.*;*.lua;*.LUA"}

	if(file_path:sub(-4):upper() ~= ".LUA") then
		file_path = file_path .. ".lua"
	end

	if file_path then
		board_export(file_path)
	end
end

function load_button:left_click()
	local file_path = native_file_dialog{patterns="*.*;*.lua;*.LUA"}

	if file_path then
		board_import(file_path)
	end
end

function tile_selector:left_click()
	if selected_tile then
		selected_tile.tile = self.hover
	end
end

for _, tile in ipairs(tiles_flatten()) do
	function tile:left_click()
		selected_tile = self
	end
end
