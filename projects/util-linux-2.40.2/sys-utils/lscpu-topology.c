/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2008 Cai Qian <qcai@redhat.com>
 * Copyright (C) 2008-2023 Karel Zak <kzak@redhat.com>
 */
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "lscpu.h"

/* add @set to the @ary, unnecessary set is deallocated. */
static int add_cpuset_to_array(cpu_set_t **ary, size_t *items, cpu_set_t *set, size_t setsize)
{
	size_t i;

	if (!ary)
		return -EINVAL;

	for (i = 0; i < *items; i++) {
		if (CPU_EQUAL_S(setsize, set, ary[i]))
			break;
	}
	if (i == *items) {
		ary[*items] = set;
		++*items;
		return 0;
	}
	CPU_FREE(set);
	return 1;
}

static void free_cpuset_array(cpu_set_t **ary, int items)
{
	int i;

	if (!ary)
		return;
	for (i = 0; i < items; i++)
		free(ary[i]);
	free(ary);
}

void lscpu_cputype_free_topology(struct lscpu_cputype *ct)
{
	if (!ct)
		return;
	free_cpuset_array(ct->coremaps, ct->ncores);
	free_cpuset_array(ct->socketmaps, ct->nsockets);
	free_cpuset_array(ct->bookmaps, ct->nbooks);
	free_cpuset_array(ct->drawermaps, ct->ndrawers);
}

void lscpu_free_caches(struct lscpu_cache *caches, size_t n)
{
	size_t i;

	if (!caches)
		return;

	for (i = 0; i < n; i++) {
		struct lscpu_cache *c = &caches[i];

		DBG(MISC, ul_debug(" freeing cache #%zu %s::%d",
					i, c->name, c->id));

		free(c->name);
		free(c->type);
		free(c->allocation_policy);
		free(c->write_policy);
		free(c->sharedmap);
	}
	free(caches);
}

static int cmp_cache(const void *a0, const void *b0)
{
	const struct lscpu_cache
		*a = (const struct lscpu_cache *) a0,
		*b = (const struct lscpu_cache *) b0;
	return strcmp(a->name, b->name);
}

void lscpu_sort_caches(struct lscpu_cache *caches, size_t n)
{
	if (caches && n)
		qsort(caches, n, sizeof(struct lscpu_cache), cmp_cache);
}


