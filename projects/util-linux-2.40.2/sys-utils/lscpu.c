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
 *
 * lscpu - CPU architecture information helper
 */
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/personality.h>

#include <libsmartcols.h>

#include "closestream.h"
#include "optutils.h"
#include "c_strtod.h"
#include "sysfs.h"

#include "lscpu.h"

static const char *virt_types[] = {
	[VIRT_TYPE_NONE]	= N_("none"),
	[VIRT_TYPE_PARA]	= N_("para"),
	[VIRT_TYPE_FULL]	= N_("full"),
	[VIRT_TYPE_CONTAINER]	= N_("container"),
};

static const char *hv_vendors[] = {
	[VIRT_VENDOR_NONE]	= NULL,
	[VIRT_VENDOR_XEN]	= "Xen",
	[VIRT_VENDOR_KVM]	= "KVM",
	[VIRT_VENDOR_MSHV]	= "Microsoft",
	[VIRT_VENDOR_VMWARE]	= "VMware",
	[VIRT_VENDOR_IBM]	= "IBM",
	[VIRT_VENDOR_VSERVER]	= "Linux-VServer",
	[VIRT_VENDOR_UML]	= "User-mode Linux",
	[VIRT_VENDOR_INNOTEK]	= "Innotek GmbH",
	[VIRT_VENDOR_HITACHI]	= "Hitachi",
	[VIRT_VENDOR_PARALLELS]	= "Parallels",
	[VIRT_VENDOR_VBOX]	= "Oracle",
	[VIRT_VENDOR_OS400]	= "OS/400",
	[VIRT_VENDOR_PHYP]	= "pHyp",
	[VIRT_VENDOR_SPAR]	= "Unisys s-Par",
	[VIRT_VENDOR_WSL]	= "Windows Subsystem for Linux"
};

/* dispatching modes */
static const char *disp_modes[] = {
	[DISP_HORIZONTAL]	= N_("horizontal"),
	[DISP_VERTICAL]		= N_("vertical")
};

struct polarization_modes {
	char *parsable;
	char *readable;
};

static struct polarization_modes polar_modes[] = {
	[POLAR_UNKNOWN]    = {"U",  "-"},
	[POLAR_VLOW]       = {"VL", "vert-low"},
	[POLAR_VMEDIUM]    = {"VM", "vert-medium"},
	[POLAR_VHIGH]      = {"VH", "vert-high"},
	[POLAR_HORIZONTAL] = {"H",  "horizontal"},
};

/*
 * IDs
 */
enum {
	COL_CPU_BOGOMIPS,
	COL_CPU_CPU,
	COL_CPU_CORE,
	COL_CPU_SOCKET,
	COL_CPU_CLUSTER,
	COL_CPU_NODE,
	COL_CPU_BOOK,
	COL_CPU_DRAWER,
	COL_CPU_CACHE,
	COL_CPU_POLARIZATION,
	COL_CPU_ADDRESS,
	COL_CPU_CONFIGURED,
	COL_CPU_ONLINE,
	COL_CPU_MHZ,
	COL_CPU_SCALMHZ,
	COL_CPU_MAXMHZ,
	COL_CPU_MINMHZ,
	COL_CPU_MODELNAME,
};

enum {
	COL_CACHE_ALLSIZE,
	COL_CACHE_LEVEL,
	COL_CACHE_NAME,
	COL_CACHE_ONESIZE,
	COL_CACHE_TYPE,
	COL_CACHE_WAYS,
	COL_CACHE_ALLOCPOL,
	COL_CACHE_WRITEPOL,
	COL_CACHE_PHYLINE,
	COL_CACHE_SETS,
	COL_CACHE_COHERENCYSIZE
};


/* column description
 */
struct lscpu_coldesc {
	const char *name;
	const char *help;

	int flags;
	unsigned int  is_abbr:1;	/* name is abbreviation */
	int json_type;
};

