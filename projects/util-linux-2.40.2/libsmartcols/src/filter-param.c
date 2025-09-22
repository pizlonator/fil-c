#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>

#include "rpmatch.h"
#include "smartcolsP.h"

struct filter_param {
	struct filter_node node;
	int type;
	enum filter_holder holder;

	union {
		char *str;
		unsigned long long num;
		long double fnum;
		bool boolean;
	} val;

	struct list_head pr_params;
	struct libscols_column *col;
	char *holder_name;
	regex_t *re;

	unsigned int fetched :1,	/* holder requested */
		     empty : 1;
};

static int cast_param(int type, struct filter_param *n);

static inline const char *datatype2str(int type)
{
	static const char *types[] = {
		[SCOLS_DATA_NONE] = "none",
		[SCOLS_DATA_STRING] = "string",
		[SCOLS_DATA_U64] = "u64",
		[SCOLS_DATA_FLOAT] = "float",
		[SCOLS_DATA_BOOLEAN] = "boolean"
	};
	return types[type];
}

static char *rem_quotation(const char *p, int c)
{
	size_t len = strlen(p);

	if (*(p + (len - 1)) == c)
		len -= 2;
	return strndup(p + 1, len);
}

static int param_set_data(struct filter_param *n, int type, const void *data)
{
	const char *p;

	/*DBG(FPARAM, ul_debugobj(n, " set %s data", datatype2str(type)));*/

	switch (type) {
	case SCOLS_DATA_STRING:
		p = data;
		if (p && (*p == '"' || *p == '\''))
			n->val.str = rem_quotation(p, *p);
		else if (data)
			n->val.str = strdup((char *) data);
		if (data && !n->val.str)
			return -ENOMEM;
		if (data) {
			rtrim_whitespace((unsigned char *) n->val.str);
			ltrim_whitespace((unsigned char *) n->val.str);
		}
		break;
	case SCOLS_DATA_U64:
		n->val.num = data ? *((unsigned long long *) data) : 0;
		break;
	case SCOLS_DATA_FLOAT:
		n->val.fnum = data ? *((long double *) data) : 0;
		break;
	case SCOLS_DATA_BOOLEAN:
		n->val.boolean = data ? (*((bool *) data) == 0 ? 0 : 1) : 0;
		break;
	default:
		return 0;
	}

	n->type = type;
	n->empty = data == NULL;
	return 0;
}

struct filter_node *filter_new_param(
		struct libscols_filter *fltr,
		int type,
		enum filter_holder holder,
		void *data)
{
	struct filter_param *n = (struct filter_param *) __filter_new_node(
					F_NODE_PARAM,
					sizeof(struct filter_param));
	if (!n)
		return NULL;

	n->type = type;
	n->holder = holder;
	INIT_LIST_HEAD(&n->pr_params);

	if (param_set_data(n, type, data) != 0) {
		filter_free_param(n);
		return NULL;
	}

	if (holder == F_HOLDER_COLUMN) {
		n->holder_name = strdup((char *) data);
		DBG(FLTR, ul_debugobj(fltr, "new %s holder", n->holder_name));
	}

	if (fltr)
		list_add_tail(&n->pr_params, &fltr->params);

	return (struct filter_node *) n;
}

int filter_compile_param(struct libscols_filter *fltr, struct filter_param *n)
{
	int rc;

	if (n->re)
		return 0;
	if (!n->val.str)
		return -EINVAL;

	n->re = calloc(1, sizeof(regex_t));
	if (!n->re)
		return -ENOMEM;

	rc = regcomp(n->re, n->val.str, REG_NOSUB | REG_EXTENDED);
	if (rc) {
		size_t size = regerror(rc, n->re, NULL, 0);

		fltr->errmsg = malloc(size + 1);
		if (!fltr->errmsg)
			return -ENOMEM;
		regerror(rc, n->re, fltr->errmsg, size);
		return -EINVAL;
	}
	return 0;
}

