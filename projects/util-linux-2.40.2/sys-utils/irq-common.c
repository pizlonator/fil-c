/*
 * SPDX-License-Identifier: GPL-2.1-or-later
 *
 * irq-common.c - functions to display kernel interrupt information.
 *
 * Copyright (C) 2019 zhenwei pi <pizhenwei@bytedance.com>
 * Copyright (C) 2020 Karel Zak <kzak@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <libsmartcols.h>

#include "c.h"
#include "nls.h"
#include "pathnames.h"
#include "strutils.h"
#include "xalloc.h"

#include "irq-common.h"

#define IRQ_INFO_LEN	64

struct colinfo {
	const char *name;
	double whint;
	int flags;
	const char *help;
	int json_type;
};

static const struct colinfo infos[] = {
	[COL_IRQ]   = {"IRQ",   0.10, SCOLS_FL_RIGHT, N_("interrupts"),  SCOLS_JSON_STRING},
	[COL_TOTAL] = {"TOTAL", 0.10, SCOLS_FL_RIGHT, N_("total count"), SCOLS_JSON_NUMBER},
	[COL_DELTA] = {"DELTA", 0.10, SCOLS_FL_RIGHT, N_("delta count"), SCOLS_JSON_NUMBER},
	[COL_NAME]  = {"NAME",  0.70, SCOLS_FL_TRUNC, N_("name"),        SCOLS_JSON_STRING},
};

/* make softirq friendly to end-user */
struct softirq_desc {
	char *irq;
	char *desc;
} softirq_descs[] = {
	{ .irq = "HI", .desc = "high priority tasklet softirq" },
	{ .irq = "TIMER", .desc = "timer softirq" },
	{ .irq = "NET_TX", .desc = "network transmit softirq", },
	{ .irq = "NET_RX", .desc = "network receive softirq" },
	{ .irq = "BLOCK", .desc = "block device softirq" },
	{ .irq = "IRQ_POLL", .desc = "IO poll softirq" },
	{ .irq = "TASKLET", .desc = "normal priority tasklet softirq" },
	{ .irq = "SCHED", .desc = "schedule softirq" },
	{ .irq = "HRTIMER", .desc = "high resolution timer softirq" },
	{ .irq = "RCU", .desc = "RCU softirq" },
};

static void get_softirq_desc(struct irq_info *curr)
{
	int i, size = ARRAY_SIZE(softirq_descs);

	for (i = 0; i < size; i++) {
		if (!strcmp(curr->irq, softirq_descs[i].irq))
			break;
	}

	if (i < size)
		curr->name = xstrdup(softirq_descs[i].desc);
	else
		curr->name = xstrdup("");
}

int irq_column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	assert(name);
	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static inline int get_column_id(struct irq_output *out, size_t const num)
{
	assert(num < out->ncolumns);
	assert(out->columns[num] < (int)ARRAY_SIZE(infos));

	return out->columns[num];
}

static inline const struct colinfo *get_column_info(
			struct irq_output *out, unsigned num)
{
	return &infos[get_column_id(out, num)];
}

void irq_print_columns(FILE *f, int nodelta)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		if (nodelta && i == COL_DELTA)
			continue;
		fprintf(f, "  %-5s  %s\n", infos[i].name, _(infos[i].help));
	}
}

static struct libscols_table *new_scols_table(struct irq_output *out)
{
	size_t i;
	struct libscols_table *table;

	table = scols_new_table();
	if (!table) {
		warn(_("failed to initialize output table"));
		return NULL;
	}
	scols_table_enable_json(table, out->json);
	scols_table_enable_noheadings(table, out->no_headings);
	scols_table_enable_export(table, out->pairs);

	if (out->json)
		scols_table_set_name(table, "interrupts");

	for (i = 0; i < out->ncolumns; i++) {
		const struct colinfo *col = get_column_info(out, i);
		int flags = col->flags;
		struct libscols_column *cl;

		cl = scols_table_new_column(table, col->name, col->whint, flags);
		if (cl == NULL) {
			warnx(_("failed to initialize output column"));
			goto err;
		}
		if (out->json)
			scols_column_set_json_type(cl, col->json_type);
	}

