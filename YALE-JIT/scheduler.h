// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once

struct scheduler_interface* scheduler_push(double, void(*)(void*), void*);
void scheduler_pop(struct scheduler_interface*);
void scheduler_change_timestamp(struct scheduler_interface*, double, int);