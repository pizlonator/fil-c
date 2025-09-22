/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2013 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2013-2023 Karel Zak <kzak@redhat.com>
 */
#ifndef LSCPU_H
#define LSCPU_H

#include "c.h"
#include "nls.h"
#include "cpuset.h"
#include "xalloc.h"
#include "strutils.h"
#include "bitops.h"
#include "path.h"
#include "pathnames.h"
#include "all-io.h"
#include "debug.h"

#define LSCPU_DEBUG_INIT	(1 << 1)
#define LSCPU_DEBUG_MISC	(1 << 2)
#define LSCPU_DEBUG_GATHER	(1 << 3)
#define LSCPU_DEBUG_TYPE	(1 << 4)
#define LSCPU_DEBUG_CPU		(1 << 5)
#define LSCPU_DEBUG_VIRT	(1 << 6)
#define LSBLK_DEBUG_ALL		0xFFFF

UL_DEBUG_DECLARE_MASK(lscpu);
#define DBG(m, x)       __UL_DBG(lscpu, LSCPU_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(lscpu, LSCPU_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(lscpu)
#include "debugobj.h"

#define _PATH_SYS_SYSTEM	"/sys/devices/system"
#define _PATH_SYS_HYP_FEATURES	"/sys/hypervisor/properties/features"
#define _PATH_SYS_CPU		_PATH_SYS_SYSTEM "/cpu"
#define _PATH_SYS_NODE		_PATH_SYS_SYSTEM "/node"
#define _PATH_SYS_DMI		"/sys/firmware/dmi/tables/DMI"
#define _PATH_ACPI_PPTT		"/sys/firmware/acpi/tables/PPTT"

struct lscpu_cache {
	int		id;		/* unique identifier */
	int		nth;		/* cache<number> from cpuinfo */
	char		*name;
	char		*type;
	char		*allocation_policy;
	char		*write_policy;

	int		level;
	uint64_t	size;

	unsigned int	ways_of_associativity;
	unsigned int	physical_line_partition;
	unsigned int	number_of_sets;
	unsigned int	coherency_line_size;

	cpu_set_t	*sharedmap;
};

struct lscpu_cputype {
	int	refcount;

	char	*vendor;
	int	vendor_id;	/* created by lscpu_decode_arm() */
	char	*bios_vendor;	/* aarch64 */
	char	*machinetype;	/* s390 */
	char	*family;
	char	*model;
	char	*modelname;
	char	*bios_modelname; /* aarch64 */
	char	*bios_family; /* aarch64 */
	char	*revision;	/* alternative for model (ppc) */
	char	*stepping;
	char    *bogomips;
	char	*flags;
	char	*mtid;		/* maximum thread id (s390) */
	char	*addrsz;	/* address sizes */
	int	dispatching;	/* -1 if not evailable, DIST_* */
	int	freqboost;	/* -1 if not evailable */

	size_t	physsockets;	/* Physical sockets (modules) */
	size_t	physchips;	/* Physical chips */
	size_t	physcoresperchip;	/* Physical cores per chip */

	size_t	nthreads_per_core;
	size_t	ncores_per_socket;
	size_t	nsockets_per_book;
	size_t	nbooks_per_drawer;
	size_t	ndrawers_per_system;

	char	*dynamic_mhz;	/* s390; copy from the first CPU */
	char	*static_mhz;	/* s390; copy from the first CPU */

	/* siblings maps */
	size_t		ncores;
	cpu_set_t	**coremaps;
	size_t		nsockets;
	cpu_set_t       **socketmaps;
	size_t		nbooks;
	cpu_set_t	**bookmaps;
	size_t		ndrawers;
	cpu_set_t	**drawermaps;

	unsigned int	has_freq : 1,
			has_configured : 1,
			has_polarization : 1,
			has_addresses : 1;

	size_t nr_socket_on_cluster; /* the number of sockets if the is_cluster is 1 */

	char	*isa;	/* loongarch */
};

/* dispatching modes */
enum {
	DISP_HORIZONTAL = 0,
	DISP_VERTICAL	= 1
};

/* cpu polarization */
enum {
	POLAR_UNKNOWN	= 0,
	POLAR_VLOW,
	POLAR_VMEDIUM,
	POLAR_VHIGH,
	POLAR_HORIZONTAL
};