	return table;
 err:
	scols_unref_table(table);
	return NULL;
}

static struct libscols_line *new_scols_line(struct libscols_table *table)
{
	struct libscols_line *line = scols_table_new_line(table, NULL);
	if (!line) {
		warn(_("failed to add line to output"));
		return NULL;
	}
	return line;
}

static void add_scols_line(struct irq_output *out,
			   struct irq_info *info,
			   struct libscols_table *table)
{
	size_t i;
	struct libscols_line *line = new_scols_line(table);

	for (i = 0; i < out->ncolumns; i++) {
		char *str = NULL;

		switch (get_column_id(out, i)) {
		case COL_IRQ:
			xasprintf(&str, "%s", info->irq);
			break;
		case COL_TOTAL:
			xasprintf(&str, "%ld", info->total);
			break;
		case COL_DELTA:
			xasprintf(&str, "%ld", info->delta);
			break;
		case COL_NAME:
			xasprintf(&str, "%s", info->name);
			break;
		default:
			break;
		}

		if (str && scols_line_refer_data(line, i, str) != 0)
			err_oom();
	}
}

static char *remove_repeated_spaces(char *str)
{
	char *inp = str, *outp = str;
	uint8_t prev_space = 0;

	while (*inp) {
		if (isspace(*inp)) {
			if (!prev_space) {
				*outp++ = ' ';
				prev_space = 1;
			}
		} else {
			*outp++ = *inp;
			prev_space = 0;
		}
		++inp;
	}
	*outp = '\0';
	return str;
}

static bool cpu_in_list(int cpu, size_t setsize, cpu_set_t *cpuset)
{
	/* no -C/--cpu-list specified, use all the CPUs */
	if (!cpuset)
		return true;

	return CPU_ISSET_S(cpu, setsize, cpuset);
}

/*
 * irqinfo - parse the system's interrupts
 */
static struct irq_stat *get_irqinfo(int softirq, size_t setsize, cpu_set_t *cpuset)
{
	FILE *irqfile;
	char *line = NULL, *tmp;
	size_t len = 0;
	struct irq_stat *stat;
	struct irq_info *curr;

	/* NAME + ':' + 11 bytes/cpu + IRQ_NAME_LEN */
	stat = xcalloc(1, sizeof(*stat));

	stat->irq_info = xmalloc(sizeof(*stat->irq_info) * IRQ_INFO_LEN);
	stat->nr_irq_info = IRQ_INFO_LEN;

	if (softirq)
		irqfile = fopen(_PATH_PROC_SOFTIRQS, "r");
	else
		irqfile = fopen(_PATH_PROC_INTERRUPTS, "r");
	if (!irqfile) {
		warn(_("cannot open %s"), _PATH_PROC_INTERRUPTS);
		goto free_stat;
	}

	/* read header firstly */
	if (getline(&line, &len, irqfile) < 0) {
		warn(_("cannot read %s"), _PATH_PROC_INTERRUPTS);
		goto close_file;
	}

	tmp = line;
	while ((tmp = strstr(tmp, "CPU")) != NULL) {
		tmp += 3;	/* skip this "CPU", find next */
		stat->nr_active_cpu++;
	}

	stat->cpus =  xcalloc(stat->nr_active_cpu, sizeof(struct irq_cpu));

