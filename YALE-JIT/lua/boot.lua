-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

print(current_time())

local function test_funct(wg)
	print(wg,wg.a.test)
end

test_button = button({y=400, x=500, a = 0, c=1,hover_start = test_funct,test="hello"})
test_button_2 = button({y=600, x=500, a = 0,hover_start = test_funct,test="hello"})

print("Boot Complete")