struct lscpu_cpu {
	int refcount;
	struct lscpu_cputype *type;

	int logical_id;

	char	*bogomips;	/* per-CPU bogomips */
	char	*mhz;		/* freq from cpuinfo */
	char	*dynamic_mhz;   /* from cpuinf for s390 */
	char	*static_mhz;	/* from cpuinf for s390 */
	float	mhz_max_freq;	/* realtime freq from /sys/.../cpuinfo_max_freq */
	float	mhz_min_freq;	/* realtime freq from /sys/.../cpuinfo_min_freq */
	float   mhz_cur_freq;

	int	coreid;
	int	socketid;
	int	bookid;
	int	drawerid;

	int	polarization;	/* POLAR_* */
	int	address;	/* physical cpu address */
	int	configured;	/* cpu configured */
};

struct lscpu_arch {
	char	*name;		/* uname() .machine */

	unsigned int	bit32:1,
			bit64:1;
};

struct lscpu_vulnerability {
	char	*name;
	char	*text;
};

/* virtualization types */
enum {
	VIRT_TYPE_NONE	= 0,
	VIRT_TYPE_PARA,
	VIRT_TYPE_FULL,
	VIRT_TYPE_CONTAINER
};

/* hypervisor vendors */
enum {
	VIRT_VENDOR_NONE	= 0,
	VIRT_VENDOR_XEN,
	VIRT_VENDOR_KVM,
	VIRT_VENDOR_MSHV,
	VIRT_VENDOR_VMWARE,
	VIRT_VENDOR_IBM,		/* sys-z powervm */
	VIRT_VENDOR_VSERVER,
	VIRT_VENDOR_UML,
	VIRT_VENDOR_INNOTEK,		/* VBOX */
	VIRT_VENDOR_HITACHI,
	VIRT_VENDOR_PARALLELS,	/* OpenVZ/VIrtuozzo */
	VIRT_VENDOR_VBOX,
	VIRT_VENDOR_OS400,
	VIRT_VENDOR_PHYP,
	VIRT_VENDOR_SPAR,
	VIRT_VENDOR_WSL,
};

struct lscpu_virt {
	char	*cpuflag;	/* virtualization flag (vmx, svm) */
	char	*hypervisor;	/* hypervisor software */
	int	vendor;		/* VIRT_VENDOR_* */
	int	type;		/* VIRT_TYPE_* ? */

};

enum {
	LSCPU_OUTPUT_SUMMARY = 0,	/* default */
	LSCPU_OUTPUT_CACHES,
	LSCPU_OUTPUT_PARSABLE,
	LSCPU_OUTPUT_READABLE
};

struct lscpu_cxt {
	int maxcpus;		/* size in bits of kernel cpu mask */
	size_t setsize;
	const char *prefix;	/* path to /sys and /proc snapshot or NULL */

	struct path_cxt	*syscpu; /* _PATH_SYS_CPU path handler */
	struct path_cxt *procfs; /* /proc path handler */
	struct path_cxt *rootfs; /* / path handler */

	size_t ncputypes;
	struct lscpu_cputype **cputypes;

	size_t npossibles;	/* number of possible CPUs */
	struct lscpu_cpu **cpus; /* possible CPUs, contains gaps (cups[n]=NULL) */

	size_t npresents;
	cpu_set_t *present;	/* mask with present CPUs */

	size_t nonlines;	/* aka number of trhreads */
	cpu_set_t *online;	/* mask with online CPUs */

	struct lscpu_arch *arch;
	struct lscpu_virt *virt;

	struct lscpu_vulnerability *vuls;	/* array of CPU vulnerabilities */
	size_t  nvuls;				/* number of CPU vulnerabilities */

	struct lscpu_cache *caches;		/* all instances of the all caches from /sys */
	size_t ncaches;

	struct lscpu_cache *ecaches;
	size_t necaches;		/* extra caches (s390) from /proc/cpuinfo */

	size_t nnodes;		/* number of NUMA modes */
	int *idx2nodenum;	/* Support for discontinuous nodes */
	cpu_set_t **nodemaps;	/* array with NUMA nodes */

