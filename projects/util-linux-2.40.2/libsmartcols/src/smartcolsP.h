/*
 * smartcolsP.h - private library header file
 *
 * Copyright (C) 2014 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#ifndef _LIBSMARTCOLS_PRIVATE_H
#define _LIBSMARTCOLS_PRIVATE_H

#include "c.h"
#include "list.h"
#include "strutils.h"
#include "color-names.h"
#include "jsonwrt.h"
#include "debug.h"
#include "buffer.h"

#include <stdbool.h>

#include "libsmartcols.h"

/*
 * Debug
 */
#define SCOLS_DEBUG_HELP	(1 << 0)
#define SCOLS_DEBUG_INIT	(1 << 1)
#define SCOLS_DEBUG_CELL	(1 << 2)
#define SCOLS_DEBUG_LINE	(1 << 3)
#define SCOLS_DEBUG_TAB		(1 << 4)
#define SCOLS_DEBUG_COL		(1 << 5)
#define SCOLS_DEBUG_BUFF	(1 << 6)
#define SCOLS_DEBUG_GROUP	(1 << 7)
#define SCOLS_DEBUG_FLTR	(1 << 8)
#define SCOLS_DEBUG_FPARAM	(1 << 9)
#define SCOLS_DEBUG_ALL		0xFFFF

UL_DEBUG_DECLARE_MASK(libsmartcols);
#define DBG(m, x)	__UL_DBG(libsmartcols, SCOLS_DEBUG_, m, x)
#define ON_DBG(m, x)	__UL_DBG_CALL(libsmartcols, SCOLS_DEBUG_, m, x)
#define DBG_FLUSH	__UL_DBG_FLUSH(libsmartcols, SCOLS_DEBUG_)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libsmartcols)
#include "debugobj.h"

#define SCOLS_BUFPTR_TREEEND	0

/*
 * Generic iterator
 */
struct libscols_iter {
	struct list_head        *p;		/* current position */
	struct list_head        *head;		/* start position */
	int			direction;	/* SCOLS_ITER_{FOR,BACK}WARD */
};

/*
 * Tree symbols
 */
struct libscols_symbols {
	int	refcount;

	char	*tree_branch;
	char	*tree_vert;
	char	*tree_right;

	char	*group_vert;
	char	*group_horz;
	char    *group_first_member;
	char	*group_last_member;
	char	*group_middle_member;
	char	*group_last_child;
	char	*group_middle_child;

	char	*title_padding;
	char	*cell_padding;
};

/*
 * Table cells
 */
struct libscols_cell {
	char	*data;
	size_t	datasiz;
	char	*color;
	void    *userdata;
	int	flags;
	size_t	width;

	unsigned int is_filled : 1;
};

extern int scols_line_move_cells(struct libscols_line *ln, size_t newn, size_t oldn);

struct libscols_wstat {
	size_t	width_min;
	size_t	width_max;
	double	width_avg;
	double  width_sqr_sum;
	double  width_deviation;

	size_t  width_treeart;
};

/*
 * Table column
 */
struct libscols_column {
	int	refcount;	/* reference counter */
	size_t	seqnum;		/* column index */

	size_t	width;		/* expected column width */
	size_t  width_treeart;
	double	width_hint;	/* hint (N < 1 is in percent of termwidth) */

	struct libscols_wstat wstat;	/* private __scols_calculate() data */

	int	json_type;	/* SCOLS_JSON_* */
	int	data_type;	/* SCOLS_DATA_* */

	int	flags;
	char	*color;		/* default column color */
	char	*safechars;	/* do not encode this bytes */

	int (*cmpfunc)(struct libscols_cell *,
		       struct libscols_cell *,
		       void *);			/* cells comparison function */
	void *cmpfunc_data;

	/* multi-line cell data wrapping */
	char *(*wrap_nextchunk)(const struct libscols_column *, char *, void *);
	void *wrapfunc_data;

	size_t	wrap_datasz;
	size_t  wrap_datamax;
	char	*wrap_data;
	char	*wrap_cur;
	char    *wrap_next;
	struct libscols_cell	*wrap_cell;

	void *(*datafunc)(const struct libscols_column *,
			struct libscols_cell *,
			void *);
	void *datafunc_data;

	struct libscols_cell	header;		/* column name with color etc. */
	char	*shellvar;			/* raw colum name in shell compatible format */

	struct list_head	cl_columns;	/* member of table->tb_columns */

	struct libscols_table	*table;

	unsigned int	is_groups  : 1;		/* print group chart */

};

#define colsep(tb)	((tb)->colsep ? (tb)->colsep : " ")
#define linesep(tb)	((tb)->linesep ? (tb)->linesep : "\n")

