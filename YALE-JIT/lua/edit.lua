-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- the edit mode logic

function enter_edit_mode()
	edit_mode = true
	edit_button.left_click = exit_edit_mode

	save_button = button{x = edit_button.x, y = edit_button.y+ 64, text="Save"}
	save_button.left_click = function(wg) board_export("save/save.lua") end
end

function exit_edit_mode()
	edit_mode = nil
	save_button.pop()
	save_button = nil
	edit_button.left_click = enter_edit_mode
end