static struct filter_param *copy_param(struct filter_param *n)
{
	void *data = NULL;

	switch (n->type) {
	case SCOLS_DATA_STRING:
		data = n->val.str;
		break;
	case SCOLS_DATA_U64:
		data = &n->val.num;
		break;
	case SCOLS_DATA_FLOAT:
		data = &n->val.fnum;
		break;
	case SCOLS_DATA_BOOLEAN:
		data = &n->val.boolean;
		break;
	}

	DBG(FPARAM, ul_debugobj(n, "copying"));
	return (struct filter_param *) filter_new_param(NULL, n->type, F_HOLDER_NONE, data);
}

static void param_reset_data(struct filter_param *n)
{
	if (n->type == SCOLS_DATA_STRING)
		free(n->val.str);

	memset(&n->val, 0, sizeof(n->val));
	n->fetched = 0;
	n->empty = 1;

	if (n->re) {
		regfree(n->re);
		free(n->re);
		n->re = NULL;
	}
}

void filter_free_param(struct filter_param *n)
{
	param_reset_data(n);

	free(n->holder_name);
	list_del_init(&n->pr_params);
	scols_unref_column(n->col);
	free(n);
}

int filter_param_get_datatype(struct filter_param *n)
{
	return n ? n->type : SCOLS_DATA_NONE;
}

int is_filter_holder_node(struct filter_node *n)
{
	return n && filter_node_get_type(n) == F_NODE_PARAM
                && ((struct filter_param *)(n))->holder;
}

void filter_dump_param(struct ul_jsonwrt *json, struct filter_param *n)
{
	ul_jsonwrt_object_open(json, "param");

	if (n->empty) {
		ul_jsonwrt_value_boolean(json, "empty", true);
		ul_jsonwrt_value_s(json, "type", datatype2str(n->type));
	} else {
		switch (n->type) {
		case SCOLS_DATA_STRING:
			ul_jsonwrt_value_s(json, "string", n->val.str);
			break;
		case SCOLS_DATA_U64:
			ul_jsonwrt_value_u64(json, "number", n->val.num);
			break;
		case SCOLS_DATA_FLOAT:
			ul_jsonwrt_value_double(json, "float", n->val.fnum);
			break;
		case SCOLS_DATA_BOOLEAN:
			ul_jsonwrt_value_boolean(json, "bool", n->val.boolean);
			break;
		default:
			break;
		}
	}

	if (n->holder == F_HOLDER_COLUMN)
		ul_jsonwrt_value_s(json, "column", n->holder_name);

	ul_jsonwrt_object_close(json);
}

int filter_param_reset_holder(struct filter_param *n)
{
	if (!n->holder || !n->col)
		return 0;

	param_reset_data(n);

	if (n->type != SCOLS_DATA_NONE)
		return 0; /* already set */

	if (scols_column_get_data_type(n->col))
		/* use by application defined type */
		n->type = scols_column_get_data_type(n->col);
	else {
		/* use by JSON defined type, default to string if not specified */
		switch (n->col->json_type) {
		case SCOLS_JSON_NUMBER:
			n->type = SCOLS_DATA_U64;
			break;
		case SCOLS_JSON_BOOLEAN:
			n->type = SCOLS_DATA_BOOLEAN;
			break;
		case SCOLS_JSON_FLOAT:
			n->type = SCOLS_DATA_FLOAT;
			break;
		case SCOLS_JSON_STRING:
		default:
			n->type = SCOLS_DATA_STRING;
			break;
		}
	}

	DBG(FPARAM, ul_debugobj(n, "holder %s type: %s", n->holder_name, datatype2str(n->type)));
	return 0;
}

static int fetch_holder_data(struct libscols_filter *fltr __attribute__((__unused__)),
			struct filter_param *n, struct libscols_line *ln)
{
	const char *data = NULL;
	struct libscols_column *cl = n->col;
	int type = n->type;
	int rc = 0;

