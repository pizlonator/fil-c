 /* print.c - functions to print table
 *
 * Copyright (C) 2010-2014 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2016 Igor Gnatenko <i.gnatenko.brain@gmail.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: table_print
 * @title: Table print
 * @short_description: output functions
 *
 * Table output API.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>

#include "mbsalign.h"
#include "carefulputc.h"
#include "smartcolsP.h"

/* Fallback for symbols
 *
 * Note that by default library define all the symbols, but in case user does
 * not define all symbols or if we extended the symbols struct then we need
 * fallback to be more robust and backwardly compatible.
 */
#define titlepadding_symbol(tb)	((tb)->symbols->title_padding ? (tb)->symbols->title_padding : " ")
#define branch_symbol(tb)	((tb)->symbols->tree_branch ? (tb)->symbols->tree_branch : "|-")
#define vertical_symbol(tb)	((tb)->symbols->tree_vert ? (tb)->symbols->tree_vert : "| ")
#define right_symbol(tb)	((tb)->symbols->tree_right ? (tb)->symbols->tree_right : "`-")

#define grp_vertical_symbol(tb)	((tb)->symbols->group_vert ? (tb)->symbols->group_vert : "|")
#define grp_horizontal_symbol(tb) ((tb)->symbols->group_horz ? (tb)->symbols->group_horz : "-")
#define grp_m_first_symbol(tb)	((tb)->symbols->group_first_member ? (tb)->symbols->group_first_member : ",->")
#define grp_m_last_symbol(tb)	((tb)->symbols->group_last_member ? (tb)->symbols->group_last_member : "\\->")
#define grp_m_middle_symbol(tb)	((tb)->symbols->group_middle_member ? (tb)->symbols->group_middle_member : "|->")
#define grp_c_middle_symbol(tb)	((tb)->symbols->group_middle_child ? (tb)->symbols->group_middle_child : "|-")
#define grp_c_last_symbol(tb)	((tb)->symbols->group_last_child ? (tb)->symbols->group_last_child : "`-")

#define cellpadding_symbol(tb)  ((tb)->padding_debug ? "." : \
				 ((tb)->symbols->cell_padding ? (tb)->symbols->cell_padding: " "))

#define want_repeat_header(tb)	(!(tb)->header_repeat || (tb)->header_next <= (tb)->termlines_used)

static int is_next_columns_empty(
			struct libscols_table *tb,
			struct libscols_column *cl,
			struct libscols_line *ln)
{
	struct libscols_iter itr;

	if (!tb || !cl)
		return 0;
	if (is_last_column(cl))
		return 1;
	if (!ln)
		return 0;

	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	scols_table_set_columns_iter(tb, &itr, cl);

	/* skip current column */
	scols_table_next_column(tb, &itr, &cl);

	while (scols_table_next_column(tb, &itr, &cl) == 0) {
		struct libscols_cell *ce;
		const char *data = NULL;

                if (scols_column_is_hidden(cl))
                        continue;
		if (scols_column_is_tree(cl))
			return 0;

		ce = scols_line_get_cell(ln, cl->seqnum);
		if (ce)
			data = scols_cell_get_data(ce);
		if (data && *data)
			return 0;
	}
	return 1;
}

/* returns pointer to the end of used data */
static int tree_ascii_art_to_buffer(struct libscols_table *tb,
				    struct libscols_line *ln,
				    struct ul_buffer *buf)
{
	const char *art;
	int rc;

	assert(ln);
	assert(buf);

	if (!ln->parent)
		return 0;

	rc = tree_ascii_art_to_buffer(tb, ln->parent, buf);
	if (rc)
		return rc;

	if (is_last_child(ln))
		art = "  ";
	else
		art = vertical_symbol(tb);

	return ul_buffer_append_string(buf, art);
}

static int grpset_is_empty(	struct libscols_table *tb,
				size_t idx,
				size_t *rest)
{
	size_t i;

	for (i = idx; i < tb->grpset_size; i++) {
		if (tb->grpset[i] == NULL) {
			if (rest)
				(*rest)++;
		} else
			return 0;
	}
	return 1;
}