enum {
	SCOLS_GSTATE_NONE = 0,		/* not activate yet */
	SCOLS_GSTATE_FIRST_MEMBER,
	SCOLS_GSTATE_MIDDLE_MEMBER,
	SCOLS_GSTATE_LAST_MEMBER,
	SCOLS_GSTATE_MIDDLE_CHILD,
	SCOLS_GSTATE_LAST_CHILD,
	SCOLS_GSTATE_CONT_MEMBERS,
	SCOLS_GSTATE_CONT_CHILDREN
};

/*
 * Every group needs at least 3 columns
 */
#define SCOLS_GRPSET_CHUNKSIZ	3

struct libscols_group {
	int     refcount;

	size_t  nmembers;

	struct list_head gr_members;	/* head of line->ln_group */
	struct list_head gr_children;	/* head of line->ln_children */
	struct list_head gr_groups;	/* member of table->tb_groups */

	int	state;			/* SCOLS_GSTATE_* */
};

/*
 * Table line
 */
struct libscols_line {
	int	refcount;
	size_t	seqnum;

	void	*userdata;
	char	*color;		/* default line color */

	struct libscols_cell	*cells;		/* array with data */
	size_t			ncells;		/* number of cells */

	struct list_head	ln_lines;	/* member of table->tb_lines */
	struct list_head	ln_branch;	/* head of line->ln_children */
	struct list_head	ln_children;	/* member of line->ln_children or group->gr_children */
	struct list_head	ln_groups;	/* member of group->gr_groups */

	struct libscols_line	*parent;
	struct libscols_group	*parent_group;	/* for group childs */
	struct libscols_group	*group;		/* for group members */
};

enum {
	SCOLS_FMT_HUMAN = 0,		/* default, human readable */
	SCOLS_FMT_RAW,			/* space separated */
	SCOLS_FMT_EXPORT,		/* COLNAME="data" ... */
	SCOLS_FMT_JSON			/* http://en.wikipedia.org/wiki/JSON */
};

/*
 * The table
 */
struct libscols_table {
	int	refcount;
	char	*name;		/* optional table name (for JSON) */
	size_t	ncols;		/* number of columns */
	size_t  ntreecols;	/* number of columns with SCOLS_FL_TREE */
	size_t	nlines;		/* number of lines */
	size_t	termwidth;	/* terminal width (number of columns) */
	size_t  termheight;	/* terminal height  (number of lines) */
	size_t  termreduce;	/* extra blank space */
	int	termforce;	/* SCOLS_TERMFORCE_* */
	FILE	*out;		/* output stream */

	char	*colsep;	/* column separator */
	char	*linesep;	/* line separator */

	struct list_head	tb_columns;	/* list of columns, items: column->cl_columns */
	struct list_head	tb_lines;	/* list of lines; items: line->ln_lines  */

	struct list_head	tb_groups;	/* all defined groups */
	struct libscols_group	**grpset;
	size_t			grpset_size;

	size_t			ngrpchlds_pending;	/* groups with not yet printed children */
	struct libscols_line	*walk_last_tree_root;	/* last root, used by scols_walk_() */

	struct libscols_column	*dflt_sort_column;	/* default sort column, set by scols_sort_table() */

	struct libscols_symbols	*symbols;
	struct libscols_cell	title;		/* optional table title (for humans) */

	struct ul_jsonwrt	json;		/* JSON formatting */

	int	format;		/* SCOLS_FMT_* */

	size_t	termlines_used;	/* printed line counter */
	size_t	header_next;	/* where repeat header */

	const char *cur_color;	/* current active color when printing */

	struct libscols_cell *cur_cell;		/* currently used cell */
	struct libscols_line *cur_line;		/* currently used line */
	struct libscols_column *cur_column;	/* currently used column */

	/* flags */
	unsigned int	ascii		:1,	/* don't use unicode */
			colors_wanted	:1,	/* enable colors */
			is_term		:1,	/* isatty() */
			padding_debug	:1,	/* output visible padding chars */
			is_dummy_print	:1,	/* printing used for width calculation only */
			is_shellvar	:1,	/* shell compatible column names */
			maxout		:1,	/* maximize output */
			minout		:1,	/* minimize output (mutually exclusive to maxout) */
			header_repeat   :1,     /* print header after libscols_table->termheight */
			header_printed  :1,	/* header already printed */
			priv_symbols	:1,	/* default private symbols */
			walk_last_done	:1,	/* last tree root walked */
			no_headings	:1,	/* don't print header */
			no_encode	:1,	/* don't care about control and non-printable chars */
			no_linesep	:1,	/* don't print line separator */
			no_wrap		:1;	/* never wrap lines */
};

