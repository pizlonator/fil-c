/*
 * filter.c - functions for lines filtering
 *
 * Copyright (C) 2023 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: filter
 * @title: Filters and counters
 * @short_description: defines lines filter and counter
 *
 * An API to define and use filter and counters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smartcolsP.h"

#include "filter-parser.h"
#include "filter-scanner.h"

/**
 * scols_new_filter:
 * @str: filter expression or NULL
 *
 * Allocated and optionally parses a new filter.
 *
 * Returns: new filter instance or NULL in case of error.
 *
 * Since: 2.40
 */
struct libscols_filter *scols_new_filter(const char *str)
{
	struct libscols_filter *fltr = calloc(1, sizeof(*fltr));

	if (!fltr)
		return NULL;

	DBG(FLTR, ul_debugobj(fltr, "alloc"));
	fltr->refcount = 1;
	INIT_LIST_HEAD(&fltr->params);
	INIT_LIST_HEAD(&fltr->counters);

	if (str && scols_filter_parse_string(fltr, str) != 0) {
		scols_unref_filter(fltr);
		return NULL;
	}

	return fltr;
}

/**
 * scols_ref_filter:
 * @fltr: filter instance
 *
 * Increment filter reference counter.
 *
 * Since: 2.40
 */
void scols_ref_filter(struct libscols_filter *fltr)
{
	if (fltr)
		fltr->refcount++;
}

static void reset_filter(struct libscols_filter *fltr)
{
	if (!fltr)
		return;
	filter_unref_node(fltr->root);
	fltr->root = NULL;

	if (fltr->src)
		fclose(fltr->src);
	fltr->src = NULL;

	free(fltr->errmsg);
	fltr->errmsg = NULL;
}

static void remove_counters(struct libscols_filter *fltr)
{
	if (!fltr)
		return;

	DBG(FLTR, ul_debugobj(fltr, "remove all counters"));
	while (!list_empty(&fltr->counters)) {
		struct libscols_counter *ct = list_entry(fltr->counters.next,
				struct libscols_counter, counters);

		filter_unref_node((struct filter_node *) ct->param);
		list_del_init(&ct->counters);
		free(ct->name);
		free(ct);
	}
}

/**
 * scols_unref_filter:
 * @fltr: filter instance
 *
 * Deincrements reference counter, unallocates the filter for the last
 * reference.
 *
 * Since: 2.40
 */
void scols_unref_filter(struct libscols_filter *fltr)
{
	if (fltr && --fltr->refcount <= 0) {
		DBG(FLTR, ul_debugobj(fltr, "dealloc"));
		reset_filter(fltr);
		remove_counters(fltr);
		free(fltr);
	}
}

/* This is generic allocater for a new node, always use the node type specific
 * functions (e.g. filter_new_param() */
struct filter_node *__filter_new_node(enum filter_ntype type, size_t sz)
{
	struct filter_node *n = calloc(1, sz);

	if (!n)
		return NULL;

	n->type = type;
	n->refcount = 1;
	return n;
}

void filter_unref_node(struct filter_node *n)
{
	if (!n || --n->refcount > 0)
		return;

	switch (n->type) {
	case F_NODE_EXPR:
		filter_free_expr((struct filter_expr *) n);
		break;
	case F_NODE_PARAM:
		filter_free_param((struct filter_param *) n);
		break;
	}
}

void filter_ref_node(struct filter_node *n)
{
	if (n)
		n->refcount++;
}

void filter_dump_node(struct ul_jsonwrt *json, struct filter_node *n)
{
	if (!n)
		return;

	switch (n->type) {
	case F_NODE_EXPR:
		filter_dump_expr(json, (struct filter_expr *) n);
		break;
	case F_NODE_PARAM:
		filter_dump_param(json, (struct filter_param *) n);
		break;
	}
}

