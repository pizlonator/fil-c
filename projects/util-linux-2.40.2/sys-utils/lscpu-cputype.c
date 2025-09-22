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
#include <sys/utsname.h>
#include <sys/personality.h>

#if defined(HAVE_LIBRTAS)
# include <librtas.h>
#endif

#include "lscpu.h"

#include "fileutils.h"
#include "c_strtod.h"

/* Lookup a pattern and get the value for format  "<pattern> : <key>"
 */
int lookup(char *line, char *pattern, char **value)
{
	char *p, *v;
	int len = strlen(pattern);

	/* don't re-fill already found tags, first one wins */
	if (!*line || *value)
		return 0;
	/* pattern */
	if (strncmp(line, pattern, len))
		return 0;
	/* white spaces */
	for (p = line + len; isspace(*p); p++);

	/* separator */
	if (*p != ':')
		return 0;
	/* white spaces */
	for (++p; isspace(*p); p++);

	/* value */
	if (!*p)
		return 0;
	v = p;

	/* end of value */
	len = strlen(line) - 1;
	for (p = line + len; isspace(*(p-1)); p--);
	*p = '\0';

	*value = xstrdup(v);
	return 1;
}

struct lscpu_cputype *lscpu_new_cputype(void)
{
	struct lscpu_cputype *ct;

	ct = xcalloc(1, sizeof(struct lscpu_cputype));
	ct->refcount = 1;
	ct->dispatching = -1;
	ct->freqboost = -1;

	DBG(TYPE, ul_debugobj(ct, "alloc"));
	return ct;
}

void lscpu_ref_cputype(struct lscpu_cputype *ct)
{
	if (ct) {
		ct->refcount++;
		DBG(TYPE, ul_debugobj(ct, ">>> ref %d", ct->refcount));
	}
}

void lscpu_unref_cputype(struct lscpu_cputype *ct)
{
	if (!ct)
		return;

	/*DBG(TYPE, ul_debugobj(ct, ">>> unref %d", ct->refcount - 1));*/

	if (--ct->refcount <= 0) {
		DBG(TYPE, ul_debugobj(ct, "  freeing %s/%s", ct->vendor, ct->model));
		lscpu_cputype_free_topology(ct);
		free(ct->vendor);
		free(ct->bios_vendor);
		free(ct->machinetype);	/* s390 */
		free(ct->family);
		free(ct->model);
		free(ct->modelname);
		free(ct->bios_modelname);
		free(ct->bios_family);
		free(ct->revision);	/* alternative for model (ppc) */
		free(ct->stepping);
		free(ct->bogomips);
		free(ct->flags);
		free(ct->mtid);		/* maximum thread id (s390) */
		free(ct->addrsz);	/* address sizes */
		free(ct->static_mhz);
		free(ct->dynamic_mhz);
		free(ct);
	}
}

struct lscpu_cputype *lscpu_cputype_get_default(struct lscpu_cxt *cxt)
{
	return cxt->cputypes ? cxt->cputypes[0] : NULL;
}

#define match(astr, bstr) \
		((!astr && !bstr) || (astr && bstr && strcmp(astr, bstr) == 0))

struct lscpu_cputype *lscpu_add_cputype(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	DBG(TYPE, ul_debugobj(ct, "add new"));
	cxt->cputypes = xreallocarray(cxt->cputypes, cxt->ncputypes + 1,
				      sizeof(struct lscpu_cputype *));
	cxt->cputypes[cxt->ncputypes] = ct;
	cxt->ncputypes++;
	lscpu_ref_cputype(ct);
	return ct;
}

static void fprintf_cputypes(FILE *f, struct lscpu_cxt *cxt)
{
	size_t i;

	for (i = 0; i <	cxt->ncputypes; i++) {
		struct lscpu_cputype *ct = cxt->cputypes[i];

		fprintf(f, "\n vendor: %s\n", ct->vendor);
		fprintf(f, " machinetype: %s\n", ct->machinetype);
		fprintf(f, " family: %s\n", ct->family);
		fprintf(f, " model: %s\n", ct->model);
		fprintf(f, " modelname: %s\n", ct->modelname);
		fprintf(f, " revision: %s\n", ct->revision);
		fprintf(f, " stepping: %s\n", ct->stepping);
		fprintf(f, " mtid: %s\n", ct->mtid);
		fprintf(f, " addrsz: %s\n", ct->addrsz);
	}
}