#define IS_ITER_FORWARD(_i)	((_i)->direction == SCOLS_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == SCOLS_ITER_BACKWARD)

#define SCOLS_ITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define SCOLS_ITER_ITERATE(itr, res, restype, member) \
	do { \
		res = list_entry((itr)->p, restype, member); \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)


static inline int scols_iter_is_last(const struct libscols_iter *itr)
{
	if (!itr || !itr->head || !itr->p)
		return 0;

	return itr->p == itr->head;
}

/*
 * line.c
 */
int scols_line_next_group_child(struct libscols_line *ln,
                          struct libscols_iter *itr,
                          struct libscols_line **chld);

/*
 * column.c
 */
void scols_column_reset_wrap(struct libscols_column *cl);
int scols_column_next_wrap(     struct libscols_column *cl,
                                struct libscols_cell *ce,
                                char **data);
int scols_column_greatest_wrap( struct libscols_column *cl,
                                struct libscols_cell *ce,
                                char **data);
int scols_column_has_pending_wrap(struct libscols_column *cl);
int scols_column_move_wrap(struct libscols_column *cl, size_t bytes);

/*
 * table.c
 */
int scols_table_next_group(struct libscols_table *tb,
                          struct libscols_iter *itr,
                          struct libscols_group **gr);
int scols_table_set_cursor(struct libscols_table *tb,
                           struct libscols_line *ln,
                           struct libscols_column *cl,
                           struct libscols_cell *ce);

#define scols_table_reset_cursor(_t)	scols_table_set_cursor((_t), NULL, NULL, NULL)


/*
 * grouping.c
 */
void scols_ref_group(struct libscols_group *gr);
void scols_group_remove_children(struct libscols_group *gr);
void scols_group_remove_members(struct libscols_group *gr);
void scols_unref_group(struct libscols_group *gr);
void scols_groups_fix_members_order(struct libscols_table *tb);
int scols_groups_update_grpset(struct libscols_table *tb, struct libscols_line *ln);
void scols_groups_reset_state(struct libscols_table *tb);
struct libscols_group *scols_grpset_get_printable_children(struct libscols_table *tb);

/*
 * walk.c
 */
extern int scols_walk_tree(struct libscols_table *tb,
                    struct libscols_column *cl,
                    int (*callback)(struct libscols_table *,
                                    struct libscols_line *,
                                    struct libscols_column *,
                                    void *),
                    void *data);
extern int scols_walk_is_last(struct libscols_table *tb, struct libscols_line *ln);

/*
 * calculate.c
 */
extern int __scols_calculate(struct libscols_table *tb, struct ul_buffer *buf);

/*
 * print.c
 */
int __cursor_to_buffer(struct libscols_table *tb,
                    struct ul_buffer *buf,
		    int cal);

void __scols_cleanup_printing(struct libscols_table *tb, struct ul_buffer *buf);
int __scols_initialize_printing(struct libscols_table *tb, struct ul_buffer *buf);
int __scols_print_tree(struct libscols_table *tb, struct ul_buffer *buf);
int __scols_print_table(struct libscols_table *tb, struct ul_buffer *buf);
int __scols_print_header(struct libscols_table *tb, struct ul_buffer *buf);
int __scols_print_title(struct libscols_table *tb);
int __scols_print_range(struct libscols_table *tb,
                        struct ul_buffer *buf,
                        struct libscols_iter *itr,
                        struct libscols_line *end);

static inline int is_tree_root(struct libscols_line *ln)
{
	return ln && !ln->parent && !ln->parent_group;
}

static inline int is_last_tree_root(struct libscols_table *tb, struct libscols_line *ln)
{
	if (!ln || !tb || tb->walk_last_tree_root != ln)
		return 0;

	return 1;
}

static inline int is_child(struct libscols_line *ln)
{
	return ln && ln->parent;
}

static inline int is_last_child(struct libscols_line *ln)
{
	if (!ln || !ln->parent)
		return 0;

	return list_entry_is_last(&ln->ln_children, &ln->parent->ln_branch);
}

static inline int is_first_child(struct libscols_line *ln)
{
	if (!ln || !ln->parent)
		return 0;

	return list_entry_is_first(&ln->ln_children, &ln->parent->ln_branch);
}


static inline int is_last_column(struct libscols_column *cl)
{
	struct libscols_column *next;

	if (list_entry_is_last(&cl->cl_columns, &cl->table->tb_columns))
		return 1;

	next = list_entry(cl->cl_columns.next, struct libscols_column, cl_columns);
	if (next && scols_column_is_hidden(next) && is_last_column(next))
		return 1;
	return 0;
}

