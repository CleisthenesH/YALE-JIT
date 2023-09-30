-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

function funct(wg)
	print("test_button",wg.id,wg)
end

test_button1 = button{y=400, x=500, id = 1, text = "button 1"}
test_button2 = button{y=600, x=500, id = 2, left_click = funct}

function test_button1:left_click()
	print("test_button",self.id,self)
end

print("Boot Complete")
