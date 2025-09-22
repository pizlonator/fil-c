/*
 * column.c - functions for table handling at the column level
 *
 * Copyright (C) 2014 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: column
 * @title: Column
 * @short_description: defines output columns formats, headers, etc.
 *
 * An API to access and modify per-column data and information.
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "mbsalign.h"
#include "strutils.h"
#include "smartcolsP.h"

/**
 * scols_new_column:
 *
 * Allocates space for a new column.
 *
 * Returns: a pointer to a new struct libscols_column instance, NULL in case of an ENOMEM error.
 */
struct libscols_column *scols_new_column(void)
{
	struct libscols_column *cl;

	cl = calloc(1, sizeof(*cl));
	if (!cl)
		return NULL;
	DBG(COL, ul_debugobj(cl, "alloc"));
	cl->refcount = 1;
	INIT_LIST_HEAD(&cl->cl_columns);
	return cl;
}

/**
 * scols_ref_column:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Increases the refcount of @cl.
 */
void scols_ref_column(struct libscols_column *cl)
{
	if (cl)
		cl->refcount++;
}

/**
 * scols_unref_column:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Decreases the refcount of @cl. When the count falls to zero, the instance
 * is automatically deallocated.
 */
void scols_unref_column(struct libscols_column *cl)
{
	if (cl && --cl->refcount <= 0) {
		DBG(COL, ul_debugobj(cl, "dealloc"));
		list_del(&cl->cl_columns);
		scols_reset_cell(&cl->header);
		free(cl->color);
		free(cl->safechars);
		free(cl->wrap_data);
		free(cl->shellvar);
		free(cl);
	}
}

/**
 * scols_copy_column:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Creates a new column and copies @cl's data over to it.
 *
 * Returns: a pointer to a new struct libscols_column instance.
 */
struct libscols_column *scols_copy_column(const struct libscols_column *cl)
{
	struct libscols_column *ret;

	if (!cl)
		return NULL;
	ret = scols_new_column();
	if (!ret)
		return NULL;

	DBG(COL, ul_debugobj(cl, "copy"));

	if (scols_column_set_color(ret, cl->color))
		goto err;
	if (scols_cell_copy_content(&ret->header, &cl->header))
		goto err;

	ret->width	= cl->width;
	ret->width_hint	= cl->width_hint;
	ret->flags	= cl->flags;
	ret->is_groups  = cl->is_groups;

	memcpy(&ret->wstat, &cl->wstat, sizeof(cl->wstat));

	return ret;
err:
	scols_unref_column(ret);
	return NULL;
}

/**
 * scols_column_set_whint:
 * @cl: a pointer to a struct libscols_column instance
 * @whint: a width hint
 *
 * Sets the width hint of column @cl to @whint. See scols_table_new_column().
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_column_set_whint(struct libscols_column *cl, double whint)
{
	if (!cl)
		return -EINVAL;

	cl->width_hint = whint;
	return 0;
}

/**
 * scols_column_get_whint:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: The width hint of column @cl, a negative value in case of an error.
 */
double scols_column_get_whint(const struct libscols_column *cl)
{
	return cl->width_hint;
}

/**
 * scols_column_set_flags:
 * @cl: a pointer to a struct libscols_column instance
 * @flags: a flag mask
 *
 * Sets the flags of @cl to @flags.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_column_set_flags(struct libscols_column *cl, int flags)
{
	if (!cl)
		return -EINVAL;

	if (cl->table) {
		if (!(cl->flags & SCOLS_FL_TREE) && (flags & SCOLS_FL_TREE))
			cl->table->ntreecols++;
		else if ((cl->flags & SCOLS_FL_TREE) && !(flags & SCOLS_FL_TREE))
			cl->table->ntreecols--;
	}

	DBG(COL, ul_debugobj(cl, "setting flags from 0x%04x to 0x%04x", cl->flags, flags));
	cl->flags = flags;
	return 0;
}

/**
 * scols_column_set_json_type:
 * @cl: a pointer to a struct libscols_column instance
 * @type: SCOLS_JSON_* type
 *
 * Sets the type used for JSON formatting, the default is SCOLS_JSON_STRING.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.33
 */
int scols_column_set_json_type(struct libscols_column *cl, int type)
{
	if (!cl)
		return -EINVAL;

	cl->json_type = type;
	return 0;

}