static int groups_ascii_art_to_buffer(	struct libscols_table *tb,
				struct libscols_line *ln,
				struct ul_buffer *buf,
				int empty)
{
	int filled = 0;
	size_t i, rest = 0;
	const char *filler = cellpadding_symbol(tb);

	if (!has_groups(tb))
		return 0;

	DBG(LINE, ul_debugobj(ln, "printing groups chart"));

	if (tb->is_dummy_print)
		return 0;		/* allocate grpset[] only */

	for (i = 0; i < tb->grpset_size; i += SCOLS_GRPSET_CHUNKSIZ) {
		struct libscols_group *gr = tb->grpset[i];

		if (!gr) {
			ul_buffer_append_ntimes(buf, SCOLS_GRPSET_CHUNKSIZ, cellpadding_symbol(tb));
			continue;
		}

		/*
		 * Empty cells (multi-line entries, etc.), print vertical symbols only
		 * to show that the group continues.
		 */
		if (empty) {
			switch (gr->state) {
			case SCOLS_GSTATE_FIRST_MEMBER:
			case SCOLS_GSTATE_MIDDLE_MEMBER:
			case SCOLS_GSTATE_CONT_MEMBERS:
				ul_buffer_append_string(buf, grp_vertical_symbol(tb));
				ul_buffer_append_ntimes(buf, 2, filler);
				break;

			case SCOLS_GSTATE_LAST_MEMBER:
			case SCOLS_GSTATE_MIDDLE_CHILD:
			case SCOLS_GSTATE_CONT_CHILDREN:
				ul_buffer_append_string(buf, filler);
				ul_buffer_append_string(buf, grp_vertical_symbol(tb));
				ul_buffer_append_string(buf, filler);
				break;
			case SCOLS_GSTATE_LAST_CHILD:
				ul_buffer_append_ntimes(buf, 3, filler);
				break;
			}
			continue;
		}

		/*
		 * Regular cell
		 */
		switch (gr->state) {
		case SCOLS_GSTATE_FIRST_MEMBER:
			ul_buffer_append_string(buf, grp_m_first_symbol(tb));
			break;
		case SCOLS_GSTATE_MIDDLE_MEMBER:
			ul_buffer_append_string(buf, grp_m_middle_symbol(tb));
			break;
		case SCOLS_GSTATE_LAST_MEMBER:
			ul_buffer_append_string(buf, grp_m_last_symbol(tb));
			break;
		case SCOLS_GSTATE_CONT_MEMBERS:
			ul_buffer_append_string(buf, grp_vertical_symbol(tb));
			ul_buffer_append_ntimes(buf, 2, filler);
			break;
		case SCOLS_GSTATE_MIDDLE_CHILD:
			ul_buffer_append_string(buf, filler);
			ul_buffer_append_string(buf, grp_c_middle_symbol(tb));
			if (grpset_is_empty(tb, i + SCOLS_GRPSET_CHUNKSIZ, &rest)) {
				ul_buffer_append_ntimes(buf, rest+1, grp_horizontal_symbol(tb));
				filled = 1;
			}
			filler = grp_horizontal_symbol(tb);
			break;
		case SCOLS_GSTATE_LAST_CHILD:
			ul_buffer_append_string(buf, cellpadding_symbol(tb));
			ul_buffer_append_string(buf, grp_c_last_symbol(tb));
			if (grpset_is_empty(tb, i + SCOLS_GRPSET_CHUNKSIZ, &rest)) {
				ul_buffer_append_ntimes(buf, rest+1, grp_horizontal_symbol(tb));
				filled = 1;
			}
			filler = grp_horizontal_symbol(tb);
			break;
		case SCOLS_GSTATE_CONT_CHILDREN:
			ul_buffer_append_string(buf, filler);
			ul_buffer_append_string(buf, grp_vertical_symbol(tb));
			ul_buffer_append_string(buf, filler);
			break;
		}

		if (filled)
			break;
	}

	if (!filled)
		ul_buffer_append_string(buf, filler);
	return 0;
}

static void fputs_color_reset(struct libscols_table *tb)
{
	if (tb->cur_color) {
		fputs(UL_COLOR_RESET, tb->out);
		tb->cur_color = NULL;
	}
}

static void fputs_color(struct libscols_table *tb, const char *color)
{
	if (tb->cur_color)
		fputs_color_reset(tb);

	tb->cur_color = color;
	if (color)
		fputs(color, tb->out);
}

static const char *get_cell_color(struct libscols_table *tb,
				struct libscols_column *cl,
				struct libscols_line *ln,
				struct libscols_cell *ce)
{
	const char *color = NULL;

	if (!tb || !tb->colors_wanted || tb->format != SCOLS_FMT_HUMAN)
		return NULL;
	if (ce)
		color = ce->color;
	if (!color && (!ln || !ln->color) && cl)
		color = cl->color;
	return color;
}

/* switch from line color to cell/column color */
static void fputs_color_cell_open(struct libscols_table *tb,
				struct libscols_column *cl,
				struct libscols_line *ln,
				struct libscols_cell *ce)
{
	const char *color = get_cell_color(tb, cl, ln, ce);

	if (color)
		fputs_color(tb, color);
}

/* switch from cell/column color to line color or reset */
static void fputs_color_cell_close(struct libscols_table *tb,
				struct libscols_column *cl,
				struct libscols_line *ln,
				struct libscols_cell *ce)
{
	const char *color = get_cell_color(tb, cl, ln, ce);

	if (color)
		fputs_color(tb, ln ? ln->color : NULL);
}