	int mode;	/* LSCPU_OUTPUT_* */

	unsigned int noalive : 1,
		     show_online : 1,
		     show_offline : 1,
		     show_physical : 1,
		     show_compatible : 1,
		     hex : 1,
		     json : 1,
		     bytes : 1;

	int is_cluster; /* For aarch64 if the machine doesn't have ACPI PPTT */
};

#define is_cpu_online(_cxt, _cpu) \
		((_cxt) && (_cpu) && (_cxt)->online && \
		 CPU_ISSET_S((_cpu)->logical_id, (_cxt)->setsize, (_cxt)->online))

#define is_cpu_present(_cxt, _cpu) \
		((_cxt) && (_cpu) && (_cxt)->present && \
		 CPU_ISSET_S((_cpu)->logical_id, (_cxt)->setsize, (_cxt)->present))

struct lscpu_cputype *lscpu_new_cputype(void);
void lscpu_ref_cputype(struct lscpu_cputype *ct);
void lscpu_unref_cputype(struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_add_cputype(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_cputype_get_default(struct lscpu_cxt *cxt);

int lscpu_read_cpuinfo(struct lscpu_cxt *cxt);
int lscpu_read_cpulists(struct lscpu_cxt *cxt);
int lscpu_read_archext(struct lscpu_cxt *cxt);
int lscpu_read_vulnerabilities(struct lscpu_cxt *cxt);
int lscpu_read_numas(struct lscpu_cxt *cxt);

void lscpu_free_caches(struct lscpu_cache *caches, size_t n);
void lscpu_sort_caches(struct lscpu_cache *caches, size_t n);

size_t lscpu_get_cache_full_size(struct lscpu_cxt *cxt, const char *name, int *instances);
struct lscpu_cache *lscpu_cpu_get_cache(struct lscpu_cxt *cxt,
                                struct lscpu_cpu *cpu, const char *name);

int lscpu_read_topology(struct lscpu_cxt *cxt);
void lscpu_cputype_free_topology(struct lscpu_cputype *ct);

float lsblk_cputype_get_maxmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);
float lsblk_cputype_get_minmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);
float lsblk_cputype_get_scalmhz(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);

struct lscpu_arch *lscpu_read_architecture(struct lscpu_cxt *cxt);
void lscpu_free_architecture(struct lscpu_arch *ar);

struct lscpu_virt *lscpu_read_virtualization(struct lscpu_cxt *cxt);
void lscpu_free_virtualization(struct lscpu_virt *virt);

struct lscpu_cpu *lscpu_new_cpu(int id);
void lscpu_ref_cpu(struct lscpu_cpu *cpu);
void lscpu_unref_cpu(struct lscpu_cpu *cpu);
struct lscpu_cpu *lscpu_get_cpu(struct lscpu_cxt *cxt, int logical_id);
int lscpu_cpu_set_type(struct lscpu_cpu *cpu, struct lscpu_cputype *type);
int lscpu_create_cpus(struct lscpu_cxt *cxt, cpu_set_t *cpuset, size_t setsize);
struct lscpu_cpu *lscpu_cpus_loopup_by_type(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);

void lscpu_decode_arm(struct lscpu_cxt *cxt);

int lookup(char *line, char *pattern, char **value);

void *get_mem_chunk(size_t base, size_t len, const char *devmem);

struct lscpu_dmi_header
{
	uint8_t type;
	uint8_t length;
	uint16_t handle;
	uint8_t *data;
};

struct dmi_info {
	char *vendor;
	char *product;
	char *manufacturer;
	int sockets;

	/* Processor Information */
	uint16_t processor_family;
	char *processor_manufacturer;
	char *processor_version;
	uint16_t current_speed;
	char *part_num;
};


void to_dmi_header(struct lscpu_dmi_header *h, uint8_t *data);
char *dmi_string(const struct lscpu_dmi_header *dm, uint8_t s);
int parse_dmi_table(uint16_t len, uint16_t num, uint8_t *data, struct dmi_info *di);
size_t get_number_of_physical_sockets_from_dmi(void);
int dmi_decode_cputype(struct lscpu_cputype *);
#endif /* LSCPU_H */