/**
 * scols_column_get_json_type:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Note that SCOLS_JSON_BOOLEAN interprets NULL, empty strings, '0', 'N' and
 * 'n' as "false"; and everything else as "true".
 *
 * Returns: JSON type used for formatting or a negative value in case of an error.
 *
 * Since: 2.33
 */
int scols_column_get_json_type(const struct libscols_column *cl)
{
	return cl ? cl->json_type : -EINVAL;
}


/**
 * scols_column_set_data_type:
 * @cl: a pointer to a struct libscols_column instance
 * @type: SCOLS_DATA_*
 *
 * The table always keep data in strings in form that is printed on output, but
 * for some internal operations (like filters or counters) it needs to convert
 * the strings to usable data format. This data format is possible to specify,
 * by this function. If the format is not specified then filter and counters
 * try to use SCOLS_JSON_* types, if also not define than defaults to string.
 *
 * If a simple string conversion is not possible then application (which want
 * to use filters and counters) needs to define data function to do the
 * conversion. See scols_column_set_data_func().
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.40
 */
int scols_column_set_data_type(struct libscols_column *cl, int type)
{
	return cl->data_type = type;
}

/**
 * scols_column_get_data_type:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: The current datatype setting of the column @cl.
 *
 * Since: 2.40
 */
int scols_column_get_data_type(const struct libscols_column *cl)
{
	return cl->data_type;
}
/**
 * scols_column_get_table:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: pointer to the table where columns is used
 */
struct libscols_table *scols_column_get_table(const struct libscols_column *cl)
{
	return cl->table;
}

/**
 * scols_column_get_flags:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: The flag mask of @cl, a negative value in case of an error.
 */
int scols_column_get_flags(const struct libscols_column *cl)
{
	return cl->flags;
}

/**
 * scols_column_get_header:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: A pointer to a struct libscols_cell instance, representing the
 * header info of column @cl or NULL in case of an error.
 */
struct libscols_cell *scols_column_get_header(struct libscols_column *cl)
{
	return &cl->header;
}

/**
 * scols_column_set_name:
 * @cl: a pointer to a struct libscols_column instance
 * @name: column name
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.38
 */
int scols_column_set_name(struct libscols_column *cl, const char *name)
{
	struct libscols_cell *hr = scols_column_get_header(cl);

	if (!hr)
		return -EINVAL;

	free(cl->shellvar);
	cl->shellvar = NULL;

	return scols_cell_set_data(hr, name);
}

/**
 * scols_column_get_name:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: A pointer to a column name, which is stored in column header
 *
 * Since: 2.38
 */
const char *scols_column_get_name(struct libscols_column *cl)
{
	return scols_cell_get_data(&cl->header);
}

/**
 * scols_shellvar_name:
 * @name: raw (column) name
 * @buf: buffer to returns normalized name
 * @bufsz: size of the buffer
 *
 * Converts @name to a name compatible with shell. The buffer is reallocated if
 * not large enough.
 *
 * Returns: 0 in case of conversion, 1 if conversion unnecessary, <0 on error.
 *
 * Since: 2.40
 */
int scols_shellvar_name(const char *name, char **buf, size_t *bufsz)
{
	char *p;
	const char *s;
	size_t sz;

	if (!name || !*name || !buf || !bufsz)
		return -EINVAL;

	/* size to convert "1FOO%" --> "_1FOO_PCT */
	sz = strlen(name) + 1 + 3;
	if (sz + 1 > *bufsz) {
		char *tmp;

		*bufsz = sz + 1;
		tmp = realloc(*buf, *bufsz);
		if (!tmp)
			return -ENOMEM;
		*buf = tmp;
	}
	memset(*buf, 0, *bufsz);
	p = *buf;

	 /* convert "1FOO" to "_1FOO" */
	if (!isalpha(*name))
		*p++ = '_';

	/* replace all "bad" chars with "_" */
	for (s = name; *s; s++)
		*p++ = !isalnum(*s) ? '_' : *s;

	if (!*s && *(s - 1) == '%') {
		*p++ = 'P';
		*p++ = 'C';
		*p++ = 'T';
	}

	return strcmp(name, *buf) == 0;
}

/**
 * scols_column_get_name_as_shellvar
 * @cl: a pointer to a struct libscols_column instance
 *
 * Like scols_column_get_name(), but column name is modified to be compatible with shells
 * requirements for variable names.
 *
 * Since: 2.38
 */