/* switch to line color */
static void fputs_color_line_open(struct libscols_table *tb,
				struct libscols_line *ln)
{
	if (!tb || !tb->colors_wanted || tb->format != SCOLS_FMT_HUMAN)
		return;
	fputs_color(tb, ln ? ln->color : NULL);
}

/* switch off all colors */
static void fputs_color_line_close(struct libscols_table *tb)
{
	if (!tb || !tb->colors_wanted || tb->format != SCOLS_FMT_HUMAN)
		return;
	fputs_color_reset(tb);
}

/* print padding or ASCII-art instead of data of @cl */
static void print_empty_cell(struct libscols_table *tb,
			  struct libscols_column *cl,
			  struct libscols_line *ln,	/* optional */
			  struct libscols_cell *ce,
			  size_t bufsz)
{
	size_t len_pad = 0;		/* in screen cells as opposed to bytes */

	DBG(COL, ul_debugobj(cl, " printing empty cell"));

	fputs_color_cell_open(tb, cl, ln, ce);

	/* generate tree/group ASCII-art rather than padding
	 */
	if (ln && scols_column_is_tree(cl)) {
		struct ul_buffer art = UL_INIT_BUFFER;
		char *data;

		if (ul_buffer_alloc_data(&art, bufsz) != 0)
			goto done;

		if (cl->is_groups)
			groups_ascii_art_to_buffer(tb, ln, &art, 1);

		tree_ascii_art_to_buffer(tb, ln, &art);

		if (!list_empty(&ln->ln_branch))
			ul_buffer_append_string(&art, vertical_symbol(tb));

		if (scols_table_is_noencoding(tb))
			data = ul_buffer_get_data(&art, NULL, &len_pad);
		else
			data = ul_buffer_get_safe_data(&art, NULL, &len_pad, NULL);

		if (data && len_pad)
			fputs(data, tb->out);
		ul_buffer_free_data(&art);
	}

done:
	/* minout -- don't fill */
	if (scols_table_is_minout(tb) && is_next_columns_empty(tb, cl, ln)) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return;
	}

	/* default -- fill except last column */
	if (!scols_table_is_maxout(tb) && is_last_column(cl)) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return;
	}

	/* fill rest of cell with space */
	for(; len_pad < cl->width; ++len_pad)
		fputs(cellpadding_symbol(tb), tb->out);

	fputs_color_cell_close(tb, cl, ln, ce);

	if (!is_last_column(cl))
		fputs(colsep(tb), tb->out);
}



/* Fill the start of a line with padding (or with tree ascii-art).
 *
 * This is necessary after a long non-truncated column, as this requires the
 * next column to be printed on the next line. For example (see 'DDD'):
 *
 * aaa bbb ccc ddd eee
 * AAA BBB CCCCCCC
 *             DDD EEE
 * ^^^^^^^^^^^^
 *  new line padding
 */
static void print_newline_padding(struct libscols_table *tb,
				  struct libscols_column *cl,
				  struct libscols_line *ln,	/* optional */
				  struct libscols_cell *ce,
				  size_t bufsz)
{
	size_t i;

	assert(tb);
	assert(cl);

	DBG(LINE, ul_debugobj(ln, "printing newline padding"));

	fputs(linesep(tb), tb->out);		/* line break */
	tb->termlines_used++;

	fputs_color_line_open(tb, ln);

	/* fill cells after line break */
	for (i = 0; i <= (size_t) cl->seqnum; i++)
		print_empty_cell(tb, scols_table_get_column(tb, i), ln, ce, bufsz);

	fputs_color_line_close(tb);
}

static int print_pending_data(struct libscols_table *tb, struct ul_buffer *buf)
{
	struct libscols_line *ln;
	struct libscols_column *cl;
	struct libscols_cell *ce;
	char *data;
	size_t i, width = 0, len = 0, bytes = 0;

	scols_table_get_cursor(tb, &ln, &cl, &ce);

	width = cl->width;
	if (!width)
		return -EINVAL;

	DBG(COL, ul_debugobj(cl, "printing pending data"));

	if (scols_table_is_noencoding(tb))
		data = ul_buffer_get_data(buf, &bytes, &len);
	else
		data = ul_buffer_get_safe_data(buf, &bytes, &len, scols_column_get_safechars(cl));

	if (!data)
		return 0;

	/* standard multi-line cell */
	if (len > width && scols_column_is_wrap(cl)
	    && !scols_column_is_customwrap(cl)) {

		len = width;
		bytes = mbs_truncate(data, &len);

		if (bytes != (size_t) -1 && bytes > 0)
			scols_column_move_wrap(cl, mbs_safe_decode_size(data));
	}

	fputs_color_cell_open(tb, cl, ln, ce);
	fputs(data, tb->out);

	/* minout -- don't fill */
	if (scols_table_is_minout(tb) && is_next_columns_empty(tb, cl, ln)) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return 0;
	}

	/* default -- fill except last column */
	if (!scols_table_is_maxout(tb) && is_last_column(cl)) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return 0;
	}

	/* fill rest of cell with space */
	for(i = len; i < width; i++)
		fputs(cellpadding_symbol(tb), tb->out);

	fputs_color_cell_close(tb, cl, ln, ce);

	if (!is_last_column(cl))
		fputs(colsep(tb), tb->out);

	return 0;
}

