-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- the edit mode logic

function exit_edit_mode()
	widgets.remove(save_button)
	save_button = nil
end

save_button = button{x = display_width - 64, y = display_height - 32, text="Save"}
save_button.left_click = function(wg) return board_export("save/save.lua") end
