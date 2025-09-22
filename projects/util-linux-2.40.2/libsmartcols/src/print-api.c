#include "smartcolsP.h"

/**
 * scola_table_print_range:
 * @tb: table
 * @start: first printed line or NULL to print from the begin of the table
 * @end: last printed line or NULL to print all from start.
 *
 * If the start is the first line in the table than prints table header too.
 * The header is printed only once. This does not work for trees.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_table_print_range(	struct libscols_table *tb,
				struct libscols_line *start,
				struct libscols_line *end)
{
	struct ul_buffer buf = UL_INIT_BUFFER;
	struct libscols_iter itr;
	int rc;

	if (scols_table_is_tree(tb))
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "printing range from API"));

	rc = __scols_initialize_printing(tb, &buf);
	if (rc)
		return rc;

	if (start) {
		itr.direction = SCOLS_ITER_FORWARD;
		itr.head = &tb->tb_lines;
		itr.p = &start->ln_lines;
	} else
		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);

	if (!start || itr.p == tb->tb_lines.next) {
		rc = __scols_print_header(tb, &buf);
		if (rc)
			goto done;
	}

	rc = __scols_print_range(tb, &buf, &itr, end);
done:
	__scols_cleanup_printing(tb, &buf);
	return rc;
}

/**
 * scols_table_print_range_to_string:
 * @tb: table
 * @start: first printed line or NULL to print from the beginning of the table
 * @end: last printed line or NULL to print all from start.
 * @data: pointer to the beginning of a memory area to print to
 *
 * The same as scols_table_print_range(), but prints to @data instead of
 * stream.
 *
 * Returns: 0, a negative value in case of an error.
 */
#ifdef HAVE_OPEN_MEMSTREAM
int scols_table_print_range_to_string(	struct libscols_table *tb,
					struct libscols_line *start,
					struct libscols_line *end,
					char **data)
{
	FILE *stream, *old_stream;
	size_t sz;
	int rc;

	if (!tb)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "printing range to string"));

	/* create a stream for output */
	stream = open_memstream(data, &sz);
	if (!stream)
		return -ENOMEM;

	old_stream = scols_table_get_stream(tb);
	scols_table_set_stream(tb, stream);
	rc = scols_table_print_range(tb, start, end);
	fclose(stream);
	scols_table_set_stream(tb, old_stream);

	return rc;
}
#else
int scols_table_print_range_to_string(
			struct libscols_table *tb __attribute__((__unused__)),
			struct libscols_line *start __attribute__((__unused__)),
			struct libscols_line *end __attribute__((__unused__)),
			char **data __attribute__((__unused__)))
{
	return -ENOSYS;
}
#endif

static int do_print_table(struct libscols_table *tb, int *is_empty)
{
	int rc = 0;
	struct ul_buffer buf = UL_INIT_BUFFER;

	if (!tb)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "printing"));
	if (is_empty)
		*is_empty = 0;

	if (list_empty(&tb->tb_columns)) {
		DBG(TAB, ul_debugobj(tb, "error -- no columns"));
		return -EINVAL;
	}
	if (list_empty(&tb->tb_lines)) {
		DBG(TAB, ul_debugobj(tb, "ignore -- no lines"));
		if (scols_table_is_json(tb)) {
			ul_jsonwrt_init(&tb->json, tb->out, 0);
			ul_jsonwrt_root_open(&tb->json);
			ul_jsonwrt_array_open(&tb->json, tb->name ? tb->name : "");
			ul_jsonwrt_array_close(&tb->json);
			ul_jsonwrt_root_close(&tb->json);
		} else if (is_empty)
			*is_empty = 1;
		return 0;
	}

	tb->header_printed = 0;
	rc = __scols_initialize_printing(tb, &buf);
	if (rc)
		return rc;

	if (scols_table_is_json(tb)) {
		ul_jsonwrt_root_open(&tb->json);
		ul_jsonwrt_array_open(&tb->json, tb->name ? tb->name : "");
	}

	if (tb->format == SCOLS_FMT_HUMAN)
		__scols_print_title(tb);

	rc = __scols_print_header(tb, &buf);
	if (rc)
		goto done;

	if (scols_table_is_tree(tb))
		rc = __scols_print_tree(tb, &buf);
	else
		rc = __scols_print_table(tb, &buf);

	if (scols_table_is_json(tb)) {
		ul_jsonwrt_array_close(&tb->json);
		ul_jsonwrt_root_close(&tb->json);
	}
done:
	__scols_cleanup_printing(tb, &buf);
	return rc;
}

/**
 * scols_print_table:
 * @tb: table
 *
 * Prints the table to the output stream and terminate by \n.
 *
 * Returns: 0, a negative value in case of an error.
 */
int scols_print_table(struct libscols_table *tb)
{
	int empty = 0;
	int rc = do_print_table(tb, &empty);

	if (rc == 0 && !empty && !scols_table_is_json(tb))
		fputc('\n', tb->out);
	return rc;
}

/**
 * scols_print_table_to_string:
 * @tb: table
 * @data: pointer to the beginning of a memory area to print to
 *
 * Prints the table to @data.
 *
 * Returns: 0, a negative value in case of an error.
 */
#ifdef HAVE_OPEN_MEMSTREAM
int scols_print_table_to_string(struct libscols_table *tb, char **data)
{
	FILE *stream, *old_stream;
	size_t sz;
	int rc;

	if (!tb)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "printing to string"));

	/* create a stream for output */
	stream = open_memstream(data, &sz);
	if (!stream)
		return -ENOMEM;

	old_stream = scols_table_get_stream(tb);
	scols_table_set_stream(tb, stream);
	rc = do_print_table(tb, NULL);
	fclose(stream);
	scols_table_set_stream(tb, old_stream);

	return rc;
}
#else
int scols_print_table_to_string(
		struct libscols_table *tb __attribute__((__unused__)),
		char **data  __attribute__((__unused__)))
{
	return -ENOSYS;
}
#endif