static void print_json_data(struct libscols_table *tb,
			    struct libscols_column *cl,
			    const char *name,
			    char *data)
{
	switch (cl->json_type) {
	case SCOLS_JSON_STRING:
		/* name: "aaa" */
		ul_jsonwrt_value_s(&tb->json, name, data);
		break;
	case SCOLS_JSON_NUMBER:
	case SCOLS_JSON_FLOAT:
		/* name: 123 */
		ul_jsonwrt_value_raw(&tb->json, name, data);
		break;
	case SCOLS_JSON_BOOLEAN:
	case SCOLS_JSON_BOOLEAN_OPTIONAL:
		/* name: true|false|null */
		if (cl->json_type == SCOLS_JSON_BOOLEAN_OPTIONAL && (!*data || !strcmp(data, "-"))) {
			ul_jsonwrt_value_null(&tb->json, name);
		} else {
			ul_jsonwrt_value_boolean(&tb->json, name,
					!*data ? 0 :
					*data == '0' ? 0 :
					*data == 'N' || *data == 'n' ? 0 : 1);
		}
		break;
	case SCOLS_JSON_ARRAY_STRING:
	case SCOLS_JSON_ARRAY_NUMBER:
		/* name: [ "aaa", "bbb", "ccc" ] */
		ul_jsonwrt_array_open(&tb->json, name);

		if (!scols_column_is_customwrap(cl))
			ul_jsonwrt_value_s(&tb->json, NULL, data);
		else do {
			if (cl->json_type == SCOLS_JSON_ARRAY_STRING)
				ul_jsonwrt_value_s(&tb->json, NULL, data);
			else
				ul_jsonwrt_value_raw(&tb->json, NULL, data);
		} while (scols_column_next_wrap(cl, NULL, &data) == 0);

		ul_jsonwrt_array_close(&tb->json);
		break;
	}
}

static int print_data(struct libscols_table *tb, struct ul_buffer *buf)
{
	struct libscols_line *ln;	/* NULL for header line! */
	struct libscols_column *cl;
	struct libscols_cell *ce;
	size_t len = 0, i, width, bytes;
	char *data = NULL;
	const char *name = NULL;
	int is_last;

	assert(tb);

	scols_table_get_cursor(tb, &ln, &cl, &ce);
	assert(cl);

	if (tb->format != SCOLS_FMT_HUMAN) {
		name = scols_table_is_shellvar(tb) ?
				scols_column_get_name_as_shellvar(cl) :
				scols_column_get_name(cl);

		data = ul_buffer_get_data(buf, NULL, NULL);
		if (!data)
			data = "";
	}

	is_last = is_last_column(cl);

	if (ln && is_last && scols_table_is_json(tb) &&
	    scols_table_is_tree(tb) && has_children(ln))
		/* "children": [] is the real last value */
		is_last = 0;

	switch (tb->format) {
	case SCOLS_FMT_RAW:
		fputs_nonblank(data, tb->out);
		if (!is_last)
			fputs(colsep(tb), tb->out);
		return 0;

	case SCOLS_FMT_EXPORT:
		fputs(name ?: "", tb->out);
		fputc('=', tb->out);
		fputs_quoted(data, tb->out);
		if (!is_last)
			fputs(colsep(tb), tb->out);
		return 0;

	case SCOLS_FMT_JSON:
		print_json_data(tb, cl, name, data);
		return 0;

	case SCOLS_FMT_HUMAN:
		break;		/* continue below */
	}

	/* Encode. Note that 'len' and 'width' are number of glyphs not bytes.
	 */
	if (scols_table_is_noencoding(tb))
		data = ul_buffer_get_data(buf, &bytes, &len);
	else
		data = ul_buffer_get_safe_data(buf, &bytes, &len, scols_column_get_safechars(cl));

	if (!data)
		data = "";
	width = cl->width;

	if (is_last
	    && len < width
	    && !scols_table_is_maxout(tb)
	    && !scols_column_is_right(cl))
		width = len;

	/* truncate data */
	if (len > width && scols_column_is_trunc(cl)) {
		len = width;
		bytes = mbs_truncate(data, &len);	/* updates 'len' */
	}

	/* standard multi-line cell */
	if (len > width && scols_column_is_wrap(cl)
	    && !scols_column_is_customwrap(cl)) {

		len = width;
		bytes = mbs_truncate(data, &len);

		if (bytes != (size_t) -1 && bytes > 0)
			scols_column_move_wrap(cl, mbs_safe_decode_size(data));
	}

	if (bytes == (size_t) -1) {
		bytes = len = 0;
		data = NULL;
	}

	fputs_color_cell_open(tb, cl, ln, ce);

	if (data && *data) {
		if (scols_column_is_right(cl)) {
			for (i = len; i < width; i++)
				fputs(cellpadding_symbol(tb), tb->out);
			len = width;
		}
		fputs(data, tb->out);
	}

	/* minout -- don't fill */
	if (scols_table_is_minout(tb) && is_next_columns_empty(tb, cl, ln)) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return 0;
	}

	/* default -- fill except last column */
	if (!scols_table_is_maxout(tb) && is_last) {
		fputs_color_cell_close(tb, cl, ln, ce);
		return 0;
	}

	/* fill rest of cell with space */
	for(i = len; i < width; i++)
		fputs(cellpadding_symbol(tb), tb->out);

	fputs_color_cell_close(tb, cl, ln, ce);

	if (len > width && !scols_column_is_trunc(cl)) {
		DBG(COL, ul_debugobj(cl, "*** data len=%zu > column width=%zu", len, width));
		print_newline_padding(tb, cl, ln, ce, ul_buffer_get_bufsiz(buf));	/* next column starts on next line */

	} else if (!is_last)
		fputs(colsep(tb), tb->out);		/* columns separator */

	return 0;
}

