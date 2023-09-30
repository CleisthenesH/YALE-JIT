// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// An obficated struct for a rendering material.
//	The obfiscation is because the internal variables of a material can change wildly but the shaders requirements are exact.
//	So I want to mantain the material's state where the user can't mess with it.
//	Might add and update element method to them at later date, might not.

#include <allegro5/allegro_color.h>

enum MATERIAL_ID
{
	MATERIAL_ID_NULL,
	MATERIAL_ID_PLAIN_FOIL,
	MATERIAL_ID_RADIAL_RGB,
	MATERIAL_ID_VORONOI,

	MATERIAL_ID_TEST,
	/*
		MATERIAL_ID_SHEEN,
	*/
	MATERIAL_ID_MAX
};

enum SELECTION_ID
{
	SELECTION_ID_FULL,
	SELECTION_ID_COLOR_BAND,

	SELECTION_ID_MAX
};

struct material* material_new(enum MATERIAL_ID, enum SELECTION_ID);

void material_point(struct material* const, double, double);
void material_color(struct material* const, ALLEGRO_COLOR);
void material_cutoff(struct material* const, double);

void material_selection_color(struct material* const, ALLEGRO_COLOR);
void material_selection_cutoff(struct material* const, double);

void material_apply(const struct material* const);