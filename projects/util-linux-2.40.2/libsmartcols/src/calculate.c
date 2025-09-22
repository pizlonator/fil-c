#include "smartcolsP.h"
#include "mbsalign.h"

static void dbg_column(struct libscols_table *tb, struct libscols_column *cl)
{
	struct libscols_wstat *st;

	if (scols_column_is_hidden(cl)) {
		DBG(COL, ul_debugobj(cl, "%s (hidden) ignored", cl->header.data));
		return;
	}

	st = &cl->wstat;

	DBG(COL, ul_debugobj(cl, "#%zu %12s: width=%zd "
				 "hint=%d max=%zu min=%zu "
				 "0x04%x [%s%s%s]",

		cl->seqnum, cl->header.data, cl->width,
		cl->width_hint >= 1.0 ? (int) cl->width_hint :
				     (int) (cl->width_hint * tb->termwidth),
		st->width_max,
		st->width_min,
		cl->flags,
		cl->flags & SCOLS_FL_TRUNC ? "trunc " : "",
		scols_column_is_right(cl) ? "right " : "",
		scols_column_is_noextremes(cl) ? "noextrem " : ""));
}

static void dbg_columns(struct libscols_table *tb)
{
	struct libscols_iter itr;
	struct libscols_column *cl;

	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_column(tb, &itr, &cl) == 0)
		dbg_column(tb, cl);
}

static int count_cell_width(struct libscols_table *tb,
		struct libscols_line *ln,
		struct libscols_column *cl,
		struct ul_buffer *buf)
{
	size_t len = 0;
	int rc = 0;
	struct libscols_cell *ce;
	char *data;

	ce = scols_line_get_cell(ln, cl->seqnum);
	scols_table_set_cursor(tb, ln, cl, ce);

	rc = __cursor_to_buffer(tb, buf, 1);
	if (rc)
		goto done;

	data = ul_buffer_get_data(buf, NULL, NULL);
	if (data) {
		len = scols_table_is_noencoding(tb) ?
			mbs_width(data) :
			mbs_safe_width(data);

		if (len == (size_t) -1)		/* ignore broken multibyte strings */
			len = 0;
	}

	if (scols_column_is_tree(cl)) {
		size_t treewidth = ul_buffer_get_safe_pointer_width(buf, SCOLS_BUFPTR_TREEEND);
		cl->width_treeart = max(cl->width_treeart, treewidth);
	}

	ce->width = len;
	cl->wstat.width_max = max(len, cl->wstat.width_max);
done:
	scols_table_reset_cursor(tb);
	return rc;
}

static int walk_count_cell_width(struct libscols_table *tb,
		struct libscols_line *ln,
		struct libscols_column *cl,
		void *data)
{
	return count_cell_width(tb, ln, cl, (struct ul_buffer *) data);
}

static double sqrtroot(double num)
{
	double tmp = 0, sq = num / 2;

	while (sq != tmp){
		tmp = sq;
		sq = (num / tmp + tmp) / 2;
	}
	return sq;
}

static void count_column_deviation(struct libscols_table *tb, struct libscols_column *cl)
{
	struct libscols_wstat *st;
	struct libscols_iter itr;
	struct libscols_line *ln;
	struct libscols_cell *ce;
	size_t sum = 0, n = 0, extra = 0;

	st = &cl->wstat;

	if (scols_column_is_tree(cl) && has_groups(tb))
		extra = tb->grpset_size + 1;

	/* count average */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_line(tb, &itr, &ln) == 0) {
		ce = scols_line_get_cell(ln, cl->seqnum);

		n++;
		sum += ce->width + extra;
	}

	if (n)
		st->width_avg = (double) sum / (double) n;

	/* count deviation */
	if (n > 1) {
		double variance;

		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
		while (scols_table_next_line(tb, &itr, &ln) == 0) {
			double diff;
			ce = scols_line_get_cell(ln, cl->seqnum);

			diff = (double) ce->width - st->width_avg;
			st->width_sqr_sum += diff * diff;	/* aka pow(x, 2) */
		}

		variance = st->width_sqr_sum / (double) (n - 1);
		st->width_deviation = sqrtroot(variance);
	}

	DBG(COL, ul_debugobj(cl, "%15s avg=%g, deviation=%g",
				cl->header.data,
				st->width_avg,
				st->width_deviation));
}

/*
 * This function counts column width.
 */
static int count_column_width(struct libscols_table *tb,
			      struct libscols_column *cl,
			      struct ul_buffer *buf)
{
	int rc = 0, no_header = 0;
	const char *data;
	struct libscols_wstat *st;
	struct libscols_iter itr;
	struct libscols_line *ln;

	assert(tb);
	assert(cl);

	st = &cl->wstat;

	cl->width = 0;
	memset(st, 0, sizeof(struct libscols_wstat));

