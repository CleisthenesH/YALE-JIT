// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

//#include "renderer_interface.h"
#include "material.h"

// To handle the varity of effect and selection data I've implemented a very basic type system.
// Instead of having a bunch of empty fields I directly manipulate memorry to make all the data next to eachother.
// The hope is to locality will improve caching, not profiled to see if it improves proformace.

enum MATERIAL_COMPONENTS
{
	MATERIAL_COMPONENTS_SELECTION_COLOR = 1,
	MATERIAL_COMPONENTS_SELECTION_CUTOFF = 2,
	MATERIAL_COMPONENTS_SELECTION_POINT = 4,

	MATERIAL_COMPONENTS_EFFECT_POINT = 8,
	MATERIAL_COMPONENTS_EFFECT_COLOR = 16,

	MATERIAL_COMPONENTS_CNT = 5,
};

static enum MATERIAL_COMPONENTS effect_compents[] =
{
	0,
	MATERIAL_COMPONENTS_EFFECT_COLOR,
	MATERIAL_COMPONENTS_EFFECT_POINT
};

static enum MATERIAL_COMPONENTS selector_compents[] =
{
	0,
	MATERIAL_COMPONENTS_SELECTION_CUTOFF | MATERIAL_COMPONENTS_SELECTION_COLOR
};

// ALLEGRO shaders only support floats, will upgrade to doubles if avalible
static size_t material_component_size[MATERIAL_COMPONENTS_CNT] =
{
	sizeof(float) * 3,
	sizeof(float),
	sizeof(float) * 2,
	sizeof(float) * 2,
	sizeof(float) * 3,
};

struct material
{
	enum EFFECT_ID effect_id;
	enum SELECTION_ID selection_id;
	enum MATERIAL_COMPONENTS components;
};

static inline size_t component_size(enum MATERIAL_COMPONENTS components)
{
	size_t size = 0;
	enum MATERIAL_COMPONENTS mask = 1;

	for (size_t idx = 0; idx < MATERIAL_COMPONENTS_CNT; idx++, mask <<= 1)
		if (mask & components)
			size += material_component_size[idx];

	return size;
}

struct material* material_new(
	enum EFFECT_ID effect_id,
	enum SELECTION_ID selection_id)
{
	const enum MATERIAL_COMPONENTS components = selector_compents[selection_id] | effect_compents[effect_id];
	const size_t component_block = component_size(components);

	struct material* const output = malloc(sizeof(struct material) + component_block);

	if (!output)
		return NULL;

	*output = (struct material)
	{
		.effect_id = effect_id,
		.selection_id = selection_id,
		.components = components,
	};

	return output;
}

void material_apply(const struct material* const material)
{
	if (!material)
	{
		al_set_shader_int("effect_id", MATERIAL_ID_NULL);
		al_set_shader_int("selection_id", SELECTION_ID_FULL);
		return;
	}

	al_set_shader_int("effect_id", material->effect_id);
	al_set_shader_int("selection_id", material->selection_id);

	al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);

	const char* ptr = (const char*)material;
	ptr += sizeof(struct material);
	float* cast;

	enum MATERIAL_COMPONENTS mask = 1;

	// TODO: obvious refactor
	for (size_t idx = 0; idx < MATERIAL_COMPONENTS_CNT; idx++, mask <<= 1)
		if (mask & material->components)
		{
			cast = (float*)ptr;

			switch (mask)
			{
			case MATERIAL_COMPONENTS_SELECTION_COLOR:
				al_set_shader_float_vector("selection_color", 3, (float*)ptr, 1);
				break;

			case MATERIAL_COMPONENTS_EFFECT_COLOR:
				al_set_shader_float_vector("effect_color", 3, (float*)ptr, 1);
				break;

			case MATERIAL_COMPONENTS_SELECTION_CUTOFF:
				al_set_shader_float("selection_cutoff", *(float*)ptr);
				break;

			case MATERIAL_COMPONENTS_SELECTION_POINT:
				al_set_shader_float_vector("selection_point", 2, (float*)ptr, 1);
				break;

			case MATERIAL_COMPONENTS_EFFECT_POINT:
				al_set_shader_float_vector("effect_point", 2, (float*)ptr, 1);
				break;
			}

			ptr += material_component_size[idx];
		}
}

// Maybe turninto a standalone itter since similar code is used in three spots.
static inline char* get_component(struct material* const material, enum MATERIAL_COMPONENTS component)
{
	if (!(material->components & component))
		return NULL;

	char* ptr = (char*)material;
	ptr += sizeof(struct material);

	enum MATERIAL_COMPONENTS mask = 1;
	for (size_t idx = 0; idx < MATERIAL_COMPONENTS_CNT; idx++, mask <<= 1)
		if (mask & material->components)
			if (component == mask)
				break;
			else
				ptr += material_component_size[idx];

	return ptr;
}

void material_selection_color(struct material* const material, ALLEGRO_COLOR color)
{
	float* const data = (float*)get_component(material, MATERIAL_COMPONENTS_SELECTION_COLOR);

	if (!data)
		return;

	data[0] = color.r;
	data[1] = color.g;
	data[2] = color.b;
}

void material_selection_cutoff(struct material* const material, double cutoff)
{
	float* const data = (float*)get_component(material, MATERIAL_COMPONENTS_SELECTION_CUTOFF);

	if (!data)
		return;

	*data = cutoff;
}

void material_point(struct material* const material, double x, double y)
{
	float* const data = (float*)get_component(material, MATERIAL_COMPONENTS_EFFECT_POINT);

	if (!data)
		return;

	data[0] = x;
	data[1] = y;
}