	if (n->fetched || n->holder != F_HOLDER_COLUMN)
		return 0;
	if (!cl) {
		DBG(FPARAM, ul_debugobj(n, "no column for %s holder", n->holder_name));
		return -EINVAL;
	}
	DBG(FPARAM, ul_debugobj(n, "fetching %s data", n->holder_name));

	if (fltr->filler_cb && !scols_line_is_filled(ln, cl->seqnum)) {
		DBG(FPARAM, ul_debugobj(n, "  by callback"));
		rc = fltr->filler_cb(fltr, ln, cl->seqnum, fltr->filler_data);
		if (rc)
			return rc;
	}

	n->fetched = 1;

	if (scols_column_has_data_func(cl)) {
		struct libscols_cell *ce = scols_line_get_column_cell(ln, cl);

		DBG(FPARAM, ul_debugobj(n, " using datafunc()"));
		if (ce)
			data = cl->datafunc(n->col, ce, cl->datafunc_data);
		if (data)
			rc = param_set_data(n, scols_column_get_data_type(cl), data);
	} else {
		DBG(FPARAM, ul_debugobj(n, " using as string"));
		data = scols_line_get_column_data(ln, n->col);
		rc = param_set_data(n, SCOLS_DATA_STRING, data);
	}

	/* cast to the wanted type */
	if (rc == 0 && type != SCOLS_DATA_NONE)
		rc = cast_param(type, n);
	return rc;
}

int filter_eval_param(struct libscols_filter *fltr,
		struct libscols_line *ln,
		struct filter_param *n,
		int *status)
{
	int rc = 0;

	DBG(FLTR, ul_debugobj(fltr, "eval param"));

	rc = fetch_holder_data(fltr, n, ln);
	if (n->empty || rc) {
		*status = 0;
		goto done;
	}

	switch (n->type) {
	case SCOLS_DATA_STRING:
		*status = n->val.str != NULL && *n->val.str != '\0';
		break;
	case SCOLS_DATA_U64:
		*status = n->val.num != 0;
		break;
	case SCOLS_DATA_FLOAT:
		*status = n->val.fnum != 0.0;
		break;
	case SCOLS_DATA_BOOLEAN:
		*status = n->val.boolean != false;
		break;
	default:
		rc = -EINVAL;
		break;
	}
done:
	if (rc)
		DBG(FLTR, ul_debugobj(fltr, "failed eval param [rc=%d]", rc));
	return rc;
}

int filter_count_param(struct libscols_filter *fltr,
		struct libscols_line *ln,
		struct libscols_counter *ct)
{
	unsigned long long num = 0;

	if (ct->func == SCOLS_COUNTER_COUNT) {
		ct->result++;
		return 0;
	}

	if (ct->param) {
		int rc;

		ct->param->type = SCOLS_DATA_U64;
		rc = fetch_holder_data(fltr, ct->param, ln);
		if (rc)
			return rc;

		if (ct->param->empty)
			return -EINVAL;

		num = ct->param->val.num;
	}

	switch (ct->func) {
	case SCOLS_COUNTER_MAX:
		if (!ct->has_result)
			ct->result = num;
		else if (num > ct->result)
			ct->result = num;
		break;
	case SCOLS_COUNTER_MIN:
		if (!ct->has_result)
			ct->result = num;
		else if (num < ct->result)
			ct->result = num;
		break;
	case SCOLS_COUNTER_SUM:
		ct->result += num;
		break;
	default:
		return -EINVAL;
	}

	ct->has_result = 1;
	DBG(FLTR, ul_debugobj(fltr, "counted '%s' [result: %llu]", ct->name, ct->result));
	return 0;
}

static int xstrcmp(char *a, char *b)
{
	if (!a && !b)
		return 0;
	if (!a && b)
		return -1;
	if (a && !b)
		return 1;
	return strcmp(a, b);
}