enum {
	CPUINFO_LINE_UNKNOWN,	/* unknown line */
	CPUINFO_LINE_CPUTYPE,	/* line found in type_patterns[] */
	CPUINFO_LINE_CPU,	/* line found in cpu_patterns[] */
	CPUINFO_LINE_CACHE	/* line found in cache_pattern[] */
};

/* Describes /proc/cpuinfo fields */
struct cpuinfo_pattern {
	int id;			/* field ID */
	int domain;		/* CPUINFO_LINE_* */
	const char *pattern;	/* field name as used in /proc/cpuinfo */
	size_t	offset;		/* offset in lscpu_cputype or lscpu_cpu struct */
};

/* field identifiers (field name may be different on different archs) */
enum {
	PAT_ADDRESS_SIZES,
	PAT_BOGOMIPS,		/* global */
	PAT_BOGOMIPS_CPU,	/* per-cpu */
	PAT_CPU,
	PAT_FAMILY,
	PAT_FEATURES,
	PAT_FLAGS,
	PAT_IMPLEMENTER,
	PAT_MAX_THREAD_ID,
	PAT_MHZ,
	PAT_MHZ_DYNAMIC,
	PAT_MHZ_STATIC,
	PAT_MODEL,
	PAT_MODEL_NAME,
	PAT_PART,
	PAT_PROCESSOR,
	PAT_REVISION,
	PAT_STEPPING,
	PAT_TYPE,
	PAT_VARIANT,
	PAT_VENDOR,
	PAT_CACHE,
	PAT_ISA,
};

/*
 * /proc/cpuinfo to lscpu_cputype conversion
 */
#define DEF_PAT_CPUTYPE(_str, _id, _member) \
	{ \
		.id = (_id), \
		.domain = CPUINFO_LINE_CPUTYPE, \
		.pattern = (_str), \
		.offset = offsetof(struct lscpu_cputype, _member), \
	}

static const struct cpuinfo_pattern type_patterns[] =
{
	/* Sort by fields name! */
	DEF_PAT_CPUTYPE( "ASEs implemented",	PAT_FLAGS,	flags),		/* mips */
	DEF_PAT_CPUTYPE( "Address Sizes",	PAT_ADDRESS_SIZES,	addrsz),/* loongarch */
	DEF_PAT_CPUTYPE( "BogoMIPS",		PAT_BOGOMIPS,	bogomips),	/* aarch64 */
	DEF_PAT_CPUTYPE( "CPU Family",		PAT_FAMILY,	family),	/* loongarch */
	DEF_PAT_CPUTYPE( "CPU Revision",	PAT_REVISION,	revision),	/* loongarch */
	DEF_PAT_CPUTYPE( "CPU implementer",	PAT_IMPLEMENTER,vendor),	/* ARM and aarch64 */
	DEF_PAT_CPUTYPE( "CPU part",		PAT_PART,	model),		/* ARM and aarch64 */
	DEF_PAT_CPUTYPE( "CPU revision",	PAT_REVISION,	revision),	/* aarch64 */
	DEF_PAT_CPUTYPE( "CPU variant",		PAT_VARIANT,	stepping),	/* aarch64 */
	DEF_PAT_CPUTYPE( "Features",		PAT_FEATURES,	flags),		/* aarch64 */
	DEF_PAT_CPUTYPE( "ISA",			PAT_ISA,	isa),		/* loongarch */
	DEF_PAT_CPUTYPE( "Model Name",		PAT_MODEL_NAME,	modelname),	/* loongarch */
	DEF_PAT_CPUTYPE( "address sizes",	PAT_ADDRESS_SIZES,	addrsz),/* x86 */
	DEF_PAT_CPUTYPE( "bogomips per cpu",	PAT_BOGOMIPS,	bogomips),	/* s390 */
	DEF_PAT_CPUTYPE( "cpu",			PAT_CPU,	modelname),	/* ppc, sparc */
	DEF_PAT_CPUTYPE( "cpu family",		PAT_FAMILY,	family),
	DEF_PAT_CPUTYPE( "cpu model",		PAT_MODEL,	model),		/* mips */
	DEF_PAT_CPUTYPE( "family",		PAT_FAMILY,	family),
	DEF_PAT_CPUTYPE( "features",		PAT_FEATURES,	flags),		/* s390 */
	DEF_PAT_CPUTYPE( "flags",		PAT_FLAGS,	flags),		/* x86 */
	DEF_PAT_CPUTYPE( "marchid",		PAT_FAMILY,	family),	/* riscv */
	DEF_PAT_CPUTYPE( "max thread id",	PAT_MAX_THREAD_ID, mtid),	/* s390 */
	DEF_PAT_CPUTYPE( "mimpid",		PAT_MODEL,	model),		/* riscv */
	DEF_PAT_CPUTYPE( "model",		PAT_MODEL,	model),
	DEF_PAT_CPUTYPE( "model name",		PAT_MODEL_NAME,	modelname),
	DEF_PAT_CPUTYPE( "mvendorid",		PAT_VENDOR,	vendor),	/* riscv */
	DEF_PAT_CPUTYPE( "revision",		PAT_REVISION,	revision),
	DEF_PAT_CPUTYPE( "stepping",		PAT_STEPPING,	stepping),
	DEF_PAT_CPUTYPE( "type",		PAT_TYPE,	flags),		/* sparc64 */
	DEF_PAT_CPUTYPE( "uarch",		PAT_MODEL_NAME,	modelname),	/* riscv */
	DEF_PAT_CPUTYPE( "vendor",		PAT_VENDOR,	vendor),
	DEF_PAT_CPUTYPE( "vendor_id",		PAT_VENDOR,	vendor),	/* s390 */
};