	/* parse each line of _PATH_PROC_INTERRUPTS */
	while (getline(&line, &len, irqfile) >= 0) {
		unsigned long count;
		size_t index;
		int length;

		tmp = strchr(line, ':');
		if (!tmp)
			continue;

		length = strlen(line);

		curr = stat->irq_info + stat->nr_irq++;
		memset(curr, 0, sizeof(*curr));
		*tmp = '\0';
		curr->irq = xstrdup(line);
		ltrim_whitespace((unsigned char *)curr->irq);

		tmp += 1;
		for (index = 0; (index < stat->nr_active_cpu) && (tmp - line < length); index++) {
			struct irq_cpu *cpu = &stat->cpus[index];

			if (sscanf(tmp, " %10lu", &count) != 1)
				continue;
			if (cpu_in_list(index, setsize, cpuset)) {
				curr->total += count;
				cpu->total += count;
				stat->total_irq += count;
			}

			tmp += 11;
		}

		/* softirq always has no desc, add additional desc for softirq */
		if (softirq)
			get_softirq_desc(curr);
		else {
			if (tmp - line < length) {
				/* strip all space before desc */
				while (isspace(*tmp))
					tmp++;
				tmp = remove_repeated_spaces(tmp);
				rtrim_whitespace((unsigned char *)tmp);
				curr->name = xstrdup(tmp);
			} else /* no irq name string, we have to set '\0' here */
				curr->name = xstrdup("");
		}

		if (stat->nr_irq == stat->nr_irq_info) {
			stat->nr_irq_info *= 2;
			stat->irq_info = xreallocarray(stat->irq_info, stat->nr_irq_info,
						       sizeof(*stat->irq_info));
		}
	}
	fclose(irqfile);
	free(line);
	return stat;

 close_file:
	fclose(irqfile);
 free_stat:
	free(stat->irq_info);
	free(stat->cpus);
	free(stat);
	free(line);
	return NULL;
}

void free_irqstat(struct irq_stat *stat)
{
	size_t i;

	if (!stat)
		return;

	for (i = 0; i < stat->nr_irq; i++) {
		free(stat->irq_info[i].name);
		free(stat->irq_info[i].irq);
	}

	free(stat->irq_info);
	free(stat->cpus);
	free(stat);
}

static inline int cmp_name(const struct irq_info *a,
		     const struct irq_info *b)
{
	return strcoll(a->name, b->name);
}

static inline int cmp_ulong_descending(unsigned long a,
		      unsigned long b)
{
	if (a == b)
		return 0;
	if (a < b)
		return 1;
	else
		return -1;
}

static inline int cmp_total(const struct irq_info *a,
		      const struct irq_info *b)
{
	int cmp = cmp_ulong_descending(a->total, b->total);
	return cmp ? cmp : cmp_name(a, b);
}

static inline int cmp_delta(const struct irq_info *a,
		      const struct irq_info *b)
{
	int cmp = cmp_ulong_descending(a->delta, b->delta);
	return cmp ? cmp : cmp_name(a, b);
}

static inline int cmp_interrupts(const struct irq_info *a,
			   const struct irq_info *b)
{
	return strverscmp(a->irq, b->irq);
}

static void sort_result(struct irq_output *out,
			struct irq_info *result,
			size_t nmemb)
{
	irq_cmp_t *func = cmp_total;	/* default */

	if (out->sort_cmp_func)
		func = out->sort_cmp_func;

	qsort(result, nmemb, sizeof(*result),
			(int (*)(const void *, const void *)) func);
}

void set_sort_func_by_name(struct irq_output *out, const char *name)
{
	if (strcasecmp(name, "IRQ") == 0)
		out->sort_cmp_func = cmp_interrupts;
	else if (strcasecmp(name, "TOTAL") == 0)
		out->sort_cmp_func = cmp_total;
	else if (strcasecmp(name, "DELTA") == 0)
		out->sort_cmp_func = cmp_delta;
	else if (strcasecmp(name, "NAME") == 0)
		out->sort_cmp_func = cmp_name;
	else
		errx(EXIT_FAILURE, _("unsupported column name to sort output"));
}

void set_sort_func_by_key(struct irq_output *out, char c)
{
	switch (c) {
	case 'i':
		out->sort_cmp_func = cmp_interrupts;
		break;
	case 't':
		out->sort_cmp_func = cmp_total;
		break;
	case 'd':
		out->sort_cmp_func = cmp_delta;
		break;
	case 'n':
		out->sort_cmp_func = cmp_name;
		break;
	}
}

