// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once
#include <allegro5/allegro.h>

// x-position, y-position, x-scale, y-scale, angle, camera, x-offset, y-offset
struct keyframe
{
    double x, y, sx, sy, a, c, dx, dy;
};

// Keyframe methods
void keyframe_default(struct keyframe* const);
void keyframe_build_transform(const struct keyframe* const, ALLEGRO_TRANSFORM* const);
void keyframe_copy(struct keyframe* const, const struct keyframe* const);
void keyframe_blend(struct keyframe* const, const struct keyframe* const, const struct keyframe* const,double);

void lua_getkeyframe(int, struct keyframe* const);
void lua_setkeyframe(int, const struct keyframe* const);
void lua_cleankeyframe(int);