/*
 * /proc/cpuinfo to lscpu_cpu conversion
 */
#define DEF_PAT_CPU(_str, _id, _member) \
	{ \
		.id = (_id), \
		.domain = CPUINFO_LINE_CPU, \
		.pattern = (_str), \
		.offset = offsetof(struct lscpu_cpu, _member), \
	}

static const struct cpuinfo_pattern cpu_patterns[] =
{
	/* Sort by fields name! */
	DEF_PAT_CPU( "CPU MHz",		PAT_MHZ,          mhz),		/* loongarch */
	DEF_PAT_CPU( "bogomips",	PAT_BOGOMIPS_CPU, bogomips),
	DEF_PAT_CPU( "cpu MHz",		PAT_MHZ,          mhz),
	DEF_PAT_CPU( "cpu MHz dynamic",	PAT_MHZ_DYNAMIC,  dynamic_mhz),	/* s390 */
	DEF_PAT_CPU( "cpu MHz static",	PAT_MHZ_STATIC,   static_mhz),	/* s390 */
	DEF_PAT_CPU( "cpu number",	PAT_PROCESSOR,    logical_id),	/* s390 */
	DEF_PAT_CPU( "processor",	PAT_PROCESSOR,    logical_id),

};

/*
 * /proc/cpuinfo to lscpu_cache conversion
 */
#define DEF_PAT_CACHE(_str, _id) \
	{ \
		.id = (_id), \
		.domain = CPUINFO_LINE_CACHE, \
		.pattern = (_str) \
	}

static const struct cpuinfo_pattern cache_patterns[] =
{
	/* Sort by fields name! */
	DEF_PAT_CACHE("cache",	PAT_CACHE),
};

#define CPUTYPE_PATTERN_BUFSZ	32

static int cmp_pattern(const void *a0, const void *b0)
{
	const struct cpuinfo_pattern
		*a = (const struct cpuinfo_pattern *) a0,
		*b = (const struct cpuinfo_pattern *) b0;
	return strcmp(a->pattern, b->pattern);
}

struct cpuinfo_parser {
	struct lscpu_cxt	*cxt;
	struct lscpu_cpu	*curr_cpu;
	struct lscpu_cputype	*curr_type;
	unsigned int		curr_type_added : 1;
};

static int is_different_cputype(struct lscpu_cputype *ct, size_t offset, const char *value)
{
	switch (offset) {
	case offsetof(struct lscpu_cputype, vendor):
		return ct->vendor && value && strcmp(ct->vendor, value) != 0;
	case offsetof(struct lscpu_cputype, model):
		return ct->model && value && strcmp(ct->model, value) != 0;
	case offsetof(struct lscpu_cputype, modelname):
		return ct->modelname && value && strcmp(ct->modelname, value) != 0;
	case offsetof(struct lscpu_cputype, stepping):
		return ct->stepping && value && strcmp(ct->stepping, value) != 0;
	}
	return 0;
}