const char *scols_column_get_name_as_shellvar(struct libscols_column *cl)
{
	if (!cl->shellvar) {
		const char *name = scols_column_get_name(cl);
		size_t sz = 0;

		if (!name || !*name)
			return NULL;
		if (scols_shellvar_name(name, &cl->shellvar, &sz) < 0)
			return NULL;
	}
	return cl->shellvar;
}


/**
 * scols_column_set_color:
 * @cl: a pointer to a struct libscols_column instance
 * @color: color name or ESC sequence
 *
 * The default color for data cells and column header.
 *
 * If you want to set header specific color then use scols_column_get_header()
 * and scols_cell_set_color().
 *
 * If you want to set data cell specific color the use scols_line_get_cell() +
 * scols_cell_set_color().
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_column_set_color(struct libscols_column *cl, const char *color)
{
	if (color && !color_is_sequence(color)) {
		char *seq = color_get_sequence(color);
		if (!seq)
			return -EINVAL;
		free(cl->color);
		cl->color = seq;
		return 0;
	}
	return strdup_to_struct_member(cl, color, color);
}

/**
 * scols_column_get_color:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: The current color setting of the column @cl.
 */
const char *scols_column_get_color(const struct libscols_column *cl)
{
	return cl->color;
}


/**
 * scols_wrapnl_nextchunk:
 * @cl: a pointer to a struct libscols_column instance
 * @data: string
 * @userdata: callback private data
 *
 * This is built-in function for scols_column_set_wrapfunc(). This function
 * terminates the current chunk by \0 and returns pointer to the begin of
 * the next chunk. The chunks are based on \n.
 *
 * For example for data "AAA\nBBB\nCCC" the next chunk is "BBB".
 *
 * Returns: next chunk
 *
 * Since: 2.29
 */
char *scols_wrapnl_nextchunk(const struct libscols_column *cl __attribute__((unused)),
			char *data,
			void *userdata __attribute__((unused)))
{
	char *p = data ? strchr(data, '\n') : NULL;

	if (p) {
		*p = '\0';
		return p + 1;
	}
	return NULL;
}

/**
 * scols_wrapzero_nextchunk:
 * @cl: a pointer to a struct libscols_column instance
 * @data: string
 * @userdata: callback private data
 *
 * This is built-in function for scols_column_set_wrapfunc(). This function
 * walk string separated by \0.
 *
 * For example for data "AAA\0BBB\0CCC\0" the next chunk is "BBB".
 *
 * Returns: next chunk
 *
 * Since: 2.40
 */
char *scols_wrapzero_nextchunk(const struct libscols_column *cl,
			char *data,
			void *userdata __attribute__((unused)))
{
	char *start = NULL;
	size_t sz = 0;

	if (!data)
		return NULL;
	scols_column_get_wrap_data(cl, &start, &sz, NULL, NULL);
	if (!start || !sz)
		return NULL;
	return ul_next_string(data, start + sz);
}


/**
 * scols_wrapnl_chunksize:
 * @cl: a pointer to a struct libscols_column instance
 * @data: string
 * @userdata: callback private data
 *
 * Analyzes @data and returns size of the largest chunk. The chunks are based
 * on \n. For example for data "AAA\nBBB\nCCCC" the largest chunk size is 4.
 *
 * Note that the size has to be based on number of terminal cells rather than
 * bytes to support multu-byte output.
 *
 * Deprecated since 2.40.
 *
 * Returns: size of the largest chunk.
 *
 * Since: 2.29
 */
size_t scols_wrapnl_chunksize(const struct libscols_column *cl __attribute__((unused)),
		const char *data,
		void *userdata __attribute__((unused)))
{
	size_t sum = 0;

	while (data && *data) {
		const char *p;
		size_t sz;

		p = strchr(data, '\n');
		if (p) {
			sz = cl->table && scols_table_is_noencoding(cl->table) ?
					mbs_nwidth(data, p - data) :
					mbs_safe_nwidth(data, p - data, NULL);
			p++;
		} else {
			sz = cl->table && scols_table_is_noencoding(cl->table) ?
					mbs_width(data) :
					mbs_safe_width(data);
		}
		sum = max(sum, sz);
		data = p;
	}

	return sum;
}