/*
 * Copy current cell data to buffer. The @cal means "calculation" phase.
 */
int __cursor_to_buffer(struct libscols_table *tb,
		     struct ul_buffer *buf,
		     int cal)
{
	const char *data = NULL;
	size_t datasiz = 0;
	struct libscols_cell *ce;
	struct libscols_line *ln;
	struct libscols_column *cl;
	int rc = 0;

	assert(tb);

	scols_table_get_cursor(tb, &ln, &cl, &ce);

	assert(ln);
	assert(cl);
	assert(buf);
	assert(cl->seqnum <= tb->ncols);

	ul_buffer_reset_data(buf);

	if (!scols_column_is_tree(cl))
		goto notree;

	/*
	 * Group stuff
	 */
	if (!scols_table_is_json(tb) && cl->is_groups)
		rc = groups_ascii_art_to_buffer(tb, ln, buf, 0);

	/*
	 * Tree stuff
	 */
	if (!rc && ln->parent && !scols_table_is_json(tb)) {
		rc = tree_ascii_art_to_buffer(tb, ln->parent, buf);

		if (!rc && is_last_child(ln))
			rc = ul_buffer_append_string(buf, right_symbol(tb));
		else if (!rc)
			rc = ul_buffer_append_string(buf, branch_symbol(tb));
	}

	if (!rc && (ln->parent || cl->is_groups) && !scols_table_is_json(tb))
		ul_buffer_save_pointer(buf, SCOLS_BUFPTR_TREEEND);
notree:
	if (!rc && ce) {
		int do_wrap = scols_column_is_wrap(cl);

		/* Disable multi-line cells for "raw" and "export" formats.
		 * JSON uses data wrapping to generate arrays */
		if (do_wrap && (tb->format == SCOLS_FMT_RAW ||
				tb->format == SCOLS_FMT_EXPORT))
			do_wrap = 0;

		/* Wrapping enabled; append the next chunk if cell data */
		if (do_wrap) {
			char *x = NULL;

			rc = cal ? scols_column_greatest_wrap(cl, ce, &x) :
				   scols_column_next_wrap(cl, ce, &x);
			/* rc: error: <0; nodata: 1; success: 0 */
			if (rc < 0)
				goto done;
			data = x;
			rc = 0;
			if (data && *data)
				datasiz = strlen(data);
			if (data && datasiz)
				rc = ul_buffer_append_data(buf, data, datasiz);

		/* Wrapping disabled, but data maintained by custom wrapping
		 * callback. Try to use data as a string, if not possible,
		 * append all chunks separated by \n (backward compatibility).
		 * */
		} else if (scols_column_is_customwrap(cl)) {
			size_t len;
			int i = 0;
			char *x = NULL;

			data = scols_cell_get_data(ce);
			datasiz = scols_cell_get_datasiz(ce);
			len = data ? strnlen(data, datasiz) : 0;

			if (len && len + 1 == datasiz)
				rc = ul_buffer_append_data(buf, data, datasiz);

			else while (scols_column_next_wrap(cl, ce, &x) == 0) {
				/* non-string data in cell, use a nextchunk callback */
				if (!x)
					continue;
				datasiz = strlen(x);
				if (i)
					rc = ul_buffer_append_data(buf, "\n", 1);
				if (!rc)
					rc = ul_buffer_append_data(buf, x, datasiz);
				i++;
			}

		/* Wrapping disabled; let's use data as a classic string. */
		} else {
			data = scols_cell_get_data(ce);
			datasiz = scols_cell_get_datasiz(ce);

			if (data && *data && !datasiz)
				datasiz = strlen(data);		/* cell content may be updated */

			if (data && datasiz)
				rc = ul_buffer_append_data(buf, data, datasiz);
		}
	}

done:
	/* reset wrapping after greatest chunk calculation */
	if (cal && scols_column_is_wrap(cl))
		scols_column_reset_wrap(cl);

	DBG(COL, ul_debugobj(cl, "__cursor_to_buffer rc=%d", rc));
	return rc;
}

