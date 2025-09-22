/*
 * line.c - functions for table handling at the line level
 *
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2014 Ondrej Oprala <ooprala@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: line
 * @title: Line
 * @short_description: cells container, also keeps tree (parent->child) information
 *
 * An API to access and modify per-line data and information.
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "smartcolsP.h"

/**
 * scols_new_line:
 *
 * Note that the line is allocated without cells, the cells will be allocated
 * later when you add the line to the table. If you want to use the line
 * without table then you have to explicitly allocate the cells by
 * scols_line_alloc_cells().
 *
 * Returns: a pointer to a new struct libscols_line instance.
 */
struct libscols_line *scols_new_line(void)
{
	struct libscols_line *ln;

	ln = calloc(1, sizeof(*ln));
	if (!ln)
		return NULL;

	DBG(LINE, ul_debugobj(ln, "alloc"));
	ln->refcount = 1;
	INIT_LIST_HEAD(&ln->ln_lines);
	INIT_LIST_HEAD(&ln->ln_children);
	INIT_LIST_HEAD(&ln->ln_branch);
	INIT_LIST_HEAD(&ln->ln_groups);
	return ln;
}

/**
 * scols_ref_line:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Increases the refcount of @ln.
 */
void scols_ref_line(struct libscols_line *ln)
{
	if (ln)
		ln->refcount++;
}

/**
 * scols_unref_line:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Decreases the refcount of @ln. When the count falls to zero, the instance
 * is automatically deallocated.
 */
void scols_unref_line(struct libscols_line *ln)
{
	if (ln && --ln->refcount <= 0) {
		DBG(CELL, ul_debugobj(ln, "dealloc"));
		list_del(&ln->ln_lines);
		list_del(&ln->ln_children);
		list_del(&ln->ln_groups);
		scols_unref_group(ln->group);
		scols_line_free_cells(ln);
		free(ln->color);
		free(ln);
		return;
	}
}

/**
 * scols_line_free_cells:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Frees the allocated cells referenced to by @ln.
 */
void scols_line_free_cells(struct libscols_line *ln)
{
	size_t i;

	if (!ln || !ln->cells)
		return;

	DBG(LINE, ul_debugobj(ln, "free cells"));

	for (i = 0; i < ln->ncells; i++)
		scols_reset_cell(&ln->cells[i]);

	free(ln->cells);
	ln->ncells = 0;
	ln->cells = NULL;
}

/**
 * scols_line_alloc_cells:
 * @ln: a pointer to a struct libscols_line instance
 * @n: the number of elements
 *
 * Allocates space for @n cells. This function is optional,
 * and libsmartcols automatically allocates necessary cells
 * according to number of columns in the table when you add
 * the line to the table. See scols_table_add_line().
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_alloc_cells(struct libscols_line *ln, size_t n)
{
	struct libscols_cell *ce;

	if (!ln)
		return -EINVAL;
	if (ln->ncells == n)
		return 0;

	if (!n) {
		scols_line_free_cells(ln);
		return 0;
	}

	DBG(LINE, ul_debugobj(ln, "alloc %zu cells", n));

	ce = reallocarray(ln->cells, n, sizeof(struct libscols_cell));
	if (!ce)
		return -errno;

	if (n > ln->ncells)
		memset(ce + ln->ncells, 0,
		       (n - ln->ncells) * sizeof(struct libscols_cell));

	ln->cells = ce;
	ln->ncells = n;
	return 0;
}

int scols_line_move_cells(struct libscols_line *ln, size_t newn, size_t oldn)
{
	struct libscols_cell ce;

	if (!ln || newn >= ln->ncells || oldn >= ln->ncells)
		return -EINVAL;
	if (oldn == newn)
		return 0;

	DBG(LINE, ul_debugobj(ln, "move cells[%zu] -> cells[%zu]", oldn, newn));

	/* remember data from old position */
	memcpy(&ce, &ln->cells[oldn], sizeof(struct libscols_cell));

	/* remove old position (move data behind oldn to oldn) */
	if (oldn + 1 < ln->ncells)
		memmove(ln->cells + oldn, ln->cells + oldn + 1,
			(ln->ncells - oldn - 1) * sizeof(struct libscols_cell));

	/* create a space for new position */
	if (newn + 1 < ln->ncells)
		memmove(ln->cells + newn + 1, ln->cells + newn,
			(ln->ncells - newn - 1) * sizeof(struct libscols_cell));

	/* copy original data to new position */
	memcpy(&ln->cells[newn], &ce, sizeof(struct libscols_cell));
	return 0;
}