/* Read topology for specified type */
static int cputype_read_topology(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	size_t i, npos;
	struct path_cxt *sys;
	int nthreads = 0, sw_topo = 0;
	FILE *fd;

	sys = cxt->syscpu;				/* /sys/devices/system/cpu/ */
	npos = cxt->npossibles;				/* possible CPUs */

	DBG(TYPE, ul_debugobj(ct, "reading %s/%s/%s topology",
				ct->vendor ?: "", ct->model ?: "", ct->modelname ?:""));

	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];
		cpu_set_t *thread_siblings = NULL, *core_siblings = NULL;
		cpu_set_t *book_siblings = NULL, *drawer_siblings = NULL;
		int num, n = 0;

		if (!cpu || cpu->type != ct)
			continue;

		num = cpu->logical_id;
		if (ul_path_accessf(sys, F_OK,
					"cpu%d/topology/thread_siblings", num) != 0)
			continue;

		/* read topology maps */
		ul_path_readf_cpuset(sys, &thread_siblings, cxt->maxcpus,
					"cpu%d/topology/thread_siblings", num);
		ul_path_readf_cpuset(sys, &core_siblings, cxt->maxcpus,
					"cpu%d/topology/core_siblings", num);
		ul_path_readf_cpuset(sys, &book_siblings, cxt->maxcpus,
					"cpu%d/topology/book_siblings", num);
		ul_path_readf_cpuset(sys, &drawer_siblings, cxt->maxcpus,
					"cpu%d/topology/drawer_siblings", num);

		if (thread_siblings)
			n = CPU_COUNT_S(cxt->setsize, thread_siblings);
		if (!n)
			n = 1;
		if (n > nthreads)
			nthreads = n;

		/* Allocate arrays for topology maps.
		 *
		 * For each map we make sure that it can have up to ncpuspos
		 * entries. This is because we cannot reliably calculate the
		 * number of cores, sockets and books on all architectures.
		 * E.g. completely virtualized architectures like s390 may
		 * have multiple sockets of different sizes.
		 */
		if (!ct->coremaps && thread_siblings)
			ct->coremaps = xcalloc(npos, sizeof(cpu_set_t *));
		if (!ct->socketmaps && core_siblings)
			ct->socketmaps = xcalloc(npos, sizeof(cpu_set_t *));
		if (!ct->bookmaps && book_siblings)
			ct->bookmaps = xcalloc(npos, sizeof(cpu_set_t *));
		if (!ct->drawermaps && drawer_siblings)
			ct->drawermaps = xcalloc(npos, sizeof(cpu_set_t *));

		/* add to topology maps */
		if (thread_siblings)
			add_cpuset_to_array(ct->coremaps, &ct->ncores, thread_siblings, cxt->setsize);
		if (core_siblings)
			add_cpuset_to_array(ct->socketmaps, &ct->nsockets, core_siblings, cxt->setsize);
		if (book_siblings)
			add_cpuset_to_array(ct->bookmaps, &ct->nbooks, book_siblings, cxt->setsize);
		if (drawer_siblings)
			add_cpuset_to_array(ct->drawermaps, &ct->ndrawers, drawer_siblings, cxt->setsize);

	}

	/* s390 detects its cpu topology via /proc/sysinfo, if present.
	 * Using simply the cpu topology masks in sysfs will not give
	 * usable results since everything is virtualized. E.g.
	 * virtual core 0 may have only 1 cpu, but virtual core 2 may
	 * five cpus.
	 * If the cpu topology is not exported (e.g. 2nd level guest)
	 * fall back to old calculation scheme.
	 */
	if ((fd = ul_path_fopen(cxt->procfs, "r", "sysinfo"))) {
		int t0, t1;
		char buf[BUFSIZ];

		DBG(TYPE, ul_debugobj(ct, " reading sysinfo"));

		while (fgets(buf, sizeof(buf), fd) != NULL) {
			if (sscanf(buf, "CPU Topology SW: %d %d %zu %zu %zu %zu",
					&t0, &t1,
					&ct->ndrawers_per_system,
					&ct->nbooks_per_drawer,
					&ct->nsockets_per_book,
					&ct->ncores_per_socket) == 6) {
				sw_topo = 1;
				DBG(TYPE, ul_debugobj(ct, " using SW topology"));
				break;
			}
		}
		if (fd)
			fclose(fd);
	}

	ct->nthreads_per_core = nthreads;
	if (ct->mtid) {
		uint64_t x;
		if (ul_strtou64(ct->mtid, &x, 10) == 0 && x <= ULONG_MAX)
			ct->nthreads_per_core = (size_t) x + 1;
	}

	if (!sw_topo) {
		ct->ncores_per_socket = ct->nsockets ? ct->ncores / ct->nsockets : 0;
		ct->nsockets_per_book = ct->nbooks   ? ct->nsockets / ct->nbooks : 0;
		ct->nbooks_per_drawer = ct->ndrawers ? ct->nbooks / ct->ndrawers : 0;
		ct->ndrawers_per_system = ct->ndrawers;
	}

	DBG(TYPE, ul_debugobj(ct, " nthreads: %zu (per core)", ct->nthreads_per_core));
	DBG(TYPE, ul_debugobj(ct, "   ncores: %zu (%zu per socket)", ct->ncores, ct->ncores_per_socket));
	DBG(TYPE, ul_debugobj(ct, " nsockets: %zu (%zu per books)", ct->nsockets, ct->nsockets_per_book));
	DBG(TYPE, ul_debugobj(ct, "   nbooks: %zu (%zu per drawer)", ct->nbooks, ct->nbooks_per_drawer));
	DBG(TYPE, ul_debugobj(ct, " ndrawers: %zu (%zu per system)", ct->ndrawers, ct->ndrawers_per_system));

	return 0;
}

/* count size of all instancess of the "name" */
size_t lscpu_get_cache_full_size(struct lscpu_cxt *cxt, const char *name, int *instances)
{
	size_t i, sz = 0;

	if (instances)
		*instances = 0;

	for (i = 0; i < cxt->ncaches; i++) {
		if (strcmp(cxt->caches[i].name, name) == 0) {
			sz += cxt->caches[i].size;
			if (instances)
				(*instances)++;
		}
	}

	return sz;
}

struct lscpu_cache *lscpu_cpu_get_cache(struct lscpu_cxt *cxt,
				struct lscpu_cpu *cpu, const char *name)
{
	size_t i;

	for (i = 0; i < cxt->ncaches; i++) {
		struct lscpu_cache *ca = &cxt->caches[i];

		if (ca->sharedmap &&
		    strcmp(ca->name, name) == 0 &&
		    CPU_ISSET_S(cpu->logical_id, cxt->setsize, ca->sharedmap))
			return ca;
	}