/**
 * scols_column_set_cmpfunc:
 * @cl: column
 * @cmp: pointer to compare function
 * @data: private data for cmp function
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_column_set_cmpfunc(struct libscols_column *cl,
			int (*cmp)(struct libscols_cell *,
				   struct libscols_cell *,
				   void *),
			void *data)
{
	if (!cl)
		return -EINVAL;

	cl->cmpfunc = cmp;
	cl->cmpfunc_data = data;
	return 0;
}

/**
 * scols_column_set_wrapfunc:
 * @cl: a pointer to a struct libscols_column instance
 * @wrap_chunksize: function to return size of the largest chink of data (deprecated)
 * @wrap_nextchunk: function to return next zero terminated data
 * @userdata: optional stuff for callbacks
 *
 * Extends SCOLS_FL_WRAP and can be used to set custom wrap function. The default
 * is to wrap by column size, but you can create functions to wrap for example
 * after \n or after words, etc.
 *
 * Note that since 2.40 the @wrap_chunksize is unnecessary. The library calculates
 * the size itself.
 *
 * The wrap functions do not work directly with cell data, but with buffer used
 * by library to compose output data. The wrap_nextchunk() function can access
 * additional details about wrap data by scols_column_get_wrap_data().
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.29
 */
int scols_column_set_wrapfunc(struct libscols_column *cl,
			size_t (*wrap_chunksize)(const struct libscols_column *,
						 const char *,
						 void *) __attribute__((__unused__)),
			char * (*wrap_nextchunk)(const struct libscols_column *,
						 char *,
						 void *),
			void *userdata)
{
	if (!cl)
		return -EINVAL;

	cl->wrap_nextchunk = wrap_nextchunk;
	cl->wrapfunc_data = userdata;
	return 0;
}

/**
 * scols_column_get_wrap_data:
 * @cl: column
 * @data: return wrap data
 * @datasiz: return wrap buffer size
 * @cur: the current pozition in the buffer
 * @next: the next pozition
 *
 * This function returns the current status of wrapping cell data (for multi-line cells).
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.40
 */
int scols_column_get_wrap_data(const struct libscols_column *cl,
		char **data, size_t *datasiz, char **cur, char **next)
{
	if (!cl)
		return -EINVAL;
	if (data)
		*data = cl->wrap_data;
	if (datasiz)
		*datasiz = cl->wrap_datasz;
	if (cur)
		*cur = cl->wrap_cur;
	if (next)
		*next = cl->wrap_next;
	return 0;
}

/*
 * scols_column_set_data_func:
 * @cl: a pointer to a struct libscols_column instance
 * @datafunc: function to return data
 * @userdata: optional stuff for callbacks
 *
 * The table always keep data in strings in form that is printed on output, but
 * for some internal operations (like filters or counters) it needs to convert
 * the strings to usable data format. If this converion is not possible then
 * application can define datafunc() callback to provide data for filters and counters.

 * The callback needs to return the data as pointer to void, and the data type
 * is defined by scols_column_set_data_type().
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.40
 */
int scols_column_set_data_func(struct libscols_column *cl,
			void *(*datafunc)(const struct libscols_column *,
					struct libscols_cell *,
					void *),
			void *userdata)
{
	if (!cl)
		return -EINVAL;

	cl->datafunc = datafunc;
	cl->datafunc_data = userdata;
	return 0;
}

/**
 * scols_column_has_data_func:
 * @cl: a pointer to a struct libscols_column instance
 *
 * See scols_column_set_data_func() for more details.
 *
 * Returns: 1 if data function defined, or 0
 *
 * Since: 2.40
 */
int scols_column_has_data_func(struct libscols_column *cl)
{
	return cl && cl->datafunc != NULL ? 1 : 0;
}

/**
 * scols_column_set_safechars:
 * @cl: a pointer to a struct libscols_column instance
 * @safe: safe characters (e.g. "\n\t")
 *
 * Use for bytes you don't want to encode on output. This is for example
 * necessary if you want to use custom wrap function based on \n, in this case
 * you have to set "\n" as a safe char.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.29
 */
int scols_column_set_safechars(struct libscols_column *cl, const char *safe)
{
	return strdup_to_struct_member(cl, safechars, safe);
}

/**
 * scols_column_get_safechars:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: safe chars
 *
 * Since: 2.29
 */
const char *scols_column_get_safechars(const struct libscols_column *cl)
{
	return cl->safechars;
}

/**
 * scols_column_get_width:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Important note: the column width is unknown until library starts printing
 * (width is calculated before printing). The function is usable for example in
 * nextchunk() callback specified by scols_column_set_wrapfunc().
 *
 * See also scols_column_get_whint(), it returns wanted size (!= final size).
 *
 * Returns: column width
 *
 * Since: 2.29
 */