/**
 * scols_line_set_userdata:
 * @ln: a pointer to a struct libscols_line instance
 * @data: user data
 *
 * Binds @data to @ln.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_set_userdata(struct libscols_line *ln, void *data)
{
	if (!ln)
		return -EINVAL;
	ln->userdata = data;
	return 0;
}

/**
 * scols_line_get_userdata:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: user data
 */
void *scols_line_get_userdata(struct libscols_line *ln)
{
	return ln->userdata;
}

/**
 * scols_line_remove_child:
 * @ln: a pointer to a struct libscols_line instance
 * @child: a pointer to a struct libscols_line instance
 *
 * Removes @child as a child of @ln.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_remove_child(struct libscols_line *ln, struct libscols_line *child)
{
	if (!ln || !child)
		return -EINVAL;

	DBG(LINE, ul_debugobj(ln, "remove child"));

	list_del_init(&child->ln_children);
	child->parent = NULL;
	scols_unref_line(child);

	scols_unref_line(ln);
	return 0;
}

/**
 * scols_line_add_child:
 * @ln: a pointer to a struct libscols_line instance
 * @child: a pointer to a struct libscols_line instance
 *
 * Sets @child as a child of @ln.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_add_child(struct libscols_line *ln, struct libscols_line *child)
{
	if (!ln || !child)
		return -EINVAL;

	DBG(LINE, ul_debugobj(ln, "add child"));
	scols_ref_line(child);
	scols_ref_line(ln);

	/* unref old<->parent */
	if (child->parent)
		scols_line_remove_child(child->parent, child);

	/* new reference from parent to child */
	list_add_tail(&child->ln_children, &ln->ln_branch);

	/* new reference from child to parent */
	child->parent = ln;
	return 0;
}

/**
 * scols_line_get_parent:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: a pointer to @ln's parent, NULL in case it has no parent or if there was an error.
 */
struct libscols_line *scols_line_get_parent(const struct libscols_line *ln)
{
	return ln ? ln->parent : NULL;
}

/**
 * scols_line_has_children:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: 1 if @ln has any children, otherwise 0.
 */
int scols_line_has_children(struct libscols_line *ln)
{
	return ln ? !list_empty(&ln->ln_branch) : 0;
}

/**
 * scols_line_next_child:
 * @ln: a pointer to a struct libscols_line instance
 * @itr: a pointer to a struct libscols_iter instance
 * @chld: a pointer to a pointer to a struct libscols_line instance
 *
 * Finds the next child and returns a pointer to it via @chld.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_next_child(struct libscols_line *ln,
			  struct libscols_iter *itr,
			  struct libscols_line **chld)
{
	int rc = 1;

	if (!ln || !itr || !chld)
		return -EINVAL;
	*chld = NULL;

	if (!itr->head)
		SCOLS_ITER_INIT(itr, &ln->ln_branch);
	if (itr->p != itr->head) {
		SCOLS_ITER_ITERATE(itr, *chld, struct libscols_line, ln_children);
		rc = 0;
	}

	return rc;
}

/* private API */
int scols_line_next_group_child(struct libscols_line *ln,
			  struct libscols_iter *itr,
			  struct libscols_line **chld)
{
	int rc = 1;

	if (!ln || !itr || !chld || !ln->group)
		return -EINVAL;
	*chld = NULL;

	if (!itr->head)
		SCOLS_ITER_INIT(itr, &ln->group->gr_children);
	if (itr->p != itr->head) {
		SCOLS_ITER_ITERATE(itr, *chld, struct libscols_line, ln_children);
		rc = 0;
	}

	return rc;
}

/**
 * scols_line_is_ancestor:
 * @ln: line
 * @parent: potential parent
 *
 * The function is designed to detect circular dependencies between @ln and
 * @parent. It checks if @ln is not any (grand) parent in the @parent's tree.
 *
 * Since: 2.30
 *
 * Returns: 0 or 1
 */
int scols_line_is_ancestor(struct libscols_line *ln, struct libscols_line *parent)
{
	while (parent) {
		if (parent == ln)
			return 1;
		parent = scols_line_get_parent(parent);
	};
	return 0;
}

/**
 * scols_line_set_color:
 * @ln: a pointer to a struct libscols_line instance
 * @color: color name or ESC sequence
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_set_color(struct libscols_line *ln, const char *color)
{
	if (color && !color_is_sequence(color)) {
		char *seq = color_get_sequence(color);
		if (!seq)
			return -EINVAL;
		free(ln->color);
		ln->color = seq;
		return 0;
	}
	return strdup_to_struct_member(ln, color, color);
}

/**
 * scols_line_get_color:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: @ln's color string, NULL in case of an error.
 */
const char *scols_line_get_color(const struct libscols_line *ln)
{
	return ln->color;
}

/**
 * scols_line_get_ncells:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: number of cells
 */