/**
 * scols_filter_parse_string:
 * @fltr: filter instance
 * @str: string with filter expression
 *
 * Parses filter, see scols_filter_get_errmsg() for errors.
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_filter_parse_string(struct libscols_filter *fltr, const char *str)
{
	yyscan_t sc;
	int rc;

	reset_filter(fltr);

	if (!str || !*str)
		return 0;	/* empty filter is not error */

	fltr->src = fmemopen((void *) str, strlen(str), "r");
	if (!fltr->src)
		return -errno;

	yylex_init(&sc);
	yyset_in(fltr->src, sc);

	rc = yyparse(sc, fltr);
	yylex_destroy(sc);

	fclose(fltr->src);
	fltr->src = NULL;

	ON_DBG(FLTR, scols_dump_filter(fltr, stderr));

	return rc;
}

/**
 * scols_dump_filter:
 * @fltr: filter instance
 * @out: output stream
 *
 * Dumps internal filter nodes in JSON format. This function is mostly designed
 * for debugging purpose. The fileds in the output are subject to change.
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_dump_filter(struct libscols_filter *fltr, FILE *out)
{
	struct ul_jsonwrt json;

	if (!fltr || !out)
		return -EINVAL;

	ul_jsonwrt_init(&json, out, 0);
	ul_jsonwrt_root_open(&json);

	filter_dump_node(&json, fltr->root);
	ul_jsonwrt_root_close(&json);
	return 0;
}

/**
 * scols_filter_get_errmsg:
 * @fltr: filter instance
 *
 * Returns: string with parse-error message of NULL (if no error)
 *
 * Since: 2.40
 */
const char *scols_filter_get_errmsg(struct libscols_filter *fltr)
{
	return fltr ? fltr->errmsg : NULL;
}

int filter_eval_node(struct libscols_filter *fltr, struct libscols_line *ln,
			struct filter_node *n, int *status)
{
	switch (n->type) {
	case F_NODE_PARAM:
		return filter_eval_param(fltr, ln, (struct filter_param *) n, status);
	case F_NODE_EXPR:
		return filter_eval_expr(fltr, ln, (struct filter_expr *) n, status);
	default:
		break;
	}
	return -EINVAL;
}

/**
 * scols_line_apply_filter:
 * @ln: apply filter to the line
 * @fltr: filter instance
 * @status: return 1 or 0 as result of the expression
 *
 * Applies filter (and also counters assisiated with the filter).
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_line_apply_filter(struct libscols_line *ln,
			struct libscols_filter *fltr, int *status)
{
	int rc, res = 0;
	struct libscols_iter itr;
	struct filter_param *prm = NULL;

	if (!ln || !fltr)
		return -EINVAL;

	/* reset column data and types stored in the filter */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (filter_next_param(fltr, &itr, &prm) == 0) {
		filter_param_reset_holder(prm);
	}

	if (fltr->root)
		rc = filter_eval_node(fltr, ln, fltr->root, &res);
	else
		rc = 0, res = 1;	/* empty filter matches all lines */

	if (rc == 0) {
		struct libscols_counter *ct = NULL;

		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
		while (scols_filter_next_counter(fltr, &itr, &ct) == 0) {
			if ((ct->neg && res == 0) || res == 1)
				filter_count_param(fltr, ln, ct);
		}
	}

	if (status)
		*status = res;
	DBG(FLTR, ul_debugobj(fltr, "filter done [rc=%d, status=%d]", rc, res));
	return rc;
}

/**
 * scols_filter_set_filler_cb:
 * @fltr: filter instance
 * @cb: application defined callback
 * @userdata: pointer to private callback data
 *
 * The application can apply filter for empty lines to avoid filling the table
 * with unnecessary data (for example if the line will be later removed from
 * the table due to filter).
 *
 * This callback is used by filter to ask application to fill to the line data
 * which are necessary to evaluate the filter expression. The callback
 * arguments are filter, column number and userdata.
 *
 * <informalexample>
 *   <programlisting>
 *      ln = scols_table_new_line(tab, NULL);
 *
 *      scols_filter_set_filler_cb(filter, my_filler, NULL);
 *
 *	scols_line_apply_filter(line, filter, &status);
 *	if (status == 0)
 *		scols_table_remove_line(tab, line);
 *	else for (i = 0; i < ncolumns; i++) {
 *		if (scols_line_is_filled(line, i))
 *			continue;
 *		my_filler(NULL, ln, i, NULL);
 *	}
 *   </programlisting>
 * </informalexample>
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_filter_set_filler_cb(struct libscols_filter *fltr,
				int (*cb)(struct libscols_filter *,
					  struct libscols_line *, size_t, void *),
				void *userdata)
{
	if (!fltr)
		return -EINVAL;
	fltr->filler_cb = cb;
	fltr->filler_data = userdata;

	return 0;
}

/**
 * scols_filter_new_counter:
 * @fltr: filter instance
 *
 * Alocates a new counter instance into the filter.
 *
 * Returns: new counter or NULL in case of an error.
 *
 * Since: 2.40
 */
