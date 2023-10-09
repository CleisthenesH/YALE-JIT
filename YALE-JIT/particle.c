// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include "particle.h"
#include "thread_pool.h"
#include "scheduler.h"
#include <stdlib.h>

extern double current_timestamp;

struct particle
{
	struct particle_bin* bin;
	void (*callback)(void*, double);
	void (*gc)(void*);
	void* data;
	double start_timestamp;
};

struct particle_bin
{
	struct particle* particles;
	size_t particles_allocated;
	size_t particles_used;
};

static void particle_gc(struct particle* particle)
{
	struct particle_bin* const bin = particle->bin;

	for (size_t i = 0; i < bin->particles_used; i++)
		if (particle == bin->particles + i)
		{
			if(particle->gc)
				particle->gc(particle->data);

			bin->particles[i--] = bin->particles[--bin->particles_used];

			break;
		}
}

struct particle_bin* particle_bin_new(size_t inital_size)
{

	struct particle_bin* bin = malloc(sizeof(struct particle_bin));

	if (!bin)
	{
		return NULL;
	}

	*bin = (struct particle_bin)
	{
		.particles = malloc(inital_size * sizeof(struct particle)),
		.particles_allocated = inital_size,
		.particles_used = 0
	};

	return bin;
}

void particle_bin_del(struct particle_bin* bin)
{
	for (size_t i = 0; i < bin->particles_used; i++)
		if (bin->particles[i].gc)
			bin->particles[i].gc(bin->particles[i].data);

	free(bin->particles);
	free(bin);
}

void particle_bin_append(struct particle_bin* bin, void (*callback)(void*, double), void (*gc)(void*), void* data, double lifespan)
{
	if (bin->particles_allocated <= bin->particles_used)
	{
		const size_t new_cnt = 2 * bin->particles_allocated + 1;

		struct particle* memsafe_hande = realloc(bin->particles, new_cnt * sizeof(struct particle));

		if (!memsafe_hande)
			return;

		bin->particles = memsafe_hande;
		bin->particles_allocated = new_cnt;
	}

	struct particle* const particle = bin->particles + bin->particles_used++;

	*particle = (struct particle)
	{
		.bin = bin,
		.data = data,
		.callback = callback,
		.gc = gc,
		.start_timestamp = current_timestamp
	};

	scheduler_push(lifespan, particle_gc, particle);
}

void particle_bin_callback(struct particle_bin* bin)
{
	for (size_t i = 0; i < bin->particles_used; i++)
	{
		const struct particle* const particle = bin->particles + i;
		particle->callback(particle->data, current_timestamp - particle->start_timestamp);
	}
}