	/* set minimal width according to width_hint */
	if (cl->width_hint < 1 && scols_table_is_maxout(tb) && tb->is_term) {
		st->width_min = (size_t) (cl->width_hint * tb->termwidth);
		if (st->width_min && !is_last_column(cl))
			st->width_min--;
	}

	/* set minimal width according to header width */
	if (!scols_table_is_noheadings(tb) &&
	    (data = scols_cell_get_data(&cl->header))) {

		size_t len = scols_table_is_noencoding(tb) ?
				mbs_width(data) : mbs_safe_width(data);

		st->width_min = max(st->width_min, len);
	} else
		no_header = 1;

	if (!st->width_min)
		st->width_min = 1;

	/* count width according to cells data */
	if (scols_table_is_tree(tb)) {
		/* Count width for tree */
		rc = scols_walk_tree(tb, cl, walk_count_cell_width, (void *) buf);
		if (rc)
			goto done;
	} else {
		/* Count width for list */
		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
		while (scols_table_next_line(tb, &itr, &ln) == 0) {
			rc = count_cell_width(tb, ln, cl, buf);
			if (rc)
				goto done;
		}
	}

	if (scols_column_is_tree(cl) && has_groups(tb)) {
		/* We don't fill buffer with groups tree ascii art during width
		 * calculation. The print function only enlarge grpset[] and we
		 * calculate final width from grpset_size.
		 */
		size_t gprwidth = tb->grpset_size + 1;
		st->width_treeart += gprwidth;
		st->width_max += gprwidth;
	}

	/* this is default, may be later reduced */
	cl->width = st->width_max;

	/* enlarge to minimal width */
	if (cl->width < st->width_min && !scols_column_is_strict_width(cl))
		cl->width = st->width_min;

	/* use absolute size for large columns */
	else if (cl->width_hint >= 1 && cl->width < (size_t) cl->width_hint
		 && st->width_min < (size_t) cl->width_hint)

		cl->width = (size_t) cl->width_hint;


	/* Column without header and data, set minimal size to zero (default is 1) */
	if (st->width_max == 0 && no_header && st->width_min == 1 && cl->width <= 1)
		cl->width = st->width_min = 0;

done:
	ON_DBG(COL, dbg_column(tb, cl));
	return rc;
}

static int cmp_deviation(struct list_head *a, struct list_head *b,
			 void *data __attribute__((__unused__)))
{
	struct libscols_column *ca = list_entry(a, struct libscols_column, cl_columns);
	struct libscols_column *cb = list_entry(b, struct libscols_column, cl_columns);

	double xa = ca->wstat.width_avg + (3.0 * ca->wstat.width_deviation);
	double xb = cb->wstat.width_avg + (3.0 * cb->wstat.width_deviation);

	return cmp_numbers(xa, xb);
}

static int cmp_seqnum(struct list_head *a, struct list_head *b,
			void *data __attribute__((__unused__)))
{
	struct libscols_column *ca = list_entry(a, struct libscols_column, cl_columns);
	struct libscols_column *cb = list_entry(b, struct libscols_column, cl_columns);

	return cmp_numbers(ca->seqnum, cb->seqnum);
}

static inline void sort_columns(struct libscols_table *tb,
			int (*cmp)(struct list_head *, struct list_head *, void *))
{
	list_sort(&tb->tb_columns, cmp, NULL);
}


/* 68%–95%–99% rule (aka empirical rule) defines relation ship between
 * mean (avg) and and standard deviation.
 *
 *	avg + (n * deviation)
 *
 * n=1	: covers 68% of data
 * n=2  : covers 95% of data
 * n=3  : covers 99.7% of data
 *
 */
static void reduce_to_68(struct libscols_column *cl, size_t wanted)
{
	struct libscols_wstat *st = &cl->wstat;
	size_t new;

	if (st->width_deviation < 1.0)
		return;

	new = (size_t) (st->width_avg + st->width_deviation);

	if (new < st->width_min)
		new = st->width_min;
	else if (new > st->width_max)
		new = st->width_max;

	if (new >= cl->width)
		return;

	if (cl->width - new > wanted)
		cl->width -= wanted;
	else
		cl->width = new;
}