struct libscols_counter *scols_filter_new_counter(struct libscols_filter *fltr)
{
	struct libscols_counter *ct;

	if (!fltr)
		return NULL;

	ct = calloc(1, sizeof(*ct));
	if (!ct)
		return NULL;

	DBG(FLTR, ul_debugobj(fltr, "alloc counter"));

	ct->filter = fltr;		/* don't use ref.counting here */
	INIT_LIST_HEAD(&ct->counters);
	list_add_tail(&ct->counters, &fltr->counters);


	return ct;
}

/**
 * scols_counter_set_name:
 * @ct: counter instance
 * @name: something for humans
 *
 * The name is not use by library, it's just description usable for application
 * when prints results from countes.
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_counter_set_name(struct libscols_counter *ct, const char *name)
{
	if (!ct)
		return -EINVAL;
	return strdup_to_struct_member(ct, name, name);
}

/**
 * scols_counter_set_param:
 * @ct: counter instance
 * @name: holder (column) name
 *
 * Assigns a counter to the column. The name is used in the same way as names
 * in the filter expression. This is usable for counter that calcuate with data
 * from table cells (e.g. max, sum, etc.)
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_counter_set_param(struct libscols_counter *ct, const char *name)
{
	if (!ct)
		return -EINVAL;

	if (ct->param) {
		filter_unref_node((struct filter_node *) ct->param);
		ct->param = NULL;
	}
	if (name) {
		ct->param = (struct filter_param *)
				filter_new_param(ct->filter, SCOLS_DATA_U64,
					     F_HOLDER_COLUMN, (void *) name);
		if (!ct->param)
			return -ENOMEM;
	}
	return 0;
}

/**
 * scols_counter_set_func:
 * @ct: counter instance
 * @func: SCOLS_COUNTER_{COUNT,MAX,MIN,SUM}
 *
 * Defines function to calculate data.
 *
 * Returns: 0, a negative number in case of an error.
 *
 * Since: 2.40
 */
int scols_counter_set_func(struct libscols_counter *ct, int func)
{
	if (!ct || func < 0 || func >= __SCOLS_NCOUNTES)
		return -EINVAL;

	ct->func = func;
	return 0;
}

/**
 * scols_counter_get_result:
 * @ct: counter instance
 *
 * Returns: result from the counter
 *
 * Since: 2.40
 */
unsigned long long scols_counter_get_result(struct libscols_counter *ct)
{
	return ct ? ct->result : 0;
}

/**
 * scols_counter_get_name:
 * @ct: counter instance
 *
 * Returns: name of the counter.
 *
 * Since: 2.40
 */
const char *scols_counter_get_name(struct libscols_counter *ct)
{
	return ct ? ct->name : NULL;;
}

/**
 * scols_filter_next_counter:
 * @fltr: filter instance
 * @itr: a pointer to a struct libscols_iter instance
 * @ct: returns the next counter
 *
 * Finds the next counter and returns a pointer to it via @ct.
 *
 * Returns: 0, a negative value in case of an error, and 1 at the end.
 *
 * Since: 2.40
 */
int scols_filter_next_counter(struct libscols_filter *fltr,
		      struct libscols_iter *itr, struct libscols_counter **ct)
{
	int rc = 1;

	if (!fltr || !itr || !ct)
		return -EINVAL;
	*ct = NULL;

	if (!itr->head)
		SCOLS_ITER_INIT(itr, &fltr->counters);
	if (itr->p != itr->head) {
		SCOLS_ITER_ITERATE(itr, *ct, struct libscols_counter, counters);
		rc = 0;
	}

	return rc;
}