	return NULL;
}

/*
 * The cache is identifued by type+level+id.
 */
static struct lscpu_cache *get_cache(struct lscpu_cxt *cxt,
				const char *type, int level, int id)
{
	size_t i;

	for (i = 0; i < cxt->ncaches; i++) {
		struct lscpu_cache *ca = &cxt->caches[i];
		if (ca->id == id &&
		    ca->level == level &&
		    strcmp(ca->type, type) == 0)
			return ca;
	}
	return NULL;
}

static struct lscpu_cache *add_cache(struct lscpu_cxt *cxt,
				const char *type, int level, int id)
{
	struct lscpu_cache *ca;

	cxt->ncaches++;
	cxt->caches = xreallocarray(cxt->caches,
				    cxt->ncaches, sizeof(*cxt->caches));

	ca = &cxt->caches[cxt->ncaches - 1];
	memset(ca, 0 , sizeof(*ca));

	ca->id = id;
	ca->level = level;
	ca->type = xstrdup(type);

	DBG(GATHER, ul_debugobj(cxt, "add cache %s%d::%d", type, level, id));
	return ca;
}

static int mk_cache_id(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu, char *type, int level)
{
	size_t i;
	int idx = 0;

	for (i = 0; i < cxt->ncaches; i++) {
		struct lscpu_cache *ca = &cxt->caches[i];

		if (ca->level != level || strcmp(ca->type, type) != 0)
			continue;

		if (ca->sharedmap &&
		    CPU_ISSET_S(cpu->logical_id, cxt->setsize, ca->sharedmap))
			return idx;
		idx++;
	}

	return idx;
}

static int read_sparc_onecache(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu,
			   int level, char *typestr, int type)
{
	struct lscpu_cache *ca;
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;
	uint32_t size;
	int rc, id;
	char buf[32];

	if (type)
		snprintf(buf, sizeof(buf), "l%d_%c", level, type);
	else
		snprintf(buf, sizeof(buf), "l%d_", level);

	rc = ul_path_readf_u32(sys, &size,
			"cpu%d/%scache_size", num, buf);
	if (rc != 0)
		return rc;

	DBG(CPU, ul_debugobj(cpu, "#%d reading sparc %s cache", num, buf));

	id = mk_cache_id(cxt, cpu, typestr, level);

	ca = get_cache(cxt, typestr, level, id);
	if (!ca)
		ca = add_cache(cxt, typestr, level, id);

	if (!ca->name) {
		ul_path_readf_u32(sys, &ca->coherency_line_size,
					"cpu%d/%scache_line_size", num, buf);
		assert(ca->type);

		if (type)
			snprintf(buf, sizeof(buf), "L%d%c", ca->level, type);
		else
			snprintf(buf, sizeof(buf), "L%d", ca->level);
		ca->name = xstrdup(buf);
		ca->size = size;
	}
	/* There is no sharedmap of the cache in /sys, we assume that caches are
	 * not shared. Send a patch if your /sys provides another information.
	 */
	if (!ca->sharedmap) {
		size_t setsize = 0;

		ca->sharedmap = cpuset_alloc(cxt->maxcpus, &setsize, NULL);
		CPU_ZERO_S(setsize, ca->sharedmap);
		CPU_SET_S(num, setsize, ca->sharedmap);
	}

	return 0;
}

static int read_sparc_caches(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	read_sparc_onecache(cxt, cpu, 1, "Instruction", 'i');
	read_sparc_onecache(cxt, cpu, 1, "Data", 'd');
	read_sparc_onecache(cxt, cpu, 2, "Unified", 0);
	read_sparc_onecache(cxt, cpu, 2, "Unified", 0);

	return 0;
}