static int reduce_column(struct libscols_table *tb,
			 struct libscols_column *cl,
			 size_t *width,
			 int stage,
			 int nth)
{
	struct libscols_wstat *st = &cl->wstat;
	size_t wanted, org_width, reduce = 1;
	int is_trunc = 0;

	if (stage > 6)
		return -1;
	if (tb->termwidth >= *width)
		return 1;
	/* ignore hidden columns */
	if (scols_column_is_hidden(cl))
		return 0;
	/* never truncate if already minimal width */
	if (cl->width == cl->wstat.width_min)
		return 0;
	/* nothing to truncate */
	if (cl->width == 0)
		return 0;
	/* never truncate the tree */
	if (scols_column_is_tree(cl) && *width <= cl->width_treeart)
		return 0;

	org_width = cl->width;
	wanted = *width - tb->termwidth;

	is_trunc = scols_column_is_trunc(cl)
			|| (scols_column_is_wrap(cl) && !scols_column_is_customwrap(cl));

	switch (stage) {
	case 0:
		/* reduce 1st column if with trunc or extreme flag (the
		 * columns are sorted by deviation, so 1st is the worst) */
		if (!is_trunc && !scols_column_is_noextremes(cl))
			break;
		if (nth != 0)
			break;
		reduce_to_68(cl, wanted);
		break;

	case 1:
		/* reduce extreme columns with large width deviation */
		if (st->width_deviation < st->width_avg / 2.0)
			break;
		/* fallthrough */
	case 2:
		/* reduce extreme columns */
		if (!scols_column_is_noextremes(cl))
			break;
		reduce_to_68(cl, wanted);
		break;

	case 3:
		/* reduce columns with trunc flag and relative whint and large width deviation */
		if (st->width_deviation < st->width_avg / 2.0)
			break;
		/* fallthrough */
	case 4:
		/* reduce columns with trunc flag and relative whint */
		if (!is_trunc)
			break;
		if (cl->width_hint <= 0 || cl->width_hint >= 1)
			break;
		if (cl->width < (size_t) (cl->width_hint * tb->termwidth))
			break;
		reduce_to_68(cl, wanted);
		break;

	case 5:
		/* reduce all columns with trunc flag large width deviation */
		if (st->width_deviation < st->width_avg / 2.2)
			break;
		/* fallthrough */
	case 6:
		/* reduce all columns with trunc flag */
		if (!is_trunc && !scols_column_is_noextremes(cl))
			break;
		if (nth == 0)
			/* columns are reduced in "bad first" way, be more
			 * agresive for the the worst column */
			reduce = 3;

		if (cl->width < reduce)
			reduce = cl->width;

		if (cl->width - reduce > st->width_min)
			cl->width -= reduce;
		else
			cl->width = st->width_min;
		break;
	default:
		return -1;	/* no more stages */
	}

	/* hide zero width columns */
	if (cl->width == 0)
		cl->flags |= SCOLS_FL_HIDDEN;

	if (cl->width != org_width)
		DBG(COL, ul_debugobj(cl, " [%02zd] %s reduced %zu-->%zu",
					cl->seqnum,
					cl->header.data, org_width, cl->width));

	*width -= org_width - cl->width;
	return 0;
}

/*
 * This is core of the scols_* voodoo...
 */