struct libscols_table *get_scols_cpus_table(struct irq_output *out,
					struct irq_stat *prev,
					struct irq_stat *curr,
					size_t setsize,
					cpu_set_t *cpuset)
{
	struct libscols_table *table;
	struct libscols_column *cl;
	struct libscols_line *ln;
	char colname[sizeof("cpu") + sizeof(stringify_value(LONG_MAX))];
	size_t i, j;

	if (prev) {
		for (i = 0; i < curr->nr_active_cpu; i++) {
			struct irq_cpu *pre = &prev->cpus[i];
			struct irq_cpu *cur = &curr->cpus[i];

			cur->delta = cur->total - pre->total;
		}
	}

	table = scols_new_table();
	if (!table) {
		warn(_("failed to initialize output table"));
		return NULL;
	}
	scols_table_enable_json(table, out->json);
	scols_table_enable_noheadings(table, out->no_headings);
	scols_table_enable_export(table, out->pairs);

	if (out->json)
		scols_table_set_name(table, _("cpu-interrupts"));
	else
		scols_table_new_column(table, "", 0, SCOLS_FL_RIGHT);

	for (i = 0; i < curr->nr_active_cpu; i++) {
		if (!cpu_in_list(i, setsize, cpuset))
			continue;
		snprintf(colname, sizeof(colname), "cpu%zu", i);
		cl = scols_table_new_column(table, colname, 0, SCOLS_FL_RIGHT);
		if (cl == NULL) {
			warnx(_("failed to initialize output column"));
			goto err;
		}
		if (out->json)
			scols_column_set_json_type(cl, SCOLS_JSON_STRING);
	}

	/* per cpu % of total */
	ln = new_scols_line(table);
	if (!ln || (!out->json && scols_line_set_data(ln, 0, "%irq:") != 0))
		goto err;

	for (i = 0, j = 0; i < curr->nr_active_cpu; i++) {
		struct irq_cpu *cpu = &curr->cpus[i];
		char *str;

		if (!cpu_in_list(i, setsize, cpuset))
			continue;
		xasprintf(&str, "%0.1f", (double)((long double) cpu->total / (long double) curr->total_irq * 100.0));
		if (str && scols_line_refer_data(ln, ++j, str) != 0)
			goto err;
	}

	/* per cpu % of delta */
	ln = new_scols_line(table);
	/* xgettext:no-c-format */
	if (!ln || (!out->json && scols_line_set_data(ln, 0, _("%delta:")) != 0))
		goto err;

	for (i = 0, j = 0; i < curr->nr_active_cpu; i++) {
		struct irq_cpu *cpu = &curr->cpus[i];
		char *str;

		if (!cpu_in_list(i, setsize, cpuset))
			continue;
		if (!curr->delta_irq)
			continue;
		xasprintf(&str, "%0.1f", (double)((long double) cpu->delta / (long double) curr->delta_irq * 100.0));
		if (str && scols_line_refer_data(ln, ++j, str) != 0)
			goto err;
	}

	return table;
 err:
	scols_unref_table(table);
	return NULL;
}

struct libscols_table *get_scols_table(struct irq_output *out,
					      struct irq_stat *prev,
					      struct irq_stat **xstat,
					      int softirq,
					      size_t setsize,
					      cpu_set_t *cpuset)
{
	struct libscols_table *table;
	struct irq_info *result;
	struct irq_stat *stat;
	size_t size;
	size_t i;

	/* the stats */
	stat = get_irqinfo(softirq, setsize, cpuset);
	if (!stat)
		return NULL;

	size = sizeof(*stat->irq_info) * stat->nr_irq;
	result = xmalloc(size);
	memcpy(result, stat->irq_info, size);

	if (prev) {
		stat->delta_irq = 0;
		for (i = 0; i < stat->nr_irq; i++) {
			struct irq_info *cur = &result[i];
			struct irq_info *pre = &prev->irq_info[i];

			cur->delta = cur->total - pre->total;
			stat->delta_irq += cur->delta;
		}
	}
	sort_result(out, result, stat->nr_irq);

	table = new_scols_table(out);
	if (!table) {
		free(result);
		free_irqstat(stat);
		return NULL;
	}

	for (i = 0; i < stat->nr_irq; i++)
		add_scols_line(out, &result[i], table);

	free(result);

	if (xstat)
		*xstat = stat;
	else
		free_irqstat(stat);

	return table;
}