/* canonicalize @str -- remove number at the end return the
 * number by @keynum. This is usable for example for "processor 5" or "cache1"
 * cpuinfo lines */
static char *key_cleanup(char *str, int *keynum)
{
	size_t sz = rtrim_whitespace((unsigned char *)str);
	size_t i;

	if (!sz)
		return str;

	for (i = sz; i > 0; i--) {
		if (!isdigit(str[i - 1]))
			break;
	}

	if (i < sz) {
		char *end = NULL, *p = str + i;
		int n;

		errno = 0;
		n = strtol(p, &end, 10);
		if (errno || !end || end == p)
			return str;

		*keynum = n;
		str[i] = '\0';
		rtrim_whitespace((unsigned char *)str);
	}
	return str;
}

static const struct cpuinfo_pattern *cpuinfo_parse_line(char *str, char **value, int *keynum)
{
	struct cpuinfo_pattern key = { .id = 0 }, *pat;
	char *p, *v;
	char buf[CPUTYPE_PATTERN_BUFSZ] = { 0 };

	DBG(GATHER, ul_debug("parse \"%s\"", str));

	if (!str || !*str)
		return NULL;
	p = (char *) skip_blank(str);
	if (!p || !*p)
		return NULL;

	v = strchr(p, ':');
	if (!v || !*v)
		return NULL;

	/* prepare name of the field */
	xstrncpy(buf, p, sizeof(buf));
	buf[v - p] = '\0';
	v++;

	/* prepare value */
	v = (char *) skip_space(v);
	if (!v || !*v)
		return NULL;

	key.pattern = key_cleanup(buf, keynum);
	/* CPU-type */
	if ((pat = bsearch(&key, type_patterns,
			ARRAY_SIZE(type_patterns),
			sizeof(struct cpuinfo_pattern),
			cmp_pattern)))
		goto found;

	/* CPU */
	if ((pat = bsearch(&key, cpu_patterns,
			ARRAY_SIZE(cpu_patterns),
			sizeof(struct cpuinfo_pattern),
			cmp_pattern)))
		goto found;

	/* CACHE */
	if ((pat = bsearch(&key, cache_patterns,
			ARRAY_SIZE(cache_patterns),
			sizeof(struct cpuinfo_pattern),
			cmp_pattern)))
		goto found;

	return NULL;
found:
	rtrim_whitespace((unsigned char *) v);
	*value = v;
	return pat;
}

/* Parse extra cache lines contained within /proc/cpuinfo but which are not
 * part of the cache topology information within the sysfs filesystem.  This is
 * true for all shared caches on e.g. s390. When there are layers of
 * hypervisors in between it is not knows which CPUs share which caches.
 * Therefore information about shared caches is only available in
 * /proc/cpuinfo. Format is:
 *
 *  cache<nr> : level=<lvl> type=<type> scope=<scope> size=<size> line_size=<lsz> associativity=<as>
 *
 * the cache<nr> part is parsed in cpuinfo_parse_line, in this function parses part after ":".
 */