static inline int is_last_group_member(struct libscols_line *ln)
{
	if (!ln || !ln->group)
		return 0;

	return list_entry_is_last(&ln->ln_groups, &ln->group->gr_members);
}

static inline int is_first_group_member(struct libscols_line *ln)
{
	if (!ln || !ln->group)
		return 0;

	return list_entry_is_first(&ln->ln_groups, &ln->group->gr_members);
}

static inline int is_group_member(struct libscols_line *ln)
{
	return ln && ln->group;
}

static inline int is_last_group_child(struct libscols_line *ln)
{
	if (!ln || !ln->parent_group)
		return 0;

	return list_entry_is_last(&ln->ln_children, &ln->parent_group->gr_children);
}

static inline int is_group_child(struct libscols_line *ln)
{
	return ln && ln->parent_group;
}

static inline int has_groups(struct libscols_table *tb)
{
	return tb && !list_empty(&tb->tb_groups);
}

static inline int has_children(struct libscols_line *ln)
{
	return ln && !list_empty(&ln->ln_branch);
}

static inline int has_group_children(struct libscols_line *ln)
{
	return ln && ln->group && !list_empty(&ln->group->gr_children);
}

/*
 * Filter stuff
 */
enum filter_holder {
	F_HOLDER_NONE,
	F_HOLDER_COLUMN		/* column name */
};

/* node types */
enum filter_ntype {
	F_NODE_PARAM,
	F_NODE_EXPR
};

/* expresion types */
enum filter_etype {
	F_EXPR_AND,
	F_EXPR_OR,
	F_EXPR_NEG,

	F_EXPR_EQ,
	F_EXPR_NE,

	F_EXPR_LT,
	F_EXPR_LE,
	F_EXPR_GT,
	F_EXPR_GE,

	F_EXPR_REG,
	F_EXPR_NREG,
};

struct filter_node {
	enum filter_ntype type;
	int refcount;
};

#define filter_node_get_type(n)	(((struct filter_node *)(n))->type)

struct filter_param;
struct filter_expr;

struct libscols_counter {
	char *name;
	struct list_head counters;
	struct filter_param *param;
	struct libscols_filter *filter;

	int func;
	unsigned long long result;

	unsigned int neg : 1,
		     has_result : 1;
};

struct libscols_filter {
	int refcount;
	char *errmsg;
	struct filter_node *root;
	FILE *src;

	int (*filler_cb)(struct libscols_filter *, struct libscols_line *, size_t, void *);
	void *filler_data;

	struct list_head params;
	struct list_head counters;
};

struct filter_node *__filter_new_node(enum filter_ntype type, size_t sz);
void filter_ref_node(struct filter_node *n);
void filter_unref_node(struct filter_node *n);

void filter_dump_node(struct ul_jsonwrt *json, struct filter_node *n);
int filter_eval_node(struct libscols_filter *fltr, struct libscols_line *ln,
			struct filter_node *n, int *status);
/* param */
int filter_compile_param(struct libscols_filter *fltr, struct filter_param *n);
void filter_dump_param(struct ul_jsonwrt *json, struct filter_param *n);
int filter_eval_param(struct libscols_filter *fltr, struct libscols_line *ln,
			struct filter_param *n, int *status);
void filter_free_param(struct filter_param *n);
int filter_param_reset_holder(struct filter_param *n);
int filter_param_get_datatype(struct filter_param *n);

int filter_next_param(struct libscols_filter *fltr,
                        struct libscols_iter *itr, struct filter_param **prm);

int filter_compare_params(struct libscols_filter *fltr,
                          enum filter_etype oper,
                          struct filter_param *l,
                          struct filter_param *r,
                          int *status);
int filter_cast_param(struct libscols_filter *fltr,
                      struct libscols_line *ln,
                      int type,
                      struct filter_param *n,
                      struct filter_param **result);

int is_filter_holder_node(struct filter_node *n);

int filter_count_param(struct libscols_filter *fltr,
                struct libscols_line *ln,
                struct libscols_counter *ct);

/* expr */
void filter_free_expr(struct filter_expr *n);
void filter_dump_expr(struct ul_jsonwrt *json, struct filter_expr *n);
int filter_eval_expr(struct libscols_filter *fltr, struct libscols_line *ln,
			struct filter_expr *n, int *status);

/* required by parser */
struct filter_node *filter_new_param(struct libscols_filter *filter,
                                 int type,
				 enum filter_holder holder,
				 void *data);
struct filter_node *filter_new_expr(struct libscols_filter *filter,
                                 enum filter_etype type,
                                 struct filter_node *left,
                                 struct filter_node *right);

#endif /* _LIBSMARTCOLS_PRIVATE_H */
