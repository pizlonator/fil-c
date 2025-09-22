/*
 * column-list-table.h - helper functions for implementing -H/--list-columns option
 *
 * Copyright (C) 2023 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef UTIL_LINUX_COLUMN_LIST_TABLE_H
#define UTIL_LINUX_COLUMN_LIST_TABLE_H

#include "libsmartcols.h"
#include <stdio.h>

enum { CLT_COL_HOLDER, CLT_COL_TYPE, CLT_COL_DESC };

static inline struct libscols_table *xcolumn_list_table_new(const char *table_name,
							    FILE *out,
							    int raw,
							    int json)
{
	struct clt_colinfo {
		const char *name;
		int flags;
	};
	struct libscols_table *tb;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_set_name(tb, table_name);
	scols_table_set_stream(tb, out);
	scols_table_enable_noheadings(tb, 1);
	scols_table_enable_raw(tb, raw);
	scols_table_enable_json(tb, json);

	if (!scols_table_new_column(tb, "HOLDER", 0, SCOLS_FL_RIGHT))
		goto failed;
	if (!scols_table_new_column(tb, "TYPE", 0, 0))
		goto failed;
	if (!scols_table_new_column(tb, "DESCRIPTION", 0, 0))
		goto failed;
	return tb;

 failed:
	err(EXIT_FAILURE, _("failed to allocate output column"));
}

static inline void xcolumn_list_table_append_line(struct libscols_table *tb,
						  const char *name,
						  int json_type, const char *fallback_typename,
						  const char *desc)
{
	struct libscols_line *ln = scols_table_new_line(tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	if (scols_line_set_data(ln, CLT_COL_HOLDER, name))
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_set_data(ln, CLT_COL_TYPE, (json_type == SCOLS_JSON_STRING ?        "<string>":
						   json_type == SCOLS_JSON_ARRAY_STRING ?  "<string>":
						   json_type == SCOLS_JSON_ARRAY_NUMBER ?  "<string>":
						   json_type == SCOLS_JSON_NUMBER ?        "<integer>":
						   json_type == SCOLS_JSON_FLOAT ?         "<float>":
						   json_type == SCOLS_JSON_BOOLEAN ?       "<boolean>":
						   fallback_typename ?: "<string>")))
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_set_data(ln, CLT_COL_DESC, desc))
		err(EXIT_FAILURE, _("failed to add output data"));
}

#endif