/*
 * Prints data. Data can be printed in more formats (raw, NAME=xxx pairs), and
 * control and non-printable characters can be encoded in the \x?? encoding.
 */
static int print_line(struct libscols_table *tb,
		      struct libscols_line *ln,
		      struct ul_buffer *buf)
{
	int rc = 0, pending = 0;
	struct libscols_column *cl;
	struct libscols_iter itr;

	assert(ln);

	DBG(LINE, ul_debugobj(ln, "     printing line"));

	fputs_color_line_open(tb, ln);

	/* regular line */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);

	while (rc == 0 && scols_table_next_column(tb, &itr, &cl) == 0) {
		if (scols_column_is_hidden(cl))
			continue;

		scols_table_set_cursor(tb, ln, cl, scols_line_get_cell(ln, cl->seqnum));
		rc = __cursor_to_buffer(tb, buf, 0);
		if (!rc)
			rc = print_data(tb, buf);
		if (!rc) {
			if (scols_column_has_pending_wrap(cl))
				pending = 1;
			else
				scols_column_reset_wrap(cl);
		}
		scols_table_reset_cursor(tb);
	}
	fputs_color_line_close(tb);

	/* extra lines of the multi-line cells */
	while (rc == 0 && pending) {
		DBG(LINE, ul_debugobj(ln, "printing pending data"));
		pending = 0;
		fputs(linesep(tb), tb->out);
		fputs_color_line_open(tb, ln);
		tb->termlines_used++;

		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);

		while (rc == 0 && scols_table_next_column(tb, &itr, &cl) == 0) {
			if (scols_column_is_hidden(cl))
				continue;

			scols_table_set_cursor(tb, ln, cl, scols_line_get_cell(ln, cl->seqnum));
			if (scols_column_has_pending_wrap(cl)) {
				rc = __cursor_to_buffer(tb, buf, 0);
				if (!rc)
					rc = print_pending_data(tb, buf);
				if (!rc) {
					if (scols_column_has_pending_wrap(cl))
						pending = 1;
					else
						scols_column_reset_wrap(cl);
				}
			} else
				print_empty_cell(tb, cl, ln, NULL, ul_buffer_get_bufsiz(buf));
			scols_table_reset_cursor(tb);
		}
		fputs_color_line_close(tb);
	}

	return 0;
}

int __scols_print_title(struct libscols_table *tb)
{
	int rc;
	mbs_align_t align;
	size_t width, len = 0, bufsz, titlesz;
	char *title = NULL, *buf = NULL;

	assert(tb);

	if (!tb->title.data)
		return 0;

	DBG(TAB, ul_debugobj(tb, "printing title"));

	/* encode data */
	if (tb->no_encode) {
		len = bufsz = strlen(tb->title.data) + 1;
		buf = strdup(tb->title.data);
		if (!buf) {
			rc = -ENOMEM;
			goto done;
		}
	} else {
		bufsz = mbs_safe_encode_size(strlen(tb->title.data)) + 1;
		if (bufsz == 1) {
			DBG(TAB, ul_debugobj(tb, "title is empty string -- ignore"));
			return 0;
		}
		buf = malloc(bufsz);
		if (!buf) {
			rc = -ENOMEM;
			goto done;
		}

		if (!mbs_safe_encode_to_buffer(tb->title.data, &len, buf, NULL) ||
		    !len || len == (size_t) -1) {
			rc = -EINVAL;
			goto done;
		}
	}

	/* truncate and align */
	width = tb->is_term ? tb->termwidth : 80;
	titlesz = width + bufsz;

	title = malloc(titlesz);
	if (!title) {
		rc = -EINVAL;
		goto done;
	}

	switch (scols_cell_get_alignment(&tb->title)) {
	case SCOLS_CELL_FL_RIGHT:
		align = MBS_ALIGN_RIGHT;
		break;
	case SCOLS_CELL_FL_CENTER:
		align = MBS_ALIGN_CENTER;
		break;
	case SCOLS_CELL_FL_LEFT:
	default:
		align = MBS_ALIGN_LEFT;
		/*
		 * Don't print extra blank chars after the title if on left
		 * (that's same as we use for the last column in the table).
		 */
		if (len < width
		    && !scols_table_is_maxout(tb)
		    && isblank(*titlepadding_symbol(tb)))
			width = len;
		break;

	}

	/* copy from buf to title and align to width with title_padding */
	rc = mbsalign_with_padding(buf, title, titlesz,
			&width, align,
			0, (int) *titlepadding_symbol(tb));

	if (rc == -1) {
		rc = -EINVAL;
		goto done;
	}


	if (tb->colors_wanted)
		fputs_color(tb, tb->title.color);

	fputs(title, tb->out);

	if (tb->colors_wanted)
		fputs_color_reset(tb);

	fputc('\n', tb->out);
	rc = 0;
done:
	free(buf);
	free(title);
	DBG(TAB, ul_debugobj(tb, "printing title done [rc=%d]", rc));
	return rc;
}