static int read_caches(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	char buf[256];
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;
	size_t i, ncaches = 0;

	while (ul_path_accessf(sys, F_OK,
				"cpu%d/cache/index%zu",
				num, ncaches) == 0)
		ncaches++;

	if (ncaches == 0 && ul_path_accessf(sys, F_OK,
				"cpu%d/l1_icache_size", num) == 0)
		return read_sparc_caches(cxt, cpu);

	DBG(CPU, ul_debugobj(cpu, "#%d reading %zd caches", num, ncaches));

	for (i = 0; i < ncaches; i++) {
		struct lscpu_cache *ca;
		int id, level;

		if (ul_path_readf_s32(sys, &id, "cpu%d/cache/index%zu/id", num, i) != 0)
			id = -1;
		if (ul_path_readf_s32(sys, &level, "cpu%d/cache/index%zu/level", num, i) != 0)
			continue;
		if (ul_path_readf_buffer(sys, buf, sizeof(buf),
                                        "cpu%d/cache/index%zu/type", num, i) <= 0)
			continue;

		if (id == -1)
			id = mk_cache_id(cxt, cpu, buf, level);

		ca = get_cache(cxt, buf, level, id);
		if (!ca)
			ca = add_cache(cxt, buf, level, id);

		if (!ca->name) {
			int type = 0;

			assert(ca->type);

			if (!strcmp(ca->type, "Data"))
				type = 'd';
			else if (!strcmp(ca->type, "Instruction"))
				type = 'i';

			if (type)
				snprintf(buf, sizeof(buf), "L%d%c", ca->level, type);
			else
				snprintf(buf, sizeof(buf), "L%d", ca->level);

			ca->name = xstrdup(buf);

			ul_path_readf_u32(sys, &ca->ways_of_associativity,
					"cpu%d/cache/index%zu/ways_of_associativity", num, i);
			ul_path_readf_u32(sys, &ca->physical_line_partition,
					"cpu%d/cache/index%zu/physical_line_partition", num, i);
			ul_path_readf_u32(sys, &ca->number_of_sets,
					"cpu%d/cache/index%zu/number_of_sets", num, i);
			ul_path_readf_u32(sys, &ca->coherency_line_size,
					"cpu%d/cache/index%zu/coherency_line_size", num, i);

			ul_path_readf_string(sys, &ca->allocation_policy,
					"cpu%d/cache/index%zu/allocation_policy", num, i);
			ul_path_readf_string(sys, &ca->write_policy,
					"cpu%d/cache/index%zu/write_policy", num, i);

			/* cache size */
			if (ul_path_readf_buffer(sys, buf, sizeof(buf),
					"cpu%d/cache/index%zu/size", num, i) > 0)
				parse_size(buf, &ca->size, NULL);
			else
				ca->size = 0;
		}

		if (!ca->sharedmap)
			/* information about how CPUs share different caches */
			ul_path_readf_cpuset(sys, &ca->sharedmap, cxt->maxcpus,
					  "cpu%d/cache/index%zu/shared_cpu_map", num, i);
	}

	return 0;
}

static int read_ids(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;

	if (ul_path_accessf(sys, F_OK, "cpu%d/topology", num) != 0)
		return 0;

	DBG(CPU, ul_debugobj(cpu, "#%d reading IDs", num));

	if (ul_path_readf_s32(sys, &cpu->coreid, "cpu%d/topology/core_id", num) != 0)
		cpu->coreid = -1;
	if (ul_path_readf_s32(sys, &cpu->socketid, "cpu%d/topology/physical_package_id", num) != 0)
		cpu->socketid = -1;
	if (ul_path_readf_s32(sys, &cpu->bookid, "cpu%d/topology/book_id", num) != 0)
		cpu->bookid = -1;
	if (ul_path_readf_s32(sys, &cpu->drawerid, "cpu%d/topology/drawer_id", num) != 0)
		cpu->drawerid = -1;

	return 0;
}

static int read_polarization(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;
	char mode[64];

	if (ul_path_accessf(sys, F_OK, "cpu%d/polarization", num) != 0)
		return 0;

	ul_path_readf_buffer(sys, mode, sizeof(mode), "cpu%d/polarization", num);

	DBG(CPU, ul_debugobj(cpu, "#%d reading polar=%s", num, mode));

	if (strncmp(mode, "vertical:low", sizeof(mode)) == 0)
		cpu->polarization = POLAR_VLOW;
	else if (strncmp(mode, "vertical:medium", sizeof(mode)) == 0)
		cpu->polarization = POLAR_VMEDIUM;
	else if (strncmp(mode, "vertical:high", sizeof(mode)) == 0)
		cpu->polarization = POLAR_VHIGH;
	else if (strncmp(mode, "horizontal", sizeof(mode)) == 0)
		cpu->polarization = POLAR_HORIZONTAL;
	else
		cpu->polarization = POLAR_UNKNOWN;

	if (cpu->type)
		cpu->type->has_polarization = 1;
	return 0;
}

static int read_address(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;

	if (ul_path_accessf(sys, F_OK, "cpu%d/address", num) != 0)
		return 0;

	DBG(CPU, ul_debugobj(cpu, "#%d reading address", num));

	ul_path_readf_s32(sys, &cpu->address, "cpu%d/address", num);
	if (cpu->type)
		cpu->type->has_addresses = 1;
	return 0;
}