size_t scols_line_get_ncells(const struct libscols_line *ln)
{
	return ln->ncells;
}

/**
 * scols_line_get_cell:
 * @ln: a pointer to a struct libscols_line instance
 * @n: cell number to retrieve
 *
 * Returns: the @n-th cell in @ln, NULL in case of an error.
 */
struct libscols_cell *scols_line_get_cell(struct libscols_line *ln,
					  size_t n)
{
	if (!ln || n >= ln->ncells)
		return NULL;
	return &ln->cells[n];
}

/**
 * scols_line_get_column_cell:
 * @ln: a pointer to a struct libscols_line instance
 * @cl: pointer to cell
 *
 * Like scols_line_get_cell() by cell is referenced by column.
 *
 * Returns: the @n-th cell in @ln, NULL in case of an error.
 */
struct libscols_cell *scols_line_get_column_cell(
			struct libscols_line *ln,
			struct libscols_column *cl)
{
	if (!ln || !cl)
		return NULL;

	return scols_line_get_cell(ln, cl->seqnum);
}

/**
 * scols_line_set_data:
 * @ln: a pointer to a struct libscols_line instance
 * @n: number of the cell, whose data is to be set
 * @data: actual data to set
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_set_data(struct libscols_line *ln, size_t n, const char *data)
{
	struct libscols_cell *ce = scols_line_get_cell(ln, n);

	if (!ce)
		return -EINVAL;
	return scols_cell_set_data(ce, data);
}

/**
 * scols_line_set_column_data:
 * @ln: a pointer to a struct libscols_line instance
 * @cl: column, whose data is to be set
 * @data: actual data to set
 *
 * The same as scols_line_set_data() but cell is referenced by column object.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.28
 */
int scols_line_set_column_data(struct libscols_line *ln,
			       struct libscols_column *cl,
			       const char *data)
{
	return scols_line_set_data(ln, cl->seqnum, data);
}

/**
 * scols_line_get_column_data:
 * @ln: a pointer to a struct libscols_line instance
 * @cl: column, whose data is to be get
 *
 * See also scols_cell_get_data()
 *
 * Returns: cell data or NULL.
 *
 * Since: 2.38
 */
const char *scols_line_get_column_data(struct libscols_line *ln,
			       struct libscols_column *cl)
{
	struct libscols_cell *cell = scols_line_get_column_cell(ln, cl);

	return cell ? scols_cell_get_data(cell) : NULL;
}


/**
 * scols_line_refer_data:
 * @ln: a pointer to a struct libscols_line instance
 * @n: number of the cell which will refer to @data
 * @data: actual data to refer to
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_line_refer_data(struct libscols_line *ln, size_t n, char *data)
{
	struct libscols_cell *ce = scols_line_get_cell(ln, n);

	if (!ce)
		return -EINVAL;
	return scols_cell_refer_data(ce, data);
}

/**
 * scols_line_is_filled:
 * @ln: a pointer to a struct libscols_line instance
 * @n: number of the cell
 *
 * Returns: 0 or 1 if cell was already filled (note that NULL is also valid filler)
 */
int scols_line_is_filled(struct libscols_line *ln, size_t n)
{
	struct libscols_cell *ce = scols_line_get_cell(ln, n);

	return ce ? ce->is_filled : 0;
}

/**
 * scols_line_refer_column_data:
 * @ln: a pointer to a struct libscols_line instance
 * @cl: column, whose data is to be set
 * @data: actual data to refer to
 *
 * The same as scols_line_refer_data() but cell is referenced by column object.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.28
 */
int scols_line_refer_column_data(struct libscols_line *ln,
			       struct libscols_column *cl,
			       char *data)
{
	return scols_line_refer_data(ln, cl->seqnum, data);
}

/**
 * scols_copy_line:
 * @ln: a pointer to a struct libscols_line instance
 *
 * Returns: A newly allocated copy of @ln, NULL in case of an error.
 */
struct libscols_line *scols_copy_line(const struct libscols_line *ln)
{
	struct libscols_line *ret;
	size_t i;

	if (!ln)
		return NULL;

	ret = scols_new_line();
	if (!ret)
		return NULL;
	if (scols_line_set_color(ret, ln->color))
		goto err;
	if (scols_line_alloc_cells(ret, ln->ncells))
		goto err;

	ret->userdata = ln->userdata;
	ret->ncells   = ln->ncells;
	ret->seqnum   = ln->seqnum;

	DBG(LINE, ul_debugobj(ln, "copy"));

	for (i = 0; i < ret->ncells; ++i) {
		if (scols_cell_copy_content(&ret->cells[i], &ln->cells[i]))
			goto err;
	}

	return ret;
err:
	scols_unref_line(ret);
	return NULL;
}