int __scols_print_header(struct libscols_table *tb, struct ul_buffer *buf)
{
	int rc = 0;
	struct libscols_column *cl;
	struct libscols_iter itr;

	assert(tb);

	if ((tb->header_printed == 1 && tb->header_repeat == 0) ||
	    scols_table_is_noheadings(tb) ||
	    scols_table_is_export(tb) ||
	    scols_table_is_json(tb) ||
	    list_empty(&tb->tb_lines))
		return 0;

	DBG(TAB, ul_debugobj(tb, "printing header"));

	/* set the width according to the size of the data */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (rc == 0 && scols_table_next_column(tb, &itr, &cl) == 0) {
		if (scols_column_is_hidden(cl))
			continue;

		ul_buffer_reset_data(buf);
		scols_table_set_cursor(tb, NULL, cl, &cl->header);

		if (cl->is_groups
		    && scols_table_is_tree(tb) && scols_column_is_tree(cl)) {
			size_t i;
			for (i = 0; i < tb->grpset_size + 1; i++) {
				rc = ul_buffer_append_data(buf, " ", 1);
				if (rc)
					break;
			}
		}
		if (!rc)
			rc = ul_buffer_append_string(buf,
					scols_table_is_shellvar(tb) ?
						scols_column_get_name_as_shellvar(cl) :
						scols_column_get_name(cl));
		if (!rc)
			rc = print_data(tb, buf);
		scols_table_reset_cursor(tb);
	}

	if (rc == 0) {
		fputs(linesep(tb), tb->out);
		tb->termlines_used++;
	}

	tb->header_printed = 1;
	tb->header_next = tb->termlines_used + tb->termheight;
	if (tb->header_repeat)
		DBG(TAB, ul_debugobj(tb, "\tnext header: %zu [current=%zu, rc=%d]",
					tb->header_next, tb->termlines_used, rc));
	return rc;
}


int __scols_print_range(struct libscols_table *tb,
			struct ul_buffer *buf,
			struct libscols_iter *itr,
			struct libscols_line *end)
{
	int rc = 0;
	struct libscols_line *ln;

	assert(tb);
	DBG(TAB, ul_debugobj(tb, "printing range"));

	while (rc == 0 && scols_table_next_line(tb, itr, &ln) == 0) {

		int last = scols_iter_is_last(itr);

		if (scols_table_is_json(tb))
			ul_jsonwrt_object_open(&tb->json, NULL);

		rc = print_line(tb, ln, buf);

		if (scols_table_is_json(tb))
			ul_jsonwrt_object_close(&tb->json);
		else if (last == 0 && tb->no_linesep == 0) {
			fputs(linesep(tb), tb->out);
			tb->termlines_used++;
		}

		if (end && ln == end)
			break;

		if (!last && want_repeat_header(tb))
			__scols_print_header(tb, buf);
	}

	return rc;

}

int __scols_print_table(struct libscols_table *tb, struct ul_buffer *buf)
{
	struct libscols_iter itr;

	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	return __scols_print_range(tb, buf, &itr, NULL);
}

/* scols_walk_tree() callback to print tree line */
static int print_tree_line(struct libscols_table *tb,
			   struct libscols_line *ln,
			   struct libscols_column *cl __attribute__((__unused__)),
			   void *data)
{
	struct ul_buffer *buf = (struct ul_buffer *) data;
	int rc;

	DBG(LINE, ul_debugobj(ln, "   printing tree line"));

	if (scols_table_is_json(tb))
		ul_jsonwrt_object_open(&tb->json, NULL);

	rc = print_line(tb, ln, buf);
	if (rc)
		return rc;

	if (has_children(ln)) {
		if (scols_table_is_json(tb))
			ul_jsonwrt_array_open(&tb->json, "children");
		else {
			/* between parent and child is separator */
			fputs(linesep(tb), tb->out);
			tb->termlines_used++;
		}
	} else {
		int last;

		/* terminate all open last children for JSON */
		if (scols_table_is_json(tb)) {
			do {
				last = (is_child(ln) && is_last_child(ln)) ||
				       (is_tree_root(ln) && is_last_tree_root(tb, ln));

				ul_jsonwrt_object_close(&tb->json);
				if (last && is_child(ln))
					ul_jsonwrt_array_close(&tb->json);
				ln = ln->parent;
			} while(ln && last);

		} else if (tb->no_linesep == 0) {
			int last_in_tree = scols_walk_is_last(tb, ln);

			if (last_in_tree == 0) {
				/* standard output */
				fputs(linesep(tb), tb->out);
				tb->termlines_used++;
			}
		}
	}

	return 0;
}