static int string_opers(enum filter_etype oper, struct filter_param *l,
			struct filter_param *r, int *status)
{
	switch (oper) {
	case F_EXPR_EQ:
		*status = xstrcmp(l->val.str, r->val.str) == 0;
		break;
	case F_EXPR_NE:
		*status = xstrcmp(l->val.str, r->val.str) != 0;
		break;
	case F_EXPR_LE:
		*status = xstrcmp(l->val.str, r->val.str) <= 0;
		break;
	case F_EXPR_LT:
		*status = xstrcmp(l->val.str, r->val.str) < 0;
		break;
	case F_EXPR_GE:
		*status = xstrcmp(l->val.str, r->val.str) >= 0;
		break;
	case F_EXPR_GT:
		*status = xstrcmp(l->val.str, r->val.str) > 0;
		break;
	case F_EXPR_REG:
		if (!r->re)
			return -EINVAL;
		*status = regexec(r->re, l->val.str ? : "", 0, NULL, 0) == 0;
		break;
	case F_EXPR_NREG:
		if (!r->re)
			return -EINVAL;
		*status = regexec(r->re, l->val.str ? : "", 0, NULL, 0) != 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int u64_opers(enum filter_etype oper, struct filter_param *l,
			     struct filter_param *r, int *status)
{
	if (l->empty || r->empty) {
		*status = 0;
		return 0;
	}

	switch (oper) {
	case F_EXPR_EQ:
		*status = l->val.num == r->val.num;
		break;
	case F_EXPR_NE:
		*status = l->val.num != r->val.num;
		break;
	case F_EXPR_LE:
		*status = l->val.num <= r->val.num;
		break;
	case F_EXPR_LT:
		*status = l->val.num < r->val.num;
		break;
	case F_EXPR_GE:
		*status = l->val.num >= r->val.num;
		break;
	case F_EXPR_GT:
		*status = l->val.num > r->val.num;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int float_opers(enum filter_etype oper, struct filter_param *l,
			     struct filter_param *r, int *status)
{
	if (l->empty || r->empty) {
		*status = 0;
		return 0;
	}

	switch (oper) {
	case F_EXPR_EQ:
		*status = l->val.fnum == r->val.fnum;
		break;
	case F_EXPR_NE:
		*status = l->val.fnum != r->val.fnum;
		break;
	case F_EXPR_LE:
		*status = l->val.fnum <= r->val.fnum;
		break;
	case F_EXPR_LT:
		*status = l->val.fnum < r->val.fnum;
		break;
	case F_EXPR_GE:
		*status = l->val.fnum >= r->val.fnum;
		break;
	case F_EXPR_GT:
		*status = l->val.fnum > r->val.fnum;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bool_opers(enum filter_etype oper, struct filter_param *l,
			     struct filter_param *r, int *status)
{
	if (l->empty || r->empty) {
		*status = 0;
		return 0;
	}

	switch (oper) {
	case F_EXPR_EQ:
		*status = l->val.boolean == r->val.boolean;
		break;
	case F_EXPR_NE:
		*status = l->val.boolean != r->val.boolean;
		break;
	case F_EXPR_LE:
		*status = l->val.boolean <= r->val.boolean;
		break;
	case F_EXPR_LT:
		*status = l->val.boolean < r->val.boolean;
		break;
	case F_EXPR_GE:
		*status = l->val.boolean >= r->val.boolean;
		break;
	case F_EXPR_GT:
		*status = l->val.boolean > r->val.boolean;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* call filter_cast_param() to be sure that param data are ready (fetched from
 * holder, etc.) */
int filter_compare_params(struct libscols_filter *fltr __attribute__((__unused__)),
			  enum filter_etype oper,
			  struct filter_param *l,
			  struct filter_param *r,
			  int *status)
{
	int rc;

	if (!l || !r || l->type != r->type)
		return -EINVAL;

	*status = 0;

	switch (l->type) {
	case SCOLS_DATA_STRING:
		rc = string_opers(oper, l, r, status);
		break;
	case SCOLS_DATA_U64:
		rc = u64_opers(oper, l, r, status);
		break;
	case SCOLS_DATA_FLOAT:
		rc = float_opers(oper, l, r, status);
		break;
	case SCOLS_DATA_BOOLEAN:
		rc = bool_opers(oper, l, r, status);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int string_cast(int type, struct filter_param *n)
{
	char *str = n->val.str;

	if (type == SCOLS_DATA_STRING)
		return 0;

	n->val.str = NULL;

	switch (type) {
	case SCOLS_DATA_U64:
	{
		uint64_t num = 0;
		if (str) {
			int rc = ul_strtou64(str, &num, 10);
			if (rc)
				return rc;
		}
		n->val.num = num;
		break;
	}
	case SCOLS_DATA_FLOAT:
	{
		long double num = 0;
		if (str) {
			int rc = ul_strtold(str, &num);
			if (rc)
				return rc;
		}
		n->val.fnum = num;
		break;
	}
	case SCOLS_DATA_BOOLEAN:
	{
		bool x = str && *str
			     && (strcasecmp(str, "1") == 0
				 || strcasecmp(str, "true") == 0
				 || rpmatch(str) == RPMATCH_YES);
		n->val.boolean = x;
		break;
	}
	default:
		return -EINVAL;
	}

	free(str);
	return 0;
}

static int u64_cast(int type, struct filter_param *n)
{
	unsigned long long num = n->val.num;

	switch (type) {
	case SCOLS_DATA_STRING:
		n->val.str = NULL;
		if (asprintf(&n->val.str, "%llu", num) <= 0)
			return -ENOMEM;
		break;
	case SCOLS_DATA_U64:
		break;
	case SCOLS_DATA_FLOAT:
		n->val.fnum = num;
		break;
	case SCOLS_DATA_BOOLEAN:
		n->val.boolean = num > 0 ? true : false;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int float_cast(int type, struct filter_param *n)
{
	long double fnum = n->val.fnum;

	switch (type) {
	case SCOLS_DATA_STRING:
		n->val.str = NULL;
		if (asprintf(&n->val.str, "%Lg", fnum) <= 0)
			return -ENOMEM;
		break;
	case SCOLS_DATA_U64:
		n->val.num = fnum;
		break;
	case SCOLS_DATA_FLOAT:
		break;;
	case SCOLS_DATA_BOOLEAN:
		n->val.boolean = fnum > 0.0 ? true : false;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bool_cast(int type, struct filter_param *n)
{
	bool x = n->val.boolean;

	switch (type) {
	case SCOLS_DATA_STRING:
		n->val.str = NULL;
		if (asprintf(&n->val.str, "%s", x ? "true" : "false") <= 0)
			return -ENOMEM;
		break;
	case SCOLS_DATA_U64:
		n->val.num = x ? 1 : 0;
		break;
	case SCOLS_DATA_FLOAT:
		n->val.fnum = x ? 1.0 : 0.0;
		break;
	case SCOLS_DATA_BOOLEAN:
		break;;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cast_param(int type, struct filter_param *n)
{
	int rc;
	int orgtype = n->type;

	if (type == orgtype)
		return 0;

	if (orgtype == SCOLS_DATA_STRING)
		DBG(FPARAM, ul_debugobj(n, " casting \"%s\" to %s", n->val.str, datatype2str(type)));
	else
		DBG(FPARAM, ul_debugobj(n, " casting %s to %s", datatype2str(orgtype), datatype2str(type)));

	switch (orgtype) {
	case SCOLS_DATA_STRING:
		rc = string_cast(type, n);
		break;
	case SCOLS_DATA_U64:
		rc = u64_cast(type, n);
		break;
	case SCOLS_DATA_FLOAT:
		rc = float_cast(type, n);
		break;
	case SCOLS_DATA_BOOLEAN:
		rc = bool_cast(type, n);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc == 0)
		n->type = type;

	if (rc)
		DBG(FPARAM, ul_debugobj(n, "cast done [rc=%d]", rc));
	return rc;
}

int filter_cast_param(struct libscols_filter *fltr,
		      struct libscols_line *ln,
		      int type,
		      struct filter_param *n,
		      struct filter_param **result)
{
	int rc;
	int orgtype = n->type;

	DBG(FPARAM, ul_debugobj(n, "casting param to %s", datatype2str(type)));
	rc = fetch_holder_data(fltr, n, ln);
	if (rc)
		return rc;

	if (type == orgtype) {
		filter_ref_node((struct filter_node *) n);	/* caller wants to call filter_unref_node() for the result */
		*result = n;
		return 0;
	}

	*result = copy_param(n);
	if (!*result)
		return -ENOMEM;
	rc = cast_param(type, *result);

	DBG(FPARAM, ul_debugobj(n, "cast done [rc=%d]", rc));
	return rc;
}

int filter_next_param(struct libscols_filter *fltr,
		      struct libscols_iter *itr, struct filter_param **prm)
{
	int rc = 1;

	if (!fltr || !itr || !prm)
		return -EINVAL;
	*prm = NULL;

	if (!itr->head)
		SCOLS_ITER_INIT(itr, &fltr->params);
	if (itr->p != itr->head) {
		SCOLS_ITER_ITERATE(itr, *prm, struct filter_param, pr_params);
		rc = 0;
	}

	return rc;
}

/**
 * scols_filter_assign_column:
 * @fltr: pointer to filter
 * @itr: iterator
 * @name: holder name
 * @col: column
 *
 * Assign @col to filter parametr. The parametr is addressed by @itr or by
 * @name. See scols_filter_next_holder().
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.40
 */
int scols_filter_assign_column(struct libscols_filter *fltr,
			struct libscols_iter *itr,
			const char *name, struct libscols_column *col)
{
	struct filter_param *n = NULL;

	if (itr && itr->p) {
		struct list_head *p = IS_ITER_FORWARD(itr) ?
						itr->p->prev : itr->p->next;
		n = list_entry(p, struct filter_param, pr_params);
	} else if (name) {
		struct libscols_iter xitr;
		struct filter_param *x = NULL;

		scols_reset_iter(&xitr, SCOLS_ITER_FORWARD);
		while (filter_next_param(fltr, &xitr, &x) == 0) {
			if (x->col
			    || x->holder != F_HOLDER_COLUMN
			    || strcmp(name, x->holder_name) != 0)
				continue;
			n = x;
			break;
		}
	}

	if (n) {
		if (n->col)
			scols_unref_column(n->col);

		DBG(FPARAM, ul_debugobj(n, "assing %s to column %s", name,
					scols_column_get_name(col)));
		n->col = col;
		scols_ref_column(col);
	}

	return n ? 0 : -EINVAL;
}

/**
 * scols_filter_next_holder:
 * @fltr: filter instance
 * @itr: a pointer to a struct libscols_iter instance
 * @name: returns the next column name
 * @type: 0 (not implemented yet)
 *
 * Finds the next holder used in the expression and and returns a name via
 * @name. The currently supported holder type is only column name.
 *
 * Returns: 0, a negative value in case of an error, and 1 at the end.
 *
 * Since: 2.40
 */
int scols_filter_next_holder(struct libscols_filter *fltr,
			struct libscols_iter *itr,
			const char **name,
			int type)
{
	struct filter_param *prm = NULL;
	int rc = 0;

	*name = NULL;
	if (!type)
		type = F_HOLDER_COLUMN;	/* default */

	do {
		rc = filter_next_param(fltr, itr, &prm);
		if (rc == 0 && (int) prm->holder == type) {
			*name = prm->holder_name;
		}
	} while (rc == 0 && !*name);

	return rc;
}