static int read_configure(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;

	if (ul_path_accessf(sys, F_OK, "cpu%d/configure", num) != 0)
		return 0;

	DBG(CPU, ul_debugobj(cpu, "#%d reading configure", num));

	ul_path_readf_s32(sys, &cpu->configured, "cpu%d/configure", num);
	if (cpu->type)
		cpu->type->has_configured = 1;
	return 0;
}

static int read_mhz(struct lscpu_cxt *cxt, struct lscpu_cpu *cpu)
{
	struct path_cxt *sys = cxt->syscpu;
	int num = cpu->logical_id;
	int mhz;

	DBG(CPU, ul_debugobj(cpu, "#%d reading mhz", num));

	if (ul_path_readf_s32(sys, &mhz, "cpu%d/cpufreq/cpuinfo_max_freq", num) == 0)
		cpu->mhz_max_freq = (float) mhz / 1000;
	if (ul_path_readf_s32(sys, &mhz, "cpu%d/cpufreq/cpuinfo_min_freq", num) == 0)
		cpu->mhz_min_freq = (float) mhz / 1000;

	/* The default current-frequency value comes is from /proc/cpuinfo (if
	 * available).  This /proc value is usually based on MSR registers
	 * (APERF/APERF) and it changes pretty often. It seems better to read
	 * frequency from cpufreq subsystem that provides the current frequency
	 * for the current policy. There is also cpuinfo_cur_freq in sysfs, but
	 * it's not always available.
	 */
	if (ul_path_readf_s32(sys, &mhz, "cpu%d/cpufreq/scaling_cur_freq", num) == 0)
		cpu->mhz_cur_freq = (float) mhz / 1000;

	if (cpu->type && (cpu->mhz_min_freq || cpu->mhz_max_freq))
		cpu->type->has_freq = 1;

	return 0;
}

float lsblk_cputype_get_maxmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	size_t i;
	float res = 0.0;

	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (!cpu || cpu->type != ct || !is_cpu_present(cxt, cpu))
			continue;
		res = max(res, cpu->mhz_max_freq);
	}
	return res;
}

float lsblk_cputype_get_minmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	size_t i;
	float res = -1.0;

	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (!cpu || cpu->type != ct || !is_cpu_present(cxt, cpu))
			continue;
		if (res < 0.0 || cpu->mhz_min_freq < res)
			res = cpu->mhz_min_freq;
	}
	return res;
}

/* returns scaling (use) of CPUs freq. in percent */
float lsblk_cputype_get_scalmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	size_t i;
	float fmax = 0, fcur = 0;

	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (!cpu || cpu->type != ct || !is_cpu_present(cxt, cpu))
			continue;
		if (cpu->mhz_max_freq <= 0.0 || cpu->mhz_cur_freq <= 0.0)
			continue;
		fmax += cpu->mhz_max_freq;
		fcur += cpu->mhz_cur_freq;
	}
	if (fcur <= 0.0)
		return 0.0;
	return fcur / fmax * 100;
}

int lscpu_read_topology(struct lscpu_cxt *cxt)
{
	size_t i;
	int rc = 0;


	for (i = 0; i < cxt->ncputypes; i++)
		rc += cputype_read_topology(cxt, cxt->cputypes[i]);

	for (i = 0; rc == 0 && i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (!cpu || !cpu->type)
			continue;

		DBG(CPU, ul_debugobj(cpu, "#%d reading topology", cpu->logical_id));

		rc = read_ids(cxt, cpu);
		if (!rc)
			rc = read_polarization(cxt, cpu);
		if (!rc)
			rc = read_address(cxt, cpu);
		if (!rc)
			rc = read_configure(cxt, cpu);
		if (!rc)
			rc = read_mhz(cxt, cpu);
		if (!rc)
			rc = read_caches(cxt, cpu);
	}

	lscpu_sort_caches(cxt->caches, cxt->ncaches);
	DBG(GATHER, ul_debugobj(cxt, " L1d: %zu", lscpu_get_cache_full_size(cxt, "L1d", NULL)));
	DBG(GATHER, ul_debugobj(cxt, " L1i: %zu", lscpu_get_cache_full_size(cxt, "L1i", NULL)));
	DBG(GATHER, ul_debugobj(cxt, " L2: %zu", lscpu_get_cache_full_size(cxt, "L2", NULL)));
	DBG(GATHER, ul_debugobj(cxt, " L3: %zu", lscpu_get_cache_full_size(cxt, "L3", NULL)));

	return rc;
}


