-- Copyright 2023 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs once after all inializations have ran but before the main loop.

function funct(wg)
	print("test_button",wg.id,wg)
end

test_button1 = button{y=400, x=500, id = 1, text = "Button 1", c=1}
test_button2 = button{y=600, x=500, id = 2, left_click = funct}

tile1 = tile{y=200, x=100,  team="red", tile="camp"}
tile2 = tile{y=200, x=200, team="red", tile="hills"}
tile3 = tile{y=200, x=300, team="red", tile="town"}

meeple = meeple{x=100,y=100}

function test_button1:left_click()
	print("test_button",self.id,self)
	self:set_keyframe{y=400, x=500}
	self:push_keyframe{y=400, x=900, t = 1}
end

text_entry1 = text_entry{y = 900, x = 500}

print("Boot Complete")

function moves(piece,zone)
	print(piece,zone)
	return {tile1,tile3}
end