static int cpuinfo_parse_cache(struct lscpu_cxt *cxt, int keynum, char *data)
{
	struct lscpu_cache *cache;
	long long size;
	char *p, type;
	int level;
	unsigned int line_size, associativity;

	DBG(GATHER, ul_debugobj(cxt, " parse cpuinfo cache '%s'", data));

	p = strstr(data, "scope=") + 6;
	/* Skip private caches, also present in sysfs */
	if (!p || strncmp(p, "Private", 7) == 0)
		return 0;
	p = strstr(data, "level=");
	if (!p || sscanf(p, "level=%d", &level) != 1)
		return 0;
	p = strstr(data, "type=") + 5;
	if (!p || !*p)
		return 0;
	type = 0;
	if (strncmp(p, "Data", 4) == 0)
		type = 'd';
	else if (strncmp(p, "Instruction", 11) == 0)
		type = 'i';
	else if (strncmp(p, "Unified", 7) == 0)
		type = 'u';
	p = strstr(data, "size=");
	if (!p || sscanf(p, "size=%lld", &size) != 1)
		return 0;

	p = strstr(data, "line_size=");
	if (!p || sscanf(p, "line_size=%u", &line_size) != 1)
		return 0;

	p = strstr(data, "associativity=");
	if (!p || sscanf(p, "associativity=%u", &associativity) != 1)
		return 0;

	cxt->necaches++;
	cxt->ecaches = xreallocarray(cxt->ecaches,
				     cxt->necaches, sizeof(struct lscpu_cache));
	cache = &cxt->ecaches[cxt->necaches - 1];
	memset(cache, 0 , sizeof(*cache));

	if (type == 'i' || type == 'd')
		xasprintf(&cache->name, "L%d%c", level, type);
	else
		xasprintf(&cache->name, "L%d", level);

	cache->nth = keynum;
	cache->level = level;
	cache->size = size * 1024;
	cache->ways_of_associativity = associativity;
	cache->coherency_line_size = line_size;
	/* Number of sets for s390. For safety, just check divide by zero */
	cache->number_of_sets = line_size ? (cache->size / line_size): 0;
	cache->number_of_sets = associativity ? (cache->number_of_sets / associativity) : 0;

	cache->type = type == 'i' ? xstrdup("Instruction") :
		      type == 'd' ? xstrdup("Data") :
		      type == 'u' ? xstrdup("Unified") : NULL;
	return 1;
}

int lscpu_read_cpuinfo(struct lscpu_cxt *cxt)
{
	FILE *fp;
	/* Used to be BUFSIZ which is small on some platforms e.g, musl,
	 * therefore hardcode to 4K */
	char buf[4096];
	size_t i;
	struct lscpu_cputype *ct;
	struct cpuinfo_parser _pr = { .cxt = cxt }, *pr = &_pr;

	assert(cxt->npossibles);	/* lscpu_create_cpus() required */
	assert(cxt->cpus);

	DBG(GATHER, ul_debugobj(cxt, "reading cpuinfo"));

	fp = ul_path_fopen(cxt->procfs, "r", "cpuinfo");
	if (!fp)
		err(EXIT_FAILURE, _("cannot open %s"), "/proc/cpuinfo");

	do {
		int keynum = -1;
		char *p = NULL, *value = NULL;
		const struct cpuinfo_pattern *pattern;

		if (fgets(buf, sizeof(buf), fp) != NULL)
			p = (char *) skip_space(buf);

		if (p == NULL || (*buf && !*p)) {
			/* Blank line separates information */
			if (p == NULL)
				break;	/* fgets() returns nothing; EOF */
			continue;
		}

		rtrim_whitespace((unsigned char *) buf);

		/* parse */
		pattern = cpuinfo_parse_line(p, &value, &keynum);
		if (!pattern) {
			DBG(GATHER, ul_debug("'%s' not found", buf));
			continue;
		}

		/* set data */
		switch (pattern->domain) {
		case CPUINFO_LINE_CPU:
			if (pattern->id == PAT_PROCESSOR) {
				/* switch CPU */
				int id = 0;

				if (keynum >= 0)
					id = keynum;
				else {
					uint32_t n;
					if (ul_strtou32(value, &n, 10) == 0)
						id = n;
				}

				if (pr->curr_cpu && pr->curr_type)
					lscpu_cpu_set_type(pr->curr_cpu, pr->curr_type);

				lscpu_unref_cpu(pr->curr_cpu);
				pr->curr_cpu = lscpu_get_cpu(cxt, id);

				if (!pr->curr_cpu)
					DBG(GATHER, ul_debug("*** cpu ID '%d' undefined", id));
				else
					DBG(GATHER, ul_debug(" switch to CPU %d", id));
				lscpu_ref_cpu(pr->curr_cpu);
				break;
			}
			if (!pr->curr_cpu)
				DBG(GATHER, ul_debug("*** cpu data before cpu ID"));
			else
				strdup_to_offset(pr->curr_cpu, pattern->offset, value);

			if (pattern->id == PAT_MHZ_DYNAMIC && pr->curr_type && !pr->curr_type->dynamic_mhz)
				pr->curr_type->dynamic_mhz = xstrdup(value);
			if (pattern->id == PAT_MHZ_STATIC && pr->curr_type && !pr->curr_type->static_mhz)
				pr->curr_type->static_mhz = xstrdup(value);
			if (pattern->id == PAT_BOGOMIPS_CPU && pr->curr_type && !pr->curr_type->bogomips)
				pr->curr_type->bogomips = xstrdup(value);
			if (pattern->id == PAT_MHZ && pr->curr_cpu && value) {
				errno = 0;
				pr->curr_cpu->mhz_cur_freq = (float) c_strtod(value, NULL);
				if (errno)
					pr->curr_cpu->mhz_cur_freq = 0;
			}
			break;
		case CPUINFO_LINE_CPUTYPE:
			if (pr->curr_type && is_different_cputype(pr->curr_type, pattern->offset, value)) {
				lscpu_unref_cputype(pr->curr_type);
				pr->curr_type = NULL;
			}
			if (!pr->curr_type) {
				pr->curr_type = lscpu_new_cputype();
				lscpu_add_cputype(cxt, pr->curr_type);
			}

			strdup_to_offset(pr->curr_type, pattern->offset, value);
			break;
		case CPUINFO_LINE_CACHE:
			if (pattern->id != PAT_CACHE)
				break;
			cpuinfo_parse_cache(cxt, keynum, value);
			break;
		}
	} while (1);

	DBG(GATHER, fprintf_cputypes(stderr, cxt));

	if (pr->curr_cpu && !pr->curr_cpu->type)
		lscpu_cpu_set_type(pr->curr_cpu, pr->curr_type);

	lscpu_unref_cputype(pr->curr_type);
	lscpu_unref_cpu(pr->curr_cpu);

	fclose(fp);
	lscpu_sort_caches(cxt->ecaches, cxt->necaches);

	/* Set the default type to CPUs which are missing (or not parsed)
	 * in cpuinfo */
	ct = lscpu_cputype_get_default(cxt);
	for (i = 0; ct && i < cxt->npossibles; i++) {
		struct lscpu_cpu *cpu = cxt->cpus[i];

		if (cpu && !cpu->type)
			lscpu_cpu_set_type(cpu, ct);
	}

	return 0;
}