static struct lscpu_coldesc coldescs_cpu[] =
{
	[COL_CPU_BOGOMIPS]     = { "BOGOMIPS", N_("crude measurement of CPU speed"), SCOLS_FL_RIGHT, 1, SCOLS_JSON_NUMBER },
	[COL_CPU_CPU]          = { "CPU", N_("logical CPU number"), SCOLS_FL_RIGHT, 1, SCOLS_JSON_NUMBER },
	[COL_CPU_CORE]         = { "CORE", N_("logical core number"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_CLUSTER]      = { "CLUSTER", N_("logical cluster number"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_SOCKET]       = { "SOCKET", N_("logical socket number"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_NODE]         = { "NODE", N_("logical NUMA node number"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_BOOK]         = { "BOOK", N_("logical book number"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_DRAWER]       = { "DRAWER", N_("logical drawer number"), SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER },
	[COL_CPU_CACHE]        = { "CACHE", N_("shows how caches are shared between CPUs") },
	[COL_CPU_POLARIZATION] = { "POLARIZATION", N_("CPU dispatching mode on virtual hardware") },
	[COL_CPU_ADDRESS]      = { "ADDRESS", N_("physical address of a CPU") },
	[COL_CPU_CONFIGURED]   = { "CONFIGURED", N_("shows if the hypervisor has allocated the CPU"), 0, 0, SCOLS_JSON_BOOLEAN_OPTIONAL },
	[COL_CPU_ONLINE]       = { "ONLINE", N_("shows if Linux currently makes use of the CPU"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_BOOLEAN_OPTIONAL },
	[COL_CPU_MHZ]          = { "MHZ", N_("shows the currently MHz of the CPU"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_SCALMHZ]      = { "SCALMHZ%", N_("shows scaling percentage of the CPU frequency"), SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER },
	[COL_CPU_MAXMHZ]       = { "MAXMHZ", N_("shows the maximum MHz of the CPU"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_MINMHZ]       = { "MINMHZ", N_("shows the minimum MHz of the CPU"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CPU_MODELNAME]    = { "MODELNAME", N_("shows CPU model name"), 0, 0, SCOLS_JSON_STRING }
};

static struct lscpu_coldesc coldescs_cache[] =
{
	[COL_CACHE_ALLSIZE]    = { "ALL-SIZE", N_("size of all system caches"), SCOLS_FL_RIGHT },
	[COL_CACHE_LEVEL]      = { "LEVEL", N_("cache level"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CACHE_NAME]       = { "NAME", N_("cache name") },
	[COL_CACHE_ONESIZE]    = { "ONE-SIZE", N_("size of one cache"), SCOLS_FL_RIGHT },
	[COL_CACHE_TYPE]       = { "TYPE", N_("cache type") },
	[COL_CACHE_WAYS]       = { "WAYS", N_("ways of associativity"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CACHE_ALLOCPOL]   = { "ALLOC-POLICY", N_("allocation policy") },
	[COL_CACHE_WRITEPOL]   = { "WRITE-POLICY", N_("write policy") },
	[COL_CACHE_PHYLINE]    = { "PHY-LINE", N_("number of physical cache line per cache tag"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CACHE_SETS]       = { "SETS", N_("number of sets in the cache; set lines has the same cache index"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER },
	[COL_CACHE_COHERENCYSIZE] = { "COHERENCY-SIZE", N_("minimum amount of data in bytes transferred from memory to cache"), SCOLS_FL_RIGHT, 0, SCOLS_JSON_NUMBER }
};

static int hierarchic = -1;

UL_DEBUG_DEFINE_MASK(lscpu);
UL_DEBUG_DEFINE_MASKNAMES(lscpu) = UL_DEBUG_EMPTY_MASKNAMES;

static void lscpu_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(lscpu, LSCPU_DEBUG_, 0, LSCPU_DEBUG);
}

static int
cpu_column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(coldescs_cpu); i++) {
		const char *cn = coldescs_cpu[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int
cache_column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(coldescs_cache); i++) {
		const char *cn = coldescs_cache[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static void lscpu_context_init_paths(struct lscpu_cxt *cxt)
{
	DBG(MISC, ul_debugobj(cxt, "initialize paths"));
	ul_path_init_debug();

	/* / */
	cxt->rootfs = NULL;
	if (cxt->prefix) {
		cxt->rootfs = ul_new_path("/");
		if (!cxt->rootfs)
			err(EXIT_FAILURE, _("failed to initialize rootfs handler"));
		ul_path_set_prefix(cxt->rootfs, cxt->prefix);
	}

	/* /sys/devices/system/cpu */
	cxt->syscpu = ul_new_path(_PATH_SYS_CPU);
	if (!cxt->syscpu)
		err(EXIT_FAILURE, _("failed to initialize CPUs sysfs handler"));
	if (cxt->prefix)
		ul_path_set_prefix(cxt->syscpu, cxt->prefix);

	/* /proc */
	cxt->procfs = ul_new_path("/proc");
	if (!cxt->procfs)
		err(EXIT_FAILURE, _("failed to initialize procfs handler"));
	if (cxt->prefix)
		ul_path_set_prefix(cxt->procfs, cxt->prefix);
}

static struct lscpu_cxt *lscpu_new_context(void)
{
	return xcalloc(1, sizeof(struct lscpu_cxt));
}

static void lscpu_free_context(struct lscpu_cxt *cxt)
{
	size_t i;

	if (!cxt)
		return;

	DBG(MISC, ul_debugobj(cxt, "freeing context"));

	DBG(MISC, ul_debugobj(cxt, " de-initialize paths"));
	ul_unref_path(cxt->syscpu);
	ul_unref_path(cxt->procfs);
	ul_unref_path(cxt->rootfs);

	DBG(MISC, ul_debugobj(cxt, " freeing cpus"));
	for (i = 0; i < cxt->npossibles; i++) {
		lscpu_unref_cpu(cxt->cpus[i]);
		cxt->cpus[i] = NULL;
	}
	DBG(MISC, ul_debugobj(cxt, " freeing types"));
	for (i = 0; i < cxt->ncputypes; i++) {
		lscpu_unref_cputype(cxt->cputypes[i]);
		cxt->cputypes[i] = NULL;
	}

	free(cxt->present);
	free(cxt->online);
	free(cxt->cputypes);
	free(cxt->cpus);

	for (i = 0; i < cxt->nvuls; i++) {
		free(cxt->vuls[i].name);
		free(cxt->vuls[i].text);
	}
	free(cxt->vuls);

	for (i = 0; i < cxt->nnodes; i++)
		free(cxt->nodemaps[i]);

	free(cxt->nodemaps);
	free(cxt->idx2nodenum);

	lscpu_free_virtualization(cxt->virt);
	lscpu_free_architecture(cxt->arch);

	lscpu_free_caches(cxt->ecaches, cxt->necaches);
	lscpu_free_caches(cxt->caches, cxt->ncaches);

	free(cxt);
}

static void __fill_id(	struct lscpu_cxt *cxt,
			struct lscpu_cpu *cpu,
			int id, cpu_set_t **map,
			size_t nitems,
			char *buf, size_t bufsz)
{
	*buf = '\0';

	if (cxt->show_physical) {
		if (id < 0)
			snprintf(buf, bufsz, "-");
		else
			snprintf(buf, bufsz, "%d", id);
	} else if (map) {
		size_t i;

		if (cpuset_ary_isset(cpu->logical_id, map, nitems,
					cxt->setsize, &i) == 0)
			snprintf(buf, bufsz, "%zu", i);
	}
}

static void get_cell_boolean(
		struct lscpu_cxt *cxt,
		int has_data, int data,
		char *buf, size_t bufsz)
{
		if (!has_data)
			return;

		if (cxt->mode == LSCPU_OUTPUT_PARSABLE || cxt->json)
			snprintf(buf, bufsz, "%s",
				 data ? _("Y") : _("N"));
		else
			snprintf(buf, bufsz, "%s",
				 data ? _("yes") : _("no"));
}

#define fill_id(_cxt, _cpu, NAME, _buf, _bufsz) \
		__fill_id(_cxt, (_cpu), \
			(_cpu)-> NAME ## id, \
			(_cpu)->type-> NAME ## maps, \
			(_cpu)->type->n ## NAME ## s, \
			_buf, _bufsz)

static char *get_cell_data(
			struct lscpu_cxt *cxt,
			struct lscpu_cpu *cpu, int col,
			char *buf, size_t bufsz)
{
	size_t i;

	*buf = '\0';

	if (!cpu->type)
		return NULL;

	switch (col) {
	case COL_CPU_CPU:
		snprintf(buf, bufsz, "%d", cpu->logical_id);
		break;
	case COL_CPU_BOGOMIPS:
		if (!cpu->bogomips && !cpu->type->bogomips)
			break;
		snprintf(buf, bufsz, "%.2f", (float) c_strtod(
				cpu->bogomips ? : cpu->type->bogomips, NULL));
		break;
	case COL_CPU_CORE:
		fill_id(cxt, cpu, core, buf, bufsz);
		break;
	case COL_CPU_SOCKET:
		fill_id(cxt, cpu, socket, buf, bufsz);
		break;
	case COL_CPU_CLUSTER:
		if (cxt->is_cluster)
			fill_id(cxt, cpu, socket, buf, bufsz);
		break;
	case COL_CPU_DRAWER:
		fill_id(cxt, cpu, drawer, buf, bufsz);
		break;
	case COL_CPU_BOOK:
		fill_id(cxt, cpu, book, buf, bufsz);
		break;
	case COL_CPU_NODE:
		if (cpuset_ary_isset(cpu->logical_id, cxt->nodemaps,
				     cxt->nnodes, cxt->setsize, &i) == 0)
			snprintf(buf, bufsz, "%d", cxt->idx2nodenum[i]);
		break;
	case COL_CPU_CACHE:
	{
		const char *last = NULL;
		char *p = buf;
		size_t sz = bufsz;

		for (i = 0; i < cxt->ncaches; i++) {
			int x;
			struct lscpu_cache *ca;
			const char *name = cxt->caches[i].name;

			if (last && strcmp(last, name) == 0)
				continue;
			last = name;
			ca = lscpu_cpu_get_cache(cxt, cpu, name);
			if (!ca)
				continue;
			x = snprintf(p, sz, "%d", ca->id);
			if (x < 0 || (size_t) x >= sz)
				return NULL;
			p += x;
			sz -= x;
			if (sz < 2)
				return NULL;
			*p++ = cxt->show_compatible ? ',' : ':';
			*p = '\0';
			sz--;
		}
		if (p > buf && (*(p - 1) == ',' || *(p - 1) == ':'))
			*(p - 1) = '\0';
		break;
	}
	case COL_CPU_POLARIZATION:
		if (cpu->polarization < 0)
			break;
		snprintf(buf, bufsz, "%s",
				cxt->mode == LSCPU_OUTPUT_PARSABLE ?
				polar_modes[cpu->polarization].parsable :
				polar_modes[cpu->polarization].readable);
		break;
	case COL_CPU_ADDRESS:
		if (cpu->address < 0)
			break;
		snprintf(buf, bufsz, "%d", cpu->address);
		break;
	case COL_CPU_CONFIGURED:
		get_cell_boolean(cxt, cpu->configured >= 0, cpu->configured, buf, bufsz);
		break;
	case COL_CPU_ONLINE:
		get_cell_boolean(cxt, !!cxt->online, is_cpu_online(cxt, cpu), buf, bufsz);
		break;
	case COL_CPU_MHZ:
		if (cpu->mhz_cur_freq)
			snprintf(buf, bufsz, "%.4f", cpu->mhz_cur_freq);
		break;
	case COL_CPU_SCALMHZ:
		if (cpu->mhz_cur_freq && cpu->mhz_max_freq)
			snprintf(buf, bufsz, "%.0f%%", cpu->mhz_cur_freq / cpu->mhz_max_freq * 100);
		break;
	case COL_CPU_MAXMHZ:
		if (cpu->mhz_max_freq)
			snprintf(buf, bufsz, "%.4f", cpu->mhz_max_freq);
		break;
	case COL_CPU_MINMHZ:
		if (cpu->mhz_min_freq)
			snprintf(buf, bufsz, "%.4f", cpu->mhz_min_freq);
		break;
	case COL_CPU_MODELNAME:
		if (cpu->type && cpu->type->modelname)
			xstrncpy(buf, cpu->type->modelname, bufsz);
		break;
	}
	return buf;
}

static char *get_cell_header(
			struct lscpu_cxt *cxt, int col,
			char *buf, size_t bufsz)
{
	*buf = '\0';

	if (col == COL_CPU_CACHE) {
		const char *last = NULL;
		char *p = buf;
		size_t sz = bufsz;
		size_t i;

		for (i = 0; i < cxt->ncaches; i++) {
			struct lscpu_cache *ca = &cxt->caches[i];
			int x;

			if (last && strcmp(last, ca->name) == 0)
				continue;
			last = ca->name;

			x = snprintf(p, sz, "%s", ca->name);
			if (x < 0 || (size_t) x >= sz)
				return NULL;
			sz -= x;
			p += x;
			if (sz < 2)
				return NULL;
			*p++ = cxt->show_compatible ? ',' : ':';
			*p = '\0';
			sz--;
		}
		if (p > buf && (*(p - 1) == ',' || *(p - 1) == ':'))
			*(p - 1) = '\0';
		if (cxt->ncaches)
			return buf;
	}
	snprintf(buf, bufsz, "%s", coldescs_cpu[col].name);
	return buf;
}


static void caches_add_line(struct lscpu_cxt *cxt,
			    struct libscols_table *tb,
			    struct lscpu_cache *ca,
			    int cols[], size_t ncols)
{
	struct libscols_line *ln;
	size_t c;

	ln = scols_table_new_line(tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	for (c = 0; c < ncols; c++) {
		char *data = NULL;
		int col = cols[c];

		switch (col) {
		case COL_CACHE_NAME:
			if (ca->name)
				data = xstrdup(ca->name);
			break;
		case COL_CACHE_ONESIZE:
			if (!ca->size)
				break;
			if (cxt->bytes)
				xasprintf(&data, "%" PRIu64, ca->size);
			else
				data = size_to_human_string(SIZE_SUFFIX_1LETTER, ca->size);
			break;
		case COL_CACHE_ALLSIZE:
		{
			uint64_t sz = 0;
			if (ca->name)
				sz = lscpu_get_cache_full_size(cxt, ca->name, NULL);
			if (!sz)
				break;
			if (cxt->bytes)
				xasprintf(&data, "%" PRIu64, sz);
			else
				data = size_to_human_string(SIZE_SUFFIX_1LETTER, sz);
			break;
		}
		case COL_CACHE_WAYS:
			if (ca->ways_of_associativity)
				xasprintf(&data, "%u", ca->ways_of_associativity);
			break;

		case COL_CACHE_TYPE:
			if (ca->type)
				data = xstrdup(ca->type);
			break;
		case COL_CACHE_LEVEL:
			if (ca->level)
				xasprintf(&data, "%d", ca->level);
			break;
		case COL_CACHE_ALLOCPOL:
			if (ca->allocation_policy)
				data = xstrdup(ca->allocation_policy);
			break;
		case COL_CACHE_WRITEPOL:
			if (ca->write_policy)
				data = xstrdup(ca->write_policy);
			break;
		case COL_CACHE_PHYLINE:
			if (ca->physical_line_partition)
				xasprintf(&data, "%u", ca->physical_line_partition);
			break;
		case COL_CACHE_SETS:
			if (ca->number_of_sets)
				xasprintf(&data, "%u", ca->number_of_sets);
			break;
		case COL_CACHE_COHERENCYSIZE:
			if (ca->coherency_line_size)
				xasprintf(&data, "%u", ca->coherency_line_size);
			break;
		}

		if (data && scols_line_refer_data(ln, c, data))
			err(EXIT_FAILURE, _("failed to add output data"));
	}
}


/*
 * [-C] backend
 */
static void print_caches_readable(struct lscpu_cxt *cxt, int cols[], size_t ncols)
{
	size_t i;
	struct libscols_table *tb;
	const char *last = NULL;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		 err(EXIT_FAILURE, _("failed to allocate output table"));
	if (cxt->json) {
		scols_table_enable_json(tb, 1);
		scols_table_set_name(tb, "caches");
	}

	for (i = 0; i < ncols; i++) {
		struct lscpu_coldesc *cd = &coldescs_cache[cols[i]];
		struct libscols_column *cl;

		cl = scols_table_new_column(tb, cd->name, 0, cd->flags);
		if (cl == NULL)
			err(EXIT_FAILURE, _("failed to allocate output column"));
		if (cxt->json)
			scols_column_set_json_type(cl, cd->json_type);
	}

	/* standard caches */
	for (i = 0; i < cxt->ncaches; i++) {
		struct lscpu_cache *ca = &cxt->caches[i];

		if (last && strcmp(last, ca->name) == 0)
			continue;
		last = ca->name;
		caches_add_line(cxt, tb, ca, cols, ncols);
	}

	/* extra caches */
	for (i = 0; i < cxt->necaches; i++) {
		struct lscpu_cache *ca = &cxt->ecaches[i];

		if (last && strcmp(last, ca->name) == 0)
			continue;
		last = ca->name;
		caches_add_line(cxt, tb, ca, cols, ncols);
	}

	scols_print_table(tb);
	scols_unref_table(tb);
}

/*
 * [-p] backend, we support two parsable formats:
 *
 * 1) "compatible" -- this format is compatible with the original lscpu(1)
 * output and it contains fixed set of the columns. The CACHE columns are at
 * the end of the line and the CACHE is not printed if the number of the caches
 * is zero. The CACHE columns are separated by two commas, for example:
 *
 *    $ lscpu --parse
 *    # CPU,Core,Socket,Node,,L1d,L1i,L2
 *    0,0,0,0,,0,0,0
 *    1,1,0,0,,1,1,0
 *
 * 2) "user defined output" -- this format prints always all columns without
 * special prefix for CACHE column. If there are not CACHEs then the column is
 * empty and the header "Cache" is printed rather than a real name of the cache.
 * The CACHE columns are separated by ':'.
 *
 *	$ lscpu --parse=CPU,CORE,SOCKET,NODE,CACHE
 *	# CPU,Core,Socket,Node,L1d:L1i:L2
 *	0,0,0,0,0:0:0
 *	1,1,0,0,1:1:0
 */
static void print_cpus_parsable(struct lscpu_cxt *cxt, int cols[], size_t ncols)
{
	char buf[BUFSIZ], *data;
	size_t i;

	/*
	 * Header
	 */
	printf(_(
	"# The following is the parsable format, which can be fed to other\n"
	"# programs. Each different item in every column has an unique ID\n"
	"# starting usually from zero.\n"));

	fputs("# ", stdout);
	for (i = 0; i < ncols; i++) {
		int col = cols[i];

		if (col == COL_CPU_CACHE) {
			if (cxt->show_compatible && !cxt->ncaches)
				continue;
			if (cxt->show_compatible && i != 0)
				putchar(',');
		}
		if (i > 0)
			putchar(',');

		data = get_cell_header(cxt, col, buf, sizeof(buf));
		if (data && * data && col != COL_CPU_CACHE &&
		    !coldescs_cpu[col].is_abbr) {
			/*
			 * For normal column names use mixed case (e.g. "Socket")
			 */
			char *p = data + 1;

			while (p && *p != '\0') {
				*p = tolower((unsigned int) *p);
				p++;
			}
		}
		fputs(data && *data ? data : "", stdout);
	}
	putchar('\n');

	/*
	 * Data
	 */
	for (i = 0; i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];
		size_t c;

		if (cxt->online) {
			if (!cxt->show_offline && !is_cpu_online(cxt, cpu))
				continue;
			if (!cxt->show_online && is_cpu_online(cxt, cpu))
				continue;
		}
		if (cxt->present && !is_cpu_present(cxt, cpu))
			continue;

		for (c = 0; c < ncols; c++) {
			if (cxt->show_compatible && cols[c] == COL_CPU_CACHE) {
				if (!cxt->ncaches)
					continue;
				if (c > 0)
					putchar(',');
			}
			if (c > 0)
				putchar(',');

			data = get_cell_data(cxt, cpu, cols[c], buf, sizeof(buf));
			fputs(data && *data ? data : "", stdout);
			*buf = '\0';
		}
		putchar('\n');
	}
}

/*
 * [-e] backend
 */
static void print_cpus_readable(struct lscpu_cxt *cxt, int cols[], size_t ncols)
{
	size_t i;
	char buf[BUFSIZ];
	const char *data;
	struct libscols_table *tb;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		 err(EXIT_FAILURE, _("failed to allocate output table"));
	if (cxt->json) {
		scols_table_enable_json(tb, 1);
		scols_table_set_name(tb, "cpus");
	}

	for (i = 0; i < ncols; i++) {
		data = get_cell_header(cxt, cols[i], buf, sizeof(buf));
		struct lscpu_coldesc *cd = &coldescs_cpu[cols[i]];
		struct libscols_column *cl;

		cl = scols_table_new_column(tb, data, 0, cd->flags);
		if (cl == NULL)
			err(EXIT_FAILURE, _("failed to allocate output column"));
		if (cxt->json)
			scols_column_set_json_type(cl, cd->json_type);
	}

	for (i = 0; i < cxt->npossibles; i++) {
		size_t c;
		struct libscols_line *ln;
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (cxt->online) {
			if (!cxt->show_offline && !is_cpu_online(cxt, cpu))
				continue;
			if (!cxt->show_online && is_cpu_online(cxt, cpu))
				continue;
		}

		if (cxt->present && !is_cpu_present(cxt, cpu))
			continue;

		ln = scols_table_new_line(tb, NULL);
		if (!ln)
			err(EXIT_FAILURE, _("failed to allocate output line"));

		for (c = 0; c < ncols; c++) {
			data = get_cell_data(cxt, cpu, cols[c], buf, sizeof(buf));
			if (!data || !*data)
				data = "-";
			if (scols_line_set_data(ln, c, data))
				err(EXIT_FAILURE, _("failed to add output data"));
		}
	}

	scols_print_table(tb);
	scols_unref_table(tb);
}

static struct libscols_line *
	__attribute__ ((__format__(printf, 4, 5)))
	add_summary_sprint(struct libscols_table *tb,
			struct libscols_line *sec,
			const char *txt,
			const char *fmt,
			...)
{
	struct libscols_line *ln;
	va_list args;

	/* Don't print section lines without data */
	if (!hierarchic && fmt == NULL)
		return NULL;

	ln = scols_table_new_line(tb, sec);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	/* description column */
	if (txt && scols_line_set_data(ln, 0, txt))
		err(EXIT_FAILURE, _("failed to add output data"));

	/* data column */
	if (fmt) {
		char *data;
		va_start(args, fmt);
		xvasprintf(&data, fmt, args);
		va_end(args);

		if (data && scols_line_refer_data(ln, 1, data))
			err(EXIT_FAILURE, _("failed to add output data"));
	}

	return ln;
}

#define add_summary_e(tb, sec, txt)		add_summary_sprint(tb, sec, txt, NULL)
#define add_summary_n(tb, sec, txt, num)	add_summary_sprint(tb, sec, txt, "%zu", num)
#define add_summary_s(tb, sec, txt, str)	add_summary_sprint(tb, sec, txt, "%s", str)
#define add_summary_x(tb, sec, txt, fmt, x)	add_summary_sprint(tb, sec, txt, fmt, x)

static void
print_cpuset(struct lscpu_cxt *cxt,
	     struct libscols_table *tb,
	     struct libscols_line *sec,
	     const char *key, cpu_set_t *set)
{
	size_t setbuflen = 7 * cxt->maxcpus;
	char *setbuf, *p;

	assert(set);
	assert(key);
	assert(tb);
	assert(cxt);

	setbuf = xmalloc(setbuflen);

	if (cxt->hex) {
		p = cpumask_create(setbuf, setbuflen, set, cxt->setsize);
		add_summary_s(tb, sec, key, p);
	} else {
		p = cpulist_create(setbuf, setbuflen, set, cxt->setsize);
		add_summary_s(tb, sec, key, p);
	}

	free(setbuf);
}

static void
print_summary_cputype(struct lscpu_cxt *cxt,
		     struct lscpu_cputype *ct,
		     struct libscols_table *tb,
		     struct libscols_line *sec)
{
	sec = add_summary_s(tb, sec, _("Model name:"), ct->modelname ? ct->modelname : "-");
	if (ct->bios_modelname)
		add_summary_s(tb, sec, _("BIOS Model name:"), ct->bios_modelname);
	if (ct->bios_family)
		add_summary_s(tb, sec, _("BIOS CPU family:"), ct->bios_family);
	if (ct->machinetype)
		add_summary_s(tb, sec, _("Machine type:"), ct->machinetype);
	if (ct->family)
		add_summary_s(tb, sec, _("CPU family:"), ct->family);
	if (ct->model || ct->revision)
		add_summary_s(tb, sec, _("Model:"), ct->revision ? ct->revision : ct->model);

	add_summary_n(tb, sec, _("Thread(s) per core:"), ct->nthreads_per_core);
	if (cxt->is_cluster)
		add_summary_n(tb, sec, _("Core(s) per cluster:"), ct->ncores_per_socket);
	else
		add_summary_n(tb, sec, _("Core(s) per socket:"), ct->ncores_per_socket);

	if (ct->nbooks) {
		add_summary_n(tb, sec, _("Socket(s) per book:"), ct->nsockets_per_book);
		if (ct->ndrawers_per_system || ct->ndrawers) {
			add_summary_n(tb, sec, _("Book(s) per drawer:"), ct->nbooks_per_drawer);
			add_summary_n(tb, sec, _("Drawer(s):"), ct->ndrawers_per_system ?: ct->ndrawers);
		} else
			add_summary_n(tb, sec, _("Book(s):"), ct->nbooks_per_drawer ?: ct->nbooks);
	} else {
		if (cxt->is_cluster) {
			if (ct->nr_socket_on_cluster > 0)
				add_summary_n(tb, sec, _("Socket(s):"), ct->nr_socket_on_cluster);
			else
				add_summary_s(tb, sec, _("Socket(s):"), "-");

			add_summary_n(tb, sec, _("Cluster(s):"),
					ct->nsockets_per_book ?: ct->nsockets);
		} else
			add_summary_n(tb, sec, _("Socket(s):"),
					ct->nsockets_per_book ?: ct->nsockets);
	}

	if (ct->stepping)
		add_summary_s(tb, sec, _("Stepping:"), ct->stepping);
	if (ct->freqboost >= 0)
		add_summary_s(tb, sec, _("Frequency boost:"), ct->freqboost ?
				_("enabled") : _("disabled"));

	/* s390 -- from the first CPU where is dynamic/static MHz */
	if (ct->dynamic_mhz)
		add_summary_s(tb, sec, _("CPU dynamic MHz:"), ct->dynamic_mhz);
	if (ct->static_mhz)
		add_summary_s(tb, sec, _("CPU static MHz:"), ct->static_mhz);

	if (ct->has_freq) {
		float scal = lsblk_cputype_get_scalmhz(cxt, ct);
		if (scal > 0.0)
			add_summary_x(tb, sec, _("CPU(s) scaling MHz:"), "%.0f%%", scal);
		add_summary_x(tb, sec, _("CPU max MHz:"), "%.4f", lsblk_cputype_get_maxmhz(cxt, ct));
		add_summary_x(tb, sec, _("CPU min MHz:"), "%.4f", lsblk_cputype_get_minmhz(cxt, ct));
	}
	if (ct->bogomips)
		add_summary_x(tb, sec, _("BogoMIPS:"), "%.2f",  (float) c_strtod(ct->bogomips, NULL));

	if (ct->dispatching >= 0)
		add_summary_s(tb, sec, _("Dispatching mode:"), _(disp_modes[ct->dispatching]));

	if (ct->physsockets) {
		add_summary_n(tb, sec, _("Physical sockets:"), ct->physsockets);
		add_summary_n(tb, sec, _("Physical chips:"), ct->physchips);
		add_summary_n(tb, sec, _("Physical cores/chip:"), ct->physcoresperchip);
	}

	if (ct->flags)
		add_summary_s(tb, sec, _("Flags:"), ct->flags);
}

/*
 * default output
 */
static void print_summary(struct lscpu_cxt *cxt)
{
	struct lscpu_cputype *ct;
	char field[256];
	size_t i = 0;
	struct libscols_table *tb;
	struct libscols_line *sec;
	int hdr_caches = 0;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_enable_noheadings(tb, 1);
	if (cxt->json) {
		scols_table_enable_json(tb, 1);
		scols_table_set_name(tb, "lscpu");
	} else if (hierarchic) {
		struct libscols_symbols *sy = scols_new_symbols();

		if (!sy)
			err_oom();
		scols_symbols_set_branch(sy, "  ");
		scols_symbols_set_vertical(sy, "  ");
		scols_symbols_set_right(sy, "  ");
		scols_table_set_symbols(tb, sy);
		scols_unref_symbols(sy);
	}

	if (scols_table_new_column(tb, "field", 0, hierarchic ? SCOLS_FL_TREE : 0) == NULL ||
	    scols_table_new_column(tb, "data", 0, SCOLS_FL_NOEXTREMES | SCOLS_FL_WRAP) == NULL)
		err(EXIT_FAILURE, _("failed to initialize output column"));

	ct = lscpu_cputype_get_default(cxt);

	/* Section: architecture */
	sec = add_summary_s(tb, NULL, _("Architecture:"), cxt->arch->name);
	if (cxt->arch->bit32 || cxt->arch->bit64) {
		const char *p;

		if (cxt->arch->bit32 && cxt->arch->bit64)
			p = "32-bit, 64-bit";
		else if (cxt->arch->bit32)
			p = "32-bit";
		else
			p = "64-bit";

		add_summary_s(tb, sec, _("CPU op-mode(s):"), p);
	}
	if (ct && ct->addrsz)
		add_summary_s(tb, sec, _("Address sizes:"), ct->addrsz);

	if (sysfs_get_byteorder(cxt->rootfs) == SYSFS_BYTEORDER_LITTLE)
		add_summary_s(tb, sec, _("Byte Order:"), "Little Endian");
	else
		add_summary_s(tb, sec, _("Byte Order:"), "Big Endian");

	/* Section: CPU lists */
	sec = add_summary_n(tb, NULL, _("CPU(s):"), cxt->npresents);

	if (cxt->online)
		print_cpuset(cxt, tb, sec,
				cxt->hex ? _("On-line CPU(s) mask:") :
					   _("On-line CPU(s) list:"),
				cxt->online);

	if (cxt->online && cxt->nonlines != cxt->npresents) {
		cpu_set_t *set;

		/* Linux kernel provides cpuset of off-line CPUs that contains
		 * all configured CPUs (see /sys/devices/system/cpu/offline),
		 * but want to print real (present in system) off-line CPUs only.
		 */
		set = cpuset_alloc(cxt->maxcpus, NULL, NULL);
		if (!set)
			err(EXIT_FAILURE, _("failed to callocate cpu set"));
		CPU_ZERO_S(cxt->setsize, set);
		for (i = 0; i < cxt->npossibles; i++) {
			struct lscpu_cpu *cpu = cxt->cpus[i];

			if (cpu && is_cpu_present(cxt, cpu) && !is_cpu_online(cxt, cpu))
				CPU_SET_S(cpu->logical_id, cxt->setsize, set);
		}
		print_cpuset(cxt, tb, sec,
				cxt->hex ? _("Off-line CPU(s) mask:") :
					   _("Off-line CPU(s) list:"), set);
		cpuset_free(set);
	}
	sec = NULL;

	/* Section: cpu type description */
	if (ct && ct->vendor)
		sec = add_summary_s(tb, NULL, _("Vendor ID:"), ct->vendor);
	if (ct && ct->bios_vendor)
		add_summary_s(tb, sec, _("BIOS Vendor ID:"), ct->bios_vendor);

	for (i = 0; i < cxt->ncputypes; i++)
		print_summary_cputype(cxt, cxt->cputypes[i], tb, sec);
	sec = NULL;

	/* Section: vitualiazation */
	if (cxt->virt) {
		sec = add_summary_e(tb, NULL, _("Virtualization features:"));
		if (cxt->virt->cpuflag && !strcmp(cxt->virt->cpuflag, "svm"))
			add_summary_s(tb, sec, _("Virtualization:"), "AMD-V");
		else if (cxt->virt->cpuflag && !strcmp(cxt->virt->cpuflag, "vmx"))
			add_summary_s(tb, sec, _("Virtualization:"), "VT-x");

		if (cxt->virt->hypervisor)
			add_summary_s(tb, sec, _("Hypervisor:"), cxt->virt->hypervisor);
		if (cxt->virt->vendor) {
			add_summary_s(tb, sec, _("Hypervisor vendor:"), hv_vendors[cxt->virt->vendor]);
			add_summary_s(tb, sec, _("Virtualization type:"), _(virt_types[cxt->virt->type]));
		}
		sec = NULL;
	}

	/* Section: caches */
	if (cxt->ncaches) {
		const char *last = NULL;

		/* The caches are sorted by name, cxt->caches[] may contains
		 * multiple instances for the same name.
		 */
		for (i = 0; i < cxt->ncaches; i++) {
			const char *name = cxt->caches[i].name;
			uint64_t sz;
			int n = 0;

			if (last && strcmp(last, name) == 0)
				continue;
			sz = lscpu_get_cache_full_size(cxt, name, &n);
			if (!sz)
				continue;
			if (!hdr_caches) {
				sec = add_summary_e(tb, NULL, _("Caches (sum of all):"));
				hdr_caches = 1;
			}

			snprintf(field, sizeof(field), hierarchic ? _("%s:") : _("%s cache:"), name);
			if (cxt->bytes)
				add_summary_sprint(tb, sec, field,
						P_("%" PRIu64 " (%d instance)",
						   "%" PRIu64 " (%d instances)", n),
						sz, n);
			else {
				char *tmp = size_to_human_string(
						SIZE_SUFFIX_3LETTER |
						SIZE_SUFFIX_SPACE,
						sz);
				add_summary_sprint(tb, sec, field,
						P_("%s (%d instance)",
						   "%s (%d instances)", n),
						tmp, n);
				free(tmp);
			}
			last = name;
		}
	}

	for (i = 0; i < cxt->necaches; i++) {
		struct lscpu_cache *ca = &cxt->ecaches[i];

		if (ca->size == 0)
			continue;
		if (!hdr_caches) {
			sec = add_summary_e(tb, NULL, _("Caches:"));
			hdr_caches = 1;
		}
		snprintf(field, sizeof(field), hierarchic ? _("%s:") : _("%s cache:"), ca->name);
		if (cxt->bytes)
			add_summary_x(tb, sec, field, "%" PRIu64, ca->size);
		else {
			char *tmp = size_to_human_string(
					SIZE_SUFFIX_3LETTER |
					SIZE_SUFFIX_SPACE,
					ca->size);
			add_summary_s(tb, sec, field, tmp);
			free(tmp);
		}
	}
	sec = NULL;

	/* Section: NUMA modes */
	if (cxt->nnodes) {
		sec = add_summary_e(tb, NULL, _("NUMA:"));

		add_summary_n(tb, sec,_("NUMA node(s):"), cxt->nnodes);
		for (i = 0; i < cxt->nnodes; i++) {
			snprintf(field, sizeof(field), _("NUMA node%d CPU(s):"), cxt->idx2nodenum[i]);
			print_cpuset(cxt, tb, sec, field, cxt->nodemaps[i]);
		}
		sec = NULL;
	}

	/* Section: Vulnerabilities */
	if (cxt->vuls) {
		sec = add_summary_e(tb, NULL, _("Vulnerabilities:"));

		for (i = 0; i < cxt->nvuls; i++) {
			snprintf(field, sizeof(field), hierarchic ?
					_("%s:") : _("Vulnerability %s:"), cxt->vuls[i].name);
			add_summary_s(tb, sec, field, cxt->vuls[i].text);
		}
		sec = NULL;
	}
	scols_print_table(tb);
	scols_unref_table(tb);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display information about the CPU architecture.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all               print both online and offline CPUs (default for -e)\n"), out);
	fputs(_(" -b, --online            print online CPUs only (default for -p)\n"), out);
	fputs(_(" -B, --bytes             print sizes in bytes rather than in human readable format\n"), out);
	fputs(_(" -C, --caches[=<list>]   info about caches in extended readable format\n"), out);
	fputs(_(" -c, --offline           print offline CPUs only\n"), out);
	fputs(_(" -J, --json              use JSON for default or extended format\n"), out);
	fputs(_(" -e, --extended[=<list>] print out an extended readable format\n"), out);
	fputs(_(" -p, --parse[=<list>]    print out a parsable format\n"), out);
	fputs(_(" -s, --sysroot <dir>     use specified directory as system root\n"), out);
	fputs(_(" -x, --hex               print hexadecimal masks rather than lists of CPUs\n"), out);
	fputs(_(" -y, --physical          print physical instead of logical IDs\n"), out);
	fputs(_("     --hierarchic[=when] use subsections in summary (auto, never, always)\n"), out);
	fputs(_("     --output-all        print all available columns for -e, -p or -C\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));

	fputs(_("\nAvailable output columns for -e or -p:\n"), out);
	for (i = 0; i < ARRAY_SIZE(coldescs_cpu); i++)
		fprintf(out, " %13s  %s\n", coldescs_cpu[i].name, _(coldescs_cpu[i].help));

	fputs(_("\nAvailable output columns for -C:\n"), out);
	for (i = 0; i < ARRAY_SIZE(coldescs_cache); i++)
		fprintf(out, " %13s  %s\n", coldescs_cache[i].name, _(coldescs_cache[i].help));

	fprintf(out, USAGE_MAN_TAIL("lscpu(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct lscpu_cxt *cxt;
	int c, all = 0;
	int columns[ARRAY_SIZE(coldescs_cpu)];
	int cpu_modifier_specified = 0;
	char *outarg = NULL;
	size_t i, ncolumns = 0;
	enum {
		OPT_OUTPUT_ALL = CHAR_MAX + 1,
		OPT_HIERARCHIC,
	};
	static const struct option longopts[] = {
		{ "all",        no_argument,       NULL, 'a' },
		{ "online",     no_argument,       NULL, 'b' },
		{ "bytes",      no_argument,       NULL, 'B' },
		{ "caches",     optional_argument, NULL, 'C' },
		{ "offline",    no_argument,       NULL, 'c' },
		{ "help",	no_argument,       NULL, 'h' },
		{ "extended",	optional_argument, NULL, 'e' },
		{ "json",       no_argument,       NULL, 'J' },
		{ "parse",	optional_argument, NULL, 'p' },
		{ "sysroot",	required_argument, NULL, 's' },
		{ "physical",	no_argument,	   NULL, 'y' },
		{ "hex",	no_argument,	   NULL, 'x' },
		{ "version",	no_argument,	   NULL, 'V' },
		{ "output-all",	no_argument,	   NULL, OPT_OUTPUT_ALL },
		{ "hierarchic", optional_argument, NULL, OPT_HIERARCHIC },
		{ NULL,		0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'C','e','p' },
		{ 'a','b','c' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	cxt = lscpu_new_context();

	while ((c = getopt_long(argc, argv, "aBbC::ce::hJp::s:xyV", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'a':
			cxt->show_online = cxt->show_offline = 1;
			cpu_modifier_specified = 1;
			break;
		case 'B':
			cxt->bytes = 1;
			break;
		case 'b':
			cxt->show_online = 1;
			cpu_modifier_specified = 1;
			break;
		case 'c':
			cxt->show_offline = 1;
			cpu_modifier_specified = 1;
			break;
		case 'C':
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				outarg = optarg;
			}
			cxt->mode = LSCPU_OUTPUT_CACHES;
			break;
		case 'J':
			cxt->json = 1;
			break;
		case 'p':
		case 'e':
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				outarg = optarg;
			}
			cxt->mode = c == 'p' ? LSCPU_OUTPUT_PARSABLE : LSCPU_OUTPUT_READABLE;
			break;
		case 's':
			cxt->prefix = optarg;
			cxt->noalive = 1;
			break;
		case 'x':
			cxt->hex = 1;
			break;
		case 'y':
			cxt->show_physical = 1;
			break;
		case OPT_OUTPUT_ALL:
			all = 1;
			break;
		case OPT_HIERARCHIC:
			if (optarg) {
				if (strcmp(optarg, "auto") == 0)
					hierarchic = -1;
				else if (strcmp(optarg, "never") == 0)
					hierarchic = 0;
				else if (strcmp(optarg, "always") == 0)
					hierarchic = 1;
				else
					errx(EXIT_FAILURE, _("unsupported --flat argument"));
			} else
				hierarchic = 1;
			break;
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (all && ncolumns == 0) {
		size_t maxsz = cxt->mode == LSCPU_OUTPUT_CACHES ?
				ARRAY_SIZE(coldescs_cache) :
				ARRAY_SIZE(coldescs_cpu);

		for (i = 0; i < maxsz; i++)
			columns[ncolumns++] = i;
	}

	if (cpu_modifier_specified && cxt->mode == LSCPU_OUTPUT_SUMMARY) {
		fprintf(stderr,
			_("%s: options --all, --online and --offline may only "
			  "be used with options --extended or --parse.\n"),
			program_invocation_short_name);
		return EXIT_FAILURE;
	}

	if (argc != optind) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	/* set default cpu display mode if none was specified */
	if (!cxt->show_online && !cxt->show_offline) {
		cxt->show_online = 1;
		cxt->show_offline = cxt->mode == LSCPU_OUTPUT_READABLE ? 1 : 0;
	}


	lscpu_init_debug();

	lscpu_context_init_paths(cxt);

	lscpu_read_cpulists(cxt);
	lscpu_read_cpuinfo(cxt);
	cxt->arch = lscpu_read_architecture(cxt);

	lscpu_read_archext(cxt);
	lscpu_read_vulnerabilities(cxt);
	lscpu_read_numas(cxt);
	lscpu_read_topology(cxt);

	lscpu_decode_arm(cxt);

	cxt->virt = lscpu_read_virtualization(cxt);

	if (hierarchic == -1)
		hierarchic = isatty(STDOUT_FILENO);	/* default */

	switch(cxt->mode) {
	case LSCPU_OUTPUT_SUMMARY:
		print_summary(cxt);
		break;
	case LSCPU_OUTPUT_CACHES:
		if (!ncolumns) {
			columns[ncolumns++] = COL_CACHE_NAME;
			columns[ncolumns++] = COL_CACHE_ONESIZE;
			columns[ncolumns++] = COL_CACHE_ALLSIZE;
			columns[ncolumns++] = COL_CACHE_WAYS;
			columns[ncolumns++] = COL_CACHE_TYPE;
			columns[ncolumns++] = COL_CACHE_LEVEL;
			columns[ncolumns++] = COL_CACHE_SETS;
			columns[ncolumns++] = COL_CACHE_PHYLINE;
			columns[ncolumns++] = COL_CACHE_COHERENCYSIZE;
		}
		if (outarg && string_add_to_idarray(outarg, columns,
					ARRAY_SIZE(columns),
					&ncolumns, cache_column_name_to_id) < 0)
			return EXIT_FAILURE;

		print_caches_readable(cxt, columns, ncolumns);
		break;
	case LSCPU_OUTPUT_READABLE:
		if (!ncolumns) {
			/* No list was given. Just print whatever is there. */
			struct lscpu_cputype *ct = lscpu_cputype_get_default(cxt);

			columns[ncolumns++] = COL_CPU_CPU;
			if (cxt->nnodes)
				columns[ncolumns++] = COL_CPU_NODE;
			if (ct && ct->ndrawers)
				columns[ncolumns++] = COL_CPU_DRAWER;
			if (ct && ct->nbooks)
				columns[ncolumns++] = COL_CPU_BOOK;
			if (ct && ct->nsockets) {
				if (cxt->is_cluster)
					columns[ncolumns++] = COL_CPU_CLUSTER;
				else
					columns[ncolumns++] = COL_CPU_SOCKET;
			}
			if (ct && ct->ncores)
				columns[ncolumns++] = COL_CPU_CORE;
			if (cxt->ncaches)
				columns[ncolumns++] = COL_CPU_CACHE;
			if (cxt->online)
				columns[ncolumns++] = COL_CPU_ONLINE;
			if (ct && ct->has_configured)
				columns[ncolumns++] = COL_CPU_CONFIGURED;
			if (ct && ct->has_polarization)
				columns[ncolumns++] = COL_CPU_POLARIZATION;
			if (ct && ct->has_addresses)
				columns[ncolumns++] = COL_CPU_ADDRESS;
			if (ct && ct->has_freq) {
				columns[ncolumns++] = COL_CPU_MAXMHZ;
				columns[ncolumns++] = COL_CPU_MINMHZ;
				columns[ncolumns++] = COL_CPU_MHZ;
			}
		}
		if (outarg && string_add_to_idarray(outarg, columns,
					ARRAY_SIZE(columns),
					&ncolumns, cpu_column_name_to_id) < 0)
			return EXIT_FAILURE;
		print_cpus_readable(cxt, columns, ncolumns);
		break;
	case LSCPU_OUTPUT_PARSABLE:
		cxt->show_compatible = 1;
		if (!ncolumns) {
			columns[ncolumns++] = COL_CPU_CPU;
			columns[ncolumns++] = COL_CPU_CORE;
			if (cxt->is_cluster)
				columns[ncolumns++] = COL_CPU_CLUSTER;
			else
				columns[ncolumns++] = COL_CPU_SOCKET;
			columns[ncolumns++] = COL_CPU_NODE;
			columns[ncolumns++] = COL_CPU_CACHE;
		}
		if (outarg) {
			if (string_add_to_idarray(outarg, columns,
					ARRAY_SIZE(columns),
					&ncolumns, cpu_column_name_to_id) < 0)
				return EXIT_FAILURE;
			cxt->show_compatible = 0;
		}

		print_cpus_parsable(cxt, columns, ncolumns);
		break;
	}

	lscpu_free_context(cxt);

	return EXIT_SUCCESS;
}