size_t scols_column_get_width(const struct libscols_column *cl)
{
	return cl->width;
}

/**
 * scols_column_is_hidden:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag hidden.
 *
 * Returns: 0 or 1
 *
 * Since: 2.27
 */
int scols_column_is_hidden(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_HIDDEN ? 1 : 0;
}

/**
 * scols_column_is_trunc:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag trunc.
 *
 * Returns: 0 or 1
 */
int scols_column_is_trunc(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_TRUNC ? 1 : 0;
}
/**
 * scols_column_is_tree:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag tree.
 *
 * Returns: 0 or 1
 */
int scols_column_is_tree(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_TREE ? 1 : 0;
}
/**
 * scols_column_is_right:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag right.
 *
 * Returns: 0 or 1
 */
int scols_column_is_right(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_RIGHT ? 1 : 0;
}
/**
 * scols_column_is_strict_width:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag strict_width.
 *
 * Returns: 0 or 1
 */
int scols_column_is_strict_width(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_STRICTWIDTH ? 1 : 0;
}
/**
 * scols_column_is_noextremes:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag no_extremes.
 *
 * Returns: 0 or 1
 */
int scols_column_is_noextremes(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_NOEXTREMES ? 1 : 0;
}
/**
 * scols_column_is_wrap:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Gets the value of @cl's flag wrap.
 *
 * Returns: 0 or 1
 *
 * Since: 2.28
 */
int scols_column_is_wrap(const struct libscols_column *cl)
{
	return cl->flags & SCOLS_FL_WRAP ? 1 : 0;
}
/**
 * scols_column_is_customwrap:
 * @cl: a pointer to a struct libscols_column instance
 *
 * Returns: 0 or 1
 *
 * Since: 2.29
 */
int scols_column_is_customwrap(const struct libscols_column *cl)
{
	return (cl->flags & SCOLS_FL_WRAP)
		&& cl->wrap_nextchunk ? 1 : 0;
}

/**
 * scols_column_set_properties:
 * @cl: a pointer to a struct libscols_column instance
 * @opts: options string
 *
 * Set properties from string, the string is comma seprated list, like
 * "trunc,right,json=number", ...
 *
 * Returns: 0 on success, <0 on error
 *
 * Since: 2.39
 */
