-- Copyright 2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Running this file transitions to the Material test screen

square = material_test{x=display_width*0.5,y=500,effect=0,selection=0}
effect_drop_down = drop_down{x=display_width/4,y=100,options={"None","Plain Foil","Radial RGB","Magma","Testing"}}
selection_drop_down = drop_down{x=display_width*2/4,y=100,options={"All","Color Band","Testing"}}
bitmap_button = button{x=display_width*3/4,y=100,text="Set Bitmap"}

function effect_drop_down:left_click()
	square.effect = self.option_id
end

function selection_drop_down:left_click()
	square.selection = self.option_id 
end

function bitmap_button:left_click()
	local file_path = native_file_dialog{patterns="*.*;*.bmp;*.png","ALLEGRO_FILECHOOSER_PICTURES","file_must_exist"}

	if file_path then
		square.bitmap = file_path
	end
end