int __scols_calculate(struct libscols_table *tb, struct ul_buffer *buf)
{
	struct libscols_column *cl, *last_cl;
	struct libscols_iter itr;
	size_t width = 0, width_min = 0;	/* output width */
	int stage = 0, rc = 0;
	int ignore_extremes = 0, group_ncolumns = 0;
	size_t colsepsz;
	int sorted = 0;

	DBG(TAB, ul_debugobj(tb, "-----calculate-(termwidth=%zu)-----", tb->termwidth));
	tb->is_dummy_print = 1;
	colsepsz = scols_table_is_noencoding(tb) ?
			mbs_width(colsep(tb)) :
			mbs_safe_width(colsep(tb));

	if (has_groups(tb))
		group_ncolumns = 1;

	/* set basic columns width
	 */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_column(tb, &itr, &cl) == 0) {
		int is_last;

		memset(&cl->wstat, 0, sizeof(cl->wstat));
		cl->width = 0;
		cl->width_treeart = 0;
		scols_column_reset_wrap(cl);

		if (scols_column_is_hidden(cl))
			continue;

		/* we print groups chart only for the for the first tree column */
		if (scols_column_is_tree(cl) && group_ncolumns == 1) {
			cl->is_groups = 1;
			group_ncolumns++;
		}

		rc = count_column_width(tb, cl, buf);
		if (rc)
			goto done;

		is_last = is_last_column(cl);

		width += cl->width + (is_last ? 0 : colsepsz);		/* separator for non-last column */
		width_min += cl->wstat.width_min + (is_last ? 0 : colsepsz);
	}

	if (!tb->is_term) {
		DBG(TAB, ul_debugobj(tb, " non-terminal output"));
		goto done;
	}

	/* be paranoid */
	if (width_min > tb->termwidth && scols_table_is_maxout(tb)) {
		DBG(TAB, ul_debugobj(tb, " min width larger than terminal! [width=%zu, term=%zu]", width_min, tb->termwidth));
		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
		while (width_min > tb->termwidth
		       && scols_table_next_column(tb, &itr, &cl) == 0) {

			if (scols_column_is_hidden(cl))
				continue;
			width_min--;
			cl->wstat.width_min--;
		}
		DBG(TAB, ul_debugobj(tb, " min width reduced to %zu", width_min));
	}

	/* calculate statistics */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_column(tb, &itr, &cl) == 0) {

		count_column_deviation(tb, cl);

		if (scols_column_is_noextremes(cl))
			ignore_extremes++;
	}

	/* remember last column before we sort columns */
	last_cl = list_entry(tb->tb_columns.prev, struct libscols_column, cl_columns);

	/* reduce columns width */
	while (width > tb->termwidth) {
		size_t org_width = width;
		int xrc = 0, n = 0;

		if (!sorted) {
			DBG(TAB, ul_debugobj(tb, "sorting by deviation"));
			sort_columns(tb, cmp_deviation);
			ON_DBG(TAB, dbg_columns(tb));
			sorted = 1;
		}

		DBG(TAB, ul_debugobj(tb, "#%d reduce stage (width=%zu, term=%zu)",
			stage, width, tb->termwidth));

		scols_reset_iter(&itr, SCOLS_ITER_BACKWARD);

		while (width > tb->termwidth
		       && xrc == 0
		       && scols_table_next_column(tb, &itr, &cl) == 0) {
			xrc = reduce_column(tb, cl, &width, stage, n++);
		}

		if (xrc != 0)
			break;
		if (org_width == width)
			stage++;
	}

	/* enlarge */
	if (width < tb->termwidth) {
		DBG(TAB, ul_debugobj(tb, " enlarge (extreme, avalable %zu)",
					tb->termwidth - width));
		if (ignore_extremes) {
			if (!sorted) {
				sort_columns(tb, cmp_deviation);
				sorted = 1;
			}

			scols_reset_iter(&itr, SCOLS_ITER_BACKWARD);
			while (scols_table_next_column(tb, &itr, &cl) == 0) {
				size_t add;

				if (!scols_column_is_noextremes(cl) || scols_column_is_hidden(cl))
					continue;
				if (cl->wstat.width_min == 0 && cl->width == 0)
					continue;
				if (cl->width >= cl->wstat.width_max)
					continue;

				add = tb->termwidth - width;
				if (add && cl->wstat.width_max
				    && cl->width + add > cl->wstat.width_max)
					add = cl->wstat.width_max - cl->width;

				if (!add)
					continue;
				DBG(COL, ul_debugobj(cl, "  add +%zd (%s)",
							add, cl->header.data));
				cl->width += add;
				width += add;

				if (width == tb->termwidth)
					break;
			}
		}

		if (width < tb->termwidth && scols_table_is_maxout(tb)) {
			DBG(TAB, ul_debugobj(tb, " enlarge (max-out, avalable %zu)",
						tb->termwidth - width));

			/* try enlarging all columns */
			while (width < tb->termwidth) {
				scols_reset_iter(&itr, SCOLS_ITER_BACKWARD);
				while (scols_table_next_column(tb, &itr, &cl) == 0) {
					if (scols_column_is_hidden(cl))
						continue;
					DBG(COL, ul_debugobj(cl, "  add +1 (%s)",
								cl->header.data));
					cl->width++;
					width++;
					if (width == tb->termwidth)
						break;
				}
			}
		} else if (width < tb->termwidth) {
			/* enlarge the last column */
			DBG(TAB, ul_debugobj(tb, " enlarge (last column, avalable %zu)",
						tb->termwidth - width));

			if (!scols_column_is_right(last_cl)) {
				DBG(COL, ul_debugobj(last_cl, "  add +%zu (%s)",
							tb->termwidth - width,
							last_cl->header.data));
				last_cl->width += tb->termwidth - width;
				width = tb->termwidth;
			}
		}
	}


	if (sorted) {
		sort_columns(tb, cmp_seqnum);
		sorted = 0;
	}

	/* ignore last column(s) or force last column to be truncated if
	 * nowrap mode enabled */
	if (tb->no_wrap && width > tb->termwidth) {
		scols_reset_iter(&itr, SCOLS_ITER_BACKWARD);
		while (scols_table_next_column(tb, &itr, &cl) == 0) {

			if (scols_column_is_hidden(cl))
				continue;
			if (width <= tb->termwidth)
				break;
			if (width - cl->width < tb->termwidth) {
				size_t r =  width - tb->termwidth;

				cl->flags |= SCOLS_FL_TRUNC;
				cl->width -= r;
				width -= r;
			} else {
				cl->flags |= SCOLS_FL_HIDDEN;
				width -= cl->width + colsepsz;
			}
		}
	}
done:
	if (sorted)
		sort_columns(tb, cmp_seqnum);

	tb->is_dummy_print = 0;
	DBG(TAB, ul_debugobj(tb, "-----final width: %zu (rc=%d)-----", width, rc));
	ON_DBG(TAB, dbg_columns(tb));

	return rc;
}
