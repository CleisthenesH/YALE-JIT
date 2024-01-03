-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- For testing some of the GUI widgets.

print("HUD_test")

test_button = button			{x=display_width*0.5, y= 64, text="TEST TEXT"}
test_text_entry = text_entry	{x=display_width*0.5, y=128, text="TEST TEXT"}
test_counter = counter			{x=display_width*0.5, y=220, icon=2206, value = 234}
test_slider = slider			{x=display_width*0.5, y=296, progress = 0.5}

function test_button:left_click()
	test_counter:set(test_slider.value)
end

