-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- For testing some of the GUI widgets.

print("HUD_test")

test_button = button			{x=600, y= 50, text="TEST TEXT"}
test_text_entry = text_entry	{x=600, y=150, text="TEST TEXT"}
test_counter = counter			{x=600, y=250, icon=2206, value = 234}

function test_button:left_click()
	test_counter:add(1)
end