int __scols_print_tree(struct libscols_table *tb, struct ul_buffer *buf)
{
	assert(tb);
	DBG(TAB, ul_debugobj(tb, "----printing-tree-----"));

	return scols_walk_tree(tb, NULL, print_tree_line, (void *) buf);
}

static size_t strlen_line(struct libscols_line *ln)
{
	size_t i, sz = 0;

	assert(ln);

	for (i = 0; i < ln->ncells; i++) {
		struct libscols_cell *ce = scols_line_get_cell(ln, i);
		const char *data = ce ? scols_cell_get_data(ce) : NULL;

		sz += data ? strlen(data) : 0;
	}

	return sz;
}

void __scols_cleanup_printing(struct libscols_table *tb, struct ul_buffer *buf)
{
	if (!tb)
		return;

	ul_buffer_free_data(buf);

	if (tb->priv_symbols) {
		scols_table_set_symbols(tb, NULL);
		tb->priv_symbols = 0;
	}
}

int __scols_initialize_printing(struct libscols_table *tb, struct ul_buffer *buf)
{
	size_t bufsz, extra_bufsz = 0;
	struct libscols_line *ln;
	struct libscols_iter itr;
	int rc;

	DBG(TAB, ul_debugobj(tb, "initialize printing"));

	if (!tb->symbols) {
		rc = scols_table_set_default_symbols(tb);
		if (rc)
			goto err;
		tb->priv_symbols = 1;
	} else
		tb->priv_symbols = 0;

	if (tb->format == SCOLS_FMT_HUMAN)
		tb->is_term = tb->termforce == SCOLS_TERMFORCE_NEVER  ? 0 :
			      tb->termforce == SCOLS_TERMFORCE_ALWAYS ? 1 :
			      isatty(STDOUT_FILENO);

	if (tb->is_term) {
		size_t width = (size_t) scols_table_get_termwidth(tb);

		if (tb->termreduce > 0 && tb->termreduce < width) {
			width -= tb->termreduce;
			scols_table_set_termwidth(tb, width);
		}
		bufsz = width;
	} else
		bufsz = BUFSIZ;

	if (!tb->is_term || tb->format != SCOLS_FMT_HUMAN || scols_table_is_tree(tb))
		tb->header_repeat = 0;

	/*
	 * Estimate extra space necessary for tree, JSON or another output
	 * decoration.
	 */
	if (scols_table_is_tree(tb))
		extra_bufsz += tb->nlines * strlen(vertical_symbol(tb));

	switch (tb->format) {
	case SCOLS_FMT_RAW:
		extra_bufsz += tb->ncols;			/* separator between columns */
		break;
	case SCOLS_FMT_JSON:
		ul_jsonwrt_init(&tb->json, tb->out, 0);
		extra_bufsz += tb->nlines * 3;		/* indentation */
		/* fallthrough */
	case SCOLS_FMT_EXPORT:
	{
		struct libscols_column *cl;

		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);

		while (scols_table_next_column(tb, &itr, &cl) == 0) {
			if (scols_column_is_hidden(cl))
				continue;

			if (scols_column_get_name(cl))
				extra_bufsz += strlen(scols_column_get_name(cl));	/* data */
			extra_bufsz += 2;						/* separators */
		}
		break;
	}
	case SCOLS_FMT_HUMAN:
		break;
	}

	/*
	 * Enlarge buffer if necessary, the buffer should be large enough to
	 * store line data and tree ascii art (or another decoration).
	 */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_line(tb, &itr, &ln) == 0) {
		size_t sz;

		sz = strlen_line(ln) + extra_bufsz;
		if (sz > bufsz)
			bufsz = sz;
	}

	/* pre-allocate space for data */
	rc = ul_buffer_alloc_data(buf, bufsz + 1);	/* data + space for \0 */
	if (rc)
		goto err;

	/*
	 * Make sure groups members are in the same orders as the tree
	 */
	if (has_groups(tb) && scols_table_is_tree(tb))
		scols_groups_fix_members_order(tb);

	if (tb->format == SCOLS_FMT_HUMAN) {
		rc = __scols_calculate(tb, buf);
		if (rc != 0)
			goto err;
	}

	return 0;
err:
	__scols_cleanup_printing(tb, buf);
	return rc;
}

