/*
 * JSON output formatting functions.
 *
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com>
 */
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <cctype.h>

#include "c.h"
#include "jsonwrt.h"

/*
 * Requirements enumerated via testing (V8, Firefox, IE11):
 *
 * var charsToEscape = [];
 * for (var i = 0; i < 65535; i += 1) {
 *	try {
 *		JSON.parse('{"sample": "' + String.fromCodePoint(i) + '"}');
 *	} catch (e) {
 *		charsToEscape.push(i);
 *	}
 * }
 */
static void fputs_quoted_case_json(const char *data, FILE *out, int dir, size_t size)
{
	const char *p;

	fputc('"', out);
	for (p = data; p && *p && (!size || p < data + size); p++) {

		const unsigned int c = (unsigned int) *p;

		/* From http://www.json.org
		 *
		 * The double-quote and backslashes would break out a string or
		 * init an escape sequence if not escaped.
		 *
		 * Note that single-quotes and forward slashes, while they're
		 * in the JSON spec, don't break double-quoted strings.
		 */
		if (c == '"' || c == '\\') {
			fputc('\\', out);
			fputc(c, out);
			continue;
		}

		/* All non-control characters OK; do the case swap as required. */
		if (c >= 0x20) {
			/*
			 * Don't use locale sensitive ctype.h functions for regular
			 * ASCII chars, because for example with Turkish locale
			 * (aka LANG=tr_TR.UTF-8) toupper('I') returns 'I'.
			 */
			if (c <= 127)
				fputc(dir ==  1 ? c_toupper(c) :
				      dir == -1 ? c_tolower(c) : *p, out);
			else
				fputc(dir ==  1 ? toupper(c) :
				      dir == -1 ? tolower(c) : *p, out);
			continue;
		}

		/* In addition, all chars under ' ' break Node's/V8/Chrome's, and
		 * Firefox's JSON.parse function
		 */
		switch (c) {
			/* Handle short-hand cases to reduce output size.  C
			 * has most of the same stuff here, so if there's an
			 * "Escape for C" function somewhere in the STL, we
			 * should probably be using it.
			 */
			case '\b':
				fputs("\\b", out);
				break;
			case '\t':
				fputs("\\t", out);
				break;
			case '\n':
				fputs("\\n", out);
				break;
			case '\f':
				fputs("\\f", out);
				break;
			case '\r':
				fputs("\\r", out);
				break;
			default:
				/* Other assorted control characters */
				fprintf(out, "\\u00%02x", c);
				break;
		}
	}
	fputc('"', out);
}

#define fputs_quoted_json(_d, _o)       fputs_quoted_case_json(_d, _o, 0, 0)
#define fputs_quoted_json_upper(_d, _o) fputs_quoted_case_json(_d, _o, 1, 0)
#define fputs_quoted_json_lower(_d, _o) fputs_quoted_case_json(_d, _o, -1, 0)

void ul_jsonwrt_init(struct ul_jsonwrt *fmt, FILE *out, int indent)
{
	fmt->out = out;
	fmt->indent = indent;
	fmt->after_close = 0;
}

int ul_jsonwrt_is_ready(struct ul_jsonwrt *fmt)
{
	return fmt->out == NULL ? 0 : 1;
}

void ul_jsonwrt_indent(struct ul_jsonwrt *fmt)
{
	int i;

	for (i = 0; i < fmt->indent; i++)
		fputs("   ", fmt->out);
}

void ul_jsonwrt_open(struct ul_jsonwrt *fmt, const char *name, int type)
{
	if (name) {
		if (fmt->after_close)
			fputs(",\n", fmt->out);
		ul_jsonwrt_indent(fmt);
		fputs_quoted_json_lower(name, fmt->out);
	} else {
		if (fmt->after_close)
			fputs(",", fmt->out);
		else
			ul_jsonwrt_indent(fmt);
	}

	switch (type) {
	case UL_JSON_OBJECT:
		fputs(name ? ": {\n" : "{\n", fmt->out);
		fmt->indent++;
		break;
	case UL_JSON_ARRAY:
		fputs(name ? ": [\n" : "[\n", fmt->out);
		fmt->indent++;
		break;
	case UL_JSON_VALUE:
		fputs(name ? ": " : " ", fmt->out);
		break;
	}
	fmt->after_close = 0;
}

void ul_jsonwrt_close(struct ul_jsonwrt *fmt, int type)
{
	assert(fmt->indent > 0);

	switch (type) {
	case UL_JSON_OBJECT:
		fmt->indent--;
		fputc('\n', fmt->out);
		ul_jsonwrt_indent(fmt);
		fputs("}", fmt->out);
		if (fmt->indent == 0)
			fputs("\n", fmt->out);
		break;
	case UL_JSON_ARRAY:
		fmt->indent--;
		fputc('\n', fmt->out);
		ul_jsonwrt_indent(fmt);
		fputs("]", fmt->out);
		break;
	case UL_JSON_VALUE:
		break;
	}

	fmt->after_close = 1;
}

void ul_jsonwrt_value_raw(struct ul_jsonwrt *fmt,
			const char *name, const char *data)
{
	ul_jsonwrt_value_open(fmt, name);
	if (data && *data)
		fputs(data, fmt->out);
	else
		fputs("null", fmt->out);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_s(struct ul_jsonwrt *fmt,
			const char *name, const char *data)
{
	ul_jsonwrt_value_open(fmt, name);
	if (data && *data)
		fputs_quoted_json(data, fmt->out);
	else
		fputs("null", fmt->out);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_s_sized(struct ul_jsonwrt *fmt,
			      const char *name, const char *data, size_t size)
{
	ul_jsonwrt_value_open(fmt, name);
	if (data && *data)
		fputs_quoted_case_json(data, fmt->out, 0, size);
	else
		fputs("null", fmt->out);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_u64(struct ul_jsonwrt *fmt,
			const char *name, uint64_t data)
{
	ul_jsonwrt_value_open(fmt, name);
	fprintf(fmt->out, "%"PRIu64, data);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_double(struct ul_jsonwrt *fmt,
			const char *name, long double data)
{
	ul_jsonwrt_value_open(fmt, name);
	fprintf(fmt->out, "%Lg", data);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_boolean(struct ul_jsonwrt *fmt,
			const char *name, int data)
{
	ul_jsonwrt_value_open(fmt, name);
	fputs(data ? "true" : "false", fmt->out);
	ul_jsonwrt_value_close(fmt);
}

void ul_jsonwrt_value_null(struct ul_jsonwrt *fmt,
			const char *name)
{
	ul_jsonwrt_value_open(fmt, name);
	fputs("null", fmt->out);
	ul_jsonwrt_value_close(fmt);
}