int scols_column_set_properties(struct libscols_column *cl, const char *opts)
{
	char *str = (char *) opts;
	char *name, *value;
	size_t namesz, valuesz;
	unsigned int flags = 0;
	int rc = 0;

	DBG(COL, ul_debugobj(cl, "apply properties '%s'", opts));

	while (rc == 0
	       && !ul_optstr_next(&str, &name, &namesz, &value, &valuesz)) {

		if (strncmp(name, "trunc", namesz) == 0)
			flags |= SCOLS_FL_TRUNC;

		else if (strncmp(name, "tree", namesz) == 0)
			flags |= SCOLS_FL_TREE;

		else if (strncmp(name, "right", namesz) == 0)
			flags |= SCOLS_FL_RIGHT;

		else if (strncmp(name, "strictwidth", namesz) == 0)
			flags |= SCOLS_FL_STRICTWIDTH;

		else if (strncmp(name, "noextremes", namesz) == 0)
			flags |= SCOLS_FL_NOEXTREMES;

		else if (strncmp(name, "hidden", namesz) == 0)
			flags |= SCOLS_FL_HIDDEN;

		else if (strncmp(name, "wrap", namesz) == 0)
			flags |= SCOLS_FL_WRAP;

		else if (strncmp(name, "wrapnl", namesz) == 0) {
			flags |= SCOLS_FL_WRAP;
			scols_column_set_wrapfunc(cl,
					NULL,
					scols_wrapnl_nextchunk,
					NULL);
			scols_column_set_safechars(cl, "\n");

		} else if (strncmp(name, "wrapzero", namesz) == 0) {
			flags |= SCOLS_FL_WRAP;
			scols_column_set_wrapfunc(cl,
					NULL,
					scols_wrapzero_nextchunk,
					NULL);

		} else if (value && strncmp(name, "json", namesz) == 0) {

			if (strncmp(value, "string", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_STRING);
			else if (strncmp(value, "number", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_NUMBER);
			else if (strncmp(value, "float", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_FLOAT);
			else if (strncmp(value, "array-string", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_ARRAY_STRING);
			else if (strncmp(value, "array-number", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_ARRAY_NUMBER);
			else if (strncmp(value, "boolean", valuesz) == 0)
				rc = scols_column_set_json_type(cl, SCOLS_JSON_BOOLEAN);

		} else if (value && strncmp(name, "width", namesz) == 0) {

			char *end = NULL;
			double x = strtod(value, &end);
			if (errno || value == end)
				return -EINVAL;
			rc = scols_column_set_whint(cl, x);

		} else if (value && strncmp(name, "color", namesz) == 0) {

			char *x = strndup(value, valuesz);
			if (x) {
				scols_column_set_color(cl, x);
				free(x);
			}

		} else if (value && strncmp(name, "name", namesz) == 0) {

			char *x = strndup(value, valuesz);
			if (x) {
				scols_column_set_name(cl, x);
				free(x);
			}
		}
	}

	if (!rc && flags)
		rc = scols_column_set_flags(cl, flags);

	return rc;
}

void scols_column_reset_wrap(struct libscols_column *cl)
{
	if (!cl)
		return;

	if (cl->wrap_data)
		memset(cl->wrap_data, 0, cl->wrap_datamax);
	cl->wrap_cell = NULL;
	cl->wrap_datasz = 0;
	cl->wrap_cur = NULL;
	cl->wrap_next = NULL;
}

static int scols_column_init_wrap(
			struct libscols_column *cl,
			struct libscols_cell *ce)
{
	const char *data = scols_cell_get_data(ce);

	if (!cl || !ce)
		return -EINVAL;

	assert(cl->table->cur_column == cl);
	assert(cl->table->cur_cell == ce);

	scols_column_reset_wrap(cl);

	cl->wrap_cell = ce;
	if (data) {
		void *tmp;
		cl->wrap_datasz = scols_cell_get_datasiz(ce);

		if (cl->wrap_datasz > cl->wrap_datamax) {
			cl->wrap_datamax = cl->wrap_datasz;
			tmp = realloc(cl->wrap_data, cl->wrap_datamax);
			if (!tmp)
				return -ENOMEM;
			cl->wrap_data = tmp;
		}
		memcpy(cl->wrap_data, data, cl->wrap_datasz);
		cl->wrap_cur = cl->wrap_data;
		cl->wrap_next = NULL;
	}

	return 0;
}

/* Returns the next chunk of cell data in multi-line cells */
int scols_column_next_wrap(
			struct libscols_column *cl,
			struct libscols_cell *ce,
			char **data)
{
	if (!cl || !data || (!cl->wrap_cell && !ce))
		return -EINVAL;

	*data = NULL;

	if (ce && cl->wrap_cell != ce)
		scols_column_init_wrap(cl, ce);		/* init */
	else {
		cl->wrap_cur = cl->wrap_next;	/* next step */
		cl->wrap_next = NULL;
	}

	if (!cl->wrap_cur)
		return 1;				/* no more data */
	if (scols_column_is_customwrap(cl))
		cl->wrap_next = cl->wrap_nextchunk(cl, cl->wrap_cur, cl->wrapfunc_data);

	*data = cl->wrap_cur;
	return 0;
}

int scols_column_greatest_wrap(
			struct libscols_column *cl,
			struct libscols_cell *ce,
			char **data)
{
	size_t maxsz = 0;
	char *res = NULL;;

	if (!scols_column_is_customwrap(cl))
		return scols_column_next_wrap(cl, ce, data);

	while (scols_column_next_wrap(cl, ce, data) == 0) {
		size_t sz = strlen(*data);

		maxsz = max(maxsz, sz);
		if (maxsz == sz)
			res = *data;
	}

	*data = res;
	return 0;
}

/* Set the "next" chunk in multi-line cell to offset specified by @bytes.
 * Don't use it for columns with custom wrapfunc().
 */
int scols_column_move_wrap(struct libscols_column *cl, size_t bytes)
{
	size_t x;	/* remaining bytes */

	if (!cl->wrap_cur)
		return -EINVAL;		/* scols_column_init_wrap() not called */

	x = cl->wrap_datasz - (cl->wrap_cur - cl->wrap_data);
	if (bytes >= x)
		cl->wrap_next = NULL;	/* done */
	else
		cl->wrap_next = cl->wrap_cur + bytes;
	return 0;
}

int scols_column_has_pending_wrap(struct libscols_column *cl)
{
	return cl && cl->wrap_next;
}
