-- Copyright 2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Running this file transitions to the Material test screen

button = button{x=display_width*0.5,y=100, left_click = function(wg) square.effect = 1 end}

square = material_test{x=display_width*0.5,y=500,effect=2,selection=0}