/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2020 Karel Zak <kzak@redhat.com>
 */
#include "lscpu.h"

struct lscpu_cpu *lscpu_new_cpu(int id)
{
	struct lscpu_cpu *cpu;

	cpu = xcalloc(1, sizeof(struct lscpu_cpu));
	cpu->refcount = 1;
	cpu->logical_id = id;
	cpu->coreid = -1;
	cpu->socketid = -1;
	cpu->bookid = -1;
	cpu->drawerid = -1;
	cpu->address = -1;
	cpu->configured = -1;

	DBG(CPU, ul_debugobj(cpu, "alloc"));
	return cpu;
}

void lscpu_ref_cpu(struct lscpu_cpu *cpu)
{
	if (cpu)
		cpu->refcount++;
}

void lscpu_unref_cpu(struct lscpu_cpu *cpu)
{
	if (!cpu)
		return;

	if (--cpu->refcount <= 0) {
		DBG(CPU, ul_debugobj(cpu, "  freeing #%d", cpu->logical_id));
		lscpu_unref_cputype(cpu->type);
		cpu->type = NULL;
		free(cpu->dynamic_mhz);
		free(cpu->static_mhz);
		free(cpu->mhz);
		free(cpu->bogomips);
		free(cpu);
	}
}

/*
 * Create and initialize array with CPU structs according to @cpuset.
 */
int lscpu_create_cpus(struct lscpu_cxt *cxt, cpu_set_t *cpuset, size_t setsize)
{
	size_t n, i;

	assert(!cxt->cpus);

	cxt->npossibles = CPU_COUNT_S(setsize, cpuset);
	cxt->cpus = xcalloc(1, cxt->npossibles * sizeof(struct lscpu_cpu *));

	for (n = 0, i = 0; n < (size_t) cxt->maxcpus && i < cxt->npossibles; n++) {
		if (CPU_ISSET_S(n, setsize, cpuset))
			cxt->cpus[i++] = lscpu_new_cpu(n);
	}

	return 0;
}

int lscpu_cpu_set_type(struct lscpu_cpu *cpu, struct lscpu_cputype *type)
{
	if (cpu->type == type)
		return 0;

	lscpu_unref_cputype(cpu->type);
	cpu->type = type;
	lscpu_ref_cputype(type);

	DBG(CPU, ul_debugobj(cpu, "cputype set to %s", type ? type->vendor : NULL));
	return 0;
}

/* don't forget lscpu_ref_cpu() ! */
struct lscpu_cpu *lscpu_get_cpu(struct lscpu_cxt *cxt, int logical_id)
{
	size_t i;

	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (cpu && cpu->logical_id == logical_id)
			return cpu;
	}

	return NULL;
}
