// Copyright 2022 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// Modifyed from the default allegro shader source

attribute vec4 al_pos;
attribute vec4 al_color;
attribute vec2 al_texcoord;

uniform vec3 picker_color;

uniform mat4 al_projview_matrix;
uniform bool al_use_tex_matrix;
uniform mat4 al_tex_matrix;
uniform float displacement;

varying vec4 varying_color;
varying vec2 varying_texcoord;

uniform float saturate;

void main()
{
	varying_color = vec4(picker_color,1);

	if (al_use_tex_matrix) {
		vec4 uv = al_tex_matrix * vec4(al_texcoord, 0, 1);
		varying_texcoord = vec2(uv.x, uv.y);
	}
	else
		varying_texcoord = al_texcoord;
	
	gl_Position = al_projview_matrix * al_pos;
}