struct lscpu_arch *lscpu_read_architecture(struct lscpu_cxt *cxt)
{
	struct utsname utsbuf;
	struct lscpu_arch *ar;
	struct lscpu_cputype *ct;

	assert(cxt);

	DBG(GATHER, ul_debug("reading architecture"));

	if (uname(&utsbuf) == -1)
		err(EXIT_FAILURE, _("error: uname failed"));

	ar = xcalloc(1, sizeof(*cxt->arch));
	ar->name = xstrdup(utsbuf.machine);

	if (cxt->noalive)
		/* reading info from any /{sys,proc} dump, don't mix it with
		 * information about our real CPU */
		;
	else {
#if defined(__alpha__) || defined(__ia64__)
		ar->bit64 = 1;	/* 64bit platforms only */
#endif
		/* platforms with 64bit flag in /proc/cpuinfo, define
		 * 32bit default here */
#if defined(__i386__) || defined(__x86_64__) || \
    defined(__s390x__) || defined(__s390__) || defined(__sparc_v9__)
		ar->bit32 = 1;
#endif

#if defined(__aarch64__)
		{
			/* personality() is the most reliable way (since 4.7)
			 * to determine aarch32 support */
			int pers = personality(PER_LINUX32);
			if (pers != -1) {
				personality(pers);
				ar->bit32 = 1;
			}
			ar->bit64 = 1;
		}
#endif
	}

	ct = lscpu_cputype_get_default(cxt);
	if (ct && ct->flags) {
		char buf[BUFSIZ];

		snprintf(buf, sizeof(buf), " %s ", ct->flags);
		if (strstr(buf, " lm "))
			ar->bit32 = ar->bit64 = 1;			/* x86_64 */
		if (strstr(buf, " zarch "))
			ar->bit32 = ar->bit64 = 1;			/* s390x */
		if (strstr(buf, " sun4v ") || strstr(buf, " sun4u "))
			ar->bit32 = ar->bit64 = 1;			/* sparc64 */
	}

	if (ct && ct->isa) {
		char buf[BUFSIZ];

		snprintf(buf, sizeof(buf), " %s ", ct->isa);
		if (strstr(buf, " loongarch32 "))
			ar->bit32 = 1;
		if (strstr(buf, " loongarch64 "))
			ar->bit64 = 1;
	}

	if (ar->name && !cxt->noalive) {
		if (strcmp(ar->name, "ppc64") == 0)
			ar->bit32 = 1, ar->bit64 = 1;
		else if (strcmp(ar->name, "ppc") == 0)
			ar->bit32 = 1;
	}

	DBG(GATHER, ul_debugobj(ar, "arch: name=%s %s %s",
				ar->name,
				ar->bit64 ? "64-bit" : "",
				ar->bit64 ? "32-bit" : ""));
	return ar;
}

void lscpu_free_architecture(struct lscpu_arch *ar)
{
	if (!ar)
		return;
	free(ar->name);
	free(ar);
}

int lscpu_read_cpulists(struct lscpu_cxt *cxt)
{
	cpu_set_t *cpuset = NULL;

	assert(cxt);
	DBG(GATHER, ul_debugobj(cxt, "reading cpulists"));

	if (ul_path_read_s32(cxt->syscpu, &cxt->maxcpus, "kernel_max") == 0)
		/* note that kernel_max is maximum index [NR_CPUS-1] */
		cxt->maxcpus += 1;

	else if (!cxt->noalive)
		/* the root is '/' so we are working with data from the current kernel */
		cxt->maxcpus = get_max_number_of_cpus();

	if (cxt->maxcpus <= 0)
		/* error or we are reading some /sys snapshot instead of the
		 * real /sys, let's use any crazy number... */
		cxt->maxcpus = 2048;

	cxt->setsize = CPU_ALLOC_SIZE(cxt->maxcpus);

	/* create CPUs from possible mask */
	if (ul_path_readf_cpulist(cxt->syscpu, &cpuset, cxt->maxcpus, "possible") == 0) {
		lscpu_create_cpus(cxt, cpuset, cxt->setsize);
		cpuset_free(cpuset);
		cpuset = NULL;
	} else
		err(EXIT_FAILURE, _("failed to determine number of CPUs: %s"),
				_PATH_SYS_CPU "/possible");


	/* get mask for present CPUs */
	if (ul_path_readf_cpulist(cxt->syscpu, &cxt->present, cxt->maxcpus, "present") == 0)
		cxt->npresents = CPU_COUNT_S(cxt->setsize, cxt->present);

	/* get mask for online CPUs */
	if (ul_path_readf_cpulist(cxt->syscpu, &cxt->online, cxt->maxcpus, "online") == 0)
		cxt->nonlines = CPU_COUNT_S(cxt->setsize, cxt->online);

	return 0;
}

#if defined(HAVE_LIBRTAS)
# define PROCESSOR_MODULE_INFO	43
static int strbe16toh(const char *buf, int offset)
{
	return (buf[offset] << 8) + buf[offset+1];
}
#endif

/* some extra information for the default CPU type */
int lscpu_read_archext(struct lscpu_cxt *cxt)
{
	FILE *f;
	char buf[BUFSIZ];
	struct lscpu_cputype *ct;

	DBG(GATHER, ul_debugobj(cxt, "reading extra arch info"));

	assert(cxt);
	ct = lscpu_cputype_get_default(cxt);
	if (!ct)
		return -EINVAL;

	/* get dispatching mode */
	if (ul_path_read_s32(cxt->syscpu, &ct->dispatching, "dispatching") != 0)
		ct->dispatching = -1;

	/* get cpufreq boost mode */
	if (ul_path_read_s32(cxt->syscpu, &ct->freqboost, "cpufreq/boost") != 0)
		ct->freqboost = -1;

	if ((f = ul_path_fopen(cxt->procfs, "r", "sysinfo"))) {
		while (fgets(buf, sizeof(buf), f) != NULL) {
			if (lookup(buf, "Type", &ct->machinetype))
				break;
		}
		fclose(f);
	}

#if defined(HAVE_LIBRTAS)
	/* Get PowerPC specific info */
	if (!cxt->noalive) {
		int rc, len, ntypes;

		ct->physsockets = ct->physchips = ct->physcoresperchip = 0;

		rc = rtas_get_sysparm(PROCESSOR_MODULE_INFO, sizeof(buf), buf);
		if (rc < 0)
			goto nortas;

		len = strbe16toh(buf, 0);
		if (len < 8)
			goto nortas;

		ntypes = strbe16toh(buf, 2);
		if (!ntypes)
			goto nortas;

		ct->physsockets = strbe16toh(buf, 4);
		ct->physchips = strbe16toh(buf, 6);
		ct->physcoresperchip = strbe16toh(buf, 8);
	}
nortas:
#endif
	return 0;
}

