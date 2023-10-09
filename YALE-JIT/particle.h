// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once

struct particle_bin* particle_bin_new(size_t);
void particle_bin_del(struct particle_bin*);

void particle_bin_append(struct particle_bin*, void (*)(void*, double), void (*)(void*), void*, double);
void particle_bin_callback(struct particle_bin*);