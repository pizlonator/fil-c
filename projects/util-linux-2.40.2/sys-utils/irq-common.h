/*
 * SPDX-License-Identifier: GPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_H_IRQ_COMMON
#define UTIL_LINUX_H_IRQ_COMMON

#include "c.h"
#include "nls.h"
#include "cpuset.h"

/* supported columns */
enum {
	COL_IRQ = 0,
	COL_TOTAL,
	COL_DELTA,
	COL_NAME,

	__COL_COUNT
};

struct irq_info {
	char *irq;			/* short name of this irq */
	char *name;			/* descriptive name of this irq */
	unsigned long total;		/* total count since system start up */
	unsigned long delta;		/* delta count since previous update */
};

struct irq_cpu {
	unsigned long total;
	unsigned long delta;
};

struct irq_stat {
	unsigned long nr_irq;		/* number of irq vector */
	unsigned long nr_irq_info;	/* number of irq info */
	struct irq_info *irq_info;	/* array of irq_info */
	struct irq_cpu *cpus;		 /* array of irq_cpu */
	size_t nr_active_cpu;		/* number of active cpu */
	unsigned long total_irq;	/* total irqs */
	unsigned long delta_irq;	/* delta irqs */
};


typedef int (irq_cmp_t)(const struct irq_info *, const struct irq_info *);

/* output definition */
struct irq_output {
	int columns[__COL_COUNT * 2];
	size_t ncolumns;

	irq_cmp_t *sort_cmp_func;

	unsigned int
		json:1,		/* JSON output */
		pairs:1,	/* export, NAME="value" aoutput */
		no_headings:1;	/* don't print header */
};

int irq_column_name_to_id(char const *const name, size_t const namesz);
void free_irqstat(struct irq_stat *stat);

void irq_print_columns(FILE *f, int nodelta);

void set_sort_func_by_name(struct irq_output *out, const char *name);
void set_sort_func_by_key(struct irq_output *out, const char c);

struct libscols_table *get_scols_table(struct irq_output *out,
                                              struct irq_stat *prev,
                                              struct irq_stat **xstat,
                                              int softirq,
                                              size_t setsize,
                                              cpu_set_t *cpuset);

struct libscols_table *get_scols_cpus_table(struct irq_output *out,
                                        struct irq_stat *prev,
                                        struct irq_stat *curr,
                                        size_t setsize,
                                        cpu_set_t *cpuset);

#endif /* UTIL_LINUX_H_IRQ_COMMON */