static int cmp_vulnerability_name(const void *a0, const void *b0)
{
	const struct lscpu_vulnerability
			*a = (const struct lscpu_vulnerability *) a0,
			*b = (const struct lscpu_vulnerability *) b0;
	return strcmp(a->name, b->name);
}

int lscpu_read_vulnerabilities(struct lscpu_cxt *cxt)
{
	struct dirent *d;
	DIR *dir;
	size_t n = 0;

	assert(cxt);

	DBG(GATHER, ul_debugobj(cxt, "reading vulnerabilities"));

	dir = ul_path_opendir(cxt->syscpu, "vulnerabilities");
	if (!dir)
		return 0;

	cxt->nvuls = n = 0;
	while (xreaddir(dir))
		n++;
	if (!n) {
		closedir(dir);
		return 0;
	}

	rewinddir(dir);
	cxt->vuls = xcalloc(n, sizeof(struct lscpu_vulnerability));

	while (cxt->nvuls < n && (d = xreaddir(dir))) {
		char *str, *p;
		struct lscpu_vulnerability *vu;

#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type == DT_DIR || d->d_type == DT_UNKNOWN)
			continue;
#endif
		if (ul_path_readf_string(cxt->syscpu, &str,
					"vulnerabilities/%s", d->d_name) <= 0)
			continue;

		vu = &cxt->vuls[cxt->nvuls++];

		/* Name */
		vu->name = xstrdup(d->d_name);
		*vu->name = toupper(*vu->name);
		strrep(vu->name, '_', ' ');

		/* Description */
		vu->text = str;
		p = (char *) startswith(vu->text, "Mitigation");
		if (p) {
			*p = ';';
			strrem(vu->text, ':');
		}
	}
	closedir(dir);

	qsort(cxt->vuls, cxt->nvuls,
	      sizeof(struct lscpu_vulnerability), cmp_vulnerability_name);

	return 0;
}

static inline int is_node_dirent(struct dirent *d)
{
	return
		d &&
#ifdef _DIRENT_HAVE_D_TYPE
		(d->d_type == DT_DIR || d->d_type == DT_UNKNOWN) &&
#endif
		strncmp(d->d_name, "node", 4) == 0 &&
		isdigit_string(d->d_name + 4);
}

static int nodecmp(const void *ap, const void *bp)
{
	int *a = (int *) ap, *b = (int *) bp;
	return *a - *b;
}

int lscpu_read_numas(struct lscpu_cxt *cxt)
{
	size_t i = 0;
	DIR *dir;
	struct dirent *d;
	struct path_cxt *sys;

	assert(!cxt->nnodes);


	sys = ul_new_path(_PATH_SYS_NODE);
	if (!sys)
		err(EXIT_FAILURE, _("failed to initialize %s handler"), _PATH_SYS_NODE);

	ul_path_set_prefix(sys, cxt->prefix);

	dir = ul_path_opendir(sys, NULL);
	if (!dir)
		goto done;

	while ((d = readdir(dir))) {
		if (is_node_dirent(d))
			cxt->nnodes++;
	}

	if (!cxt->nnodes) {
		closedir(dir);
		goto done;
	}

	cxt->nodemaps = xcalloc(cxt->nnodes, sizeof(cpu_set_t *));
	cxt->idx2nodenum = xmalloc(cxt->nnodes * sizeof(int));

	rewinddir(dir);
	for (i = 0; (d = readdir(dir)) && i < cxt->nnodes;) {
		if (is_node_dirent(d))
			cxt->idx2nodenum[i++] = strtol_or_err(((d->d_name) + 4),
						_("Failed to extract the node number"));
	}
	closedir(dir);
	qsort(cxt->idx2nodenum, cxt->nnodes, sizeof(int), nodecmp);

	/* information about how nodes share different CPUs */
	for (i = 0; i < cxt->nnodes; i++)
		ul_path_readf_cpuset(sys, &cxt->nodemaps[i], cxt->maxcpus,
				"node%d/cpumap", cxt->idx2nodenum[i]);
done:
	DBG(GATHER, ul_debugobj(cxt, "read %zu numas", cxt->nnodes));

	ul_unref_path(sys);
	return 0;
}
