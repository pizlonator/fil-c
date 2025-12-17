/*
 * Copyright (C) 2021 Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "cfg.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xsize.h"

#define SIZEOF(x) (sizeof(x) / sizeof((x)[0]))

struct options_st {
	struct cfg_option_st *data;
	size_t length;
	size_t capacity;
};

struct parser_st {
	FILE *fp;
	char pushback[2];
	size_t pushback_length;
};

static inline void clear_option(struct cfg_option_st *option)
{
	free(option->name);
	free(option->value);
	memset(option, 0, sizeof(*option));
}

void cfg_free(cfg_option_t options)
{
	for (size_t i = 0; options[i].name; i++) {
		clear_option(&options[i]);
	}
	free(options);
}

#define HORIZONTAL_WHITESPACE "\t "
#define WHITESPACE HORIZONTAL_WHITESPACE "\n\v\f\r\b"
#define ALPHABETIC "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define DECIMAL "0123456789"
#define NAME_FIRST_CHARS "_" ALPHABETIC
#define VALUE_NAME_CHARS ":^-" NAME_FIRST_CHARS DECIMAL

struct buffer_st {
	char *data;
	size_t length;
	size_t capacity;
};

static int buffer_append(struct buffer_st *buffer, int c)
{
	size_t new_length = xsum(buffer->length, 1);
	if (size_overflow_p(new_length)) {
		return -EINVAL;
	}
	if (buffer->capacity < new_length) {
		size_t new_capacity;
		char *new_array;

		new_capacity = xtimes(xsum(buffer->capacity, 1), 2);
		if (size_overflow_p(new_capacity)) {
			return -EINVAL;
		}
		new_array = realloc(buffer->data, new_capacity);
		if (!new_array) {
			return -errno;
		}
		buffer->capacity = new_capacity;
		buffer->data = new_array;
	}
	assert(buffer->data);
	buffer->data[buffer->length++] = c;
	return 0;
}

static int parser_getc(struct parser_st *parser)
{
	if (parser->pushback_length > 0) {
		return parser->pushback[--parser->pushback_length];
	}
	int c = getc(parser->fp);
	return c;
}

static void parser_ungetc(struct parser_st *parser, int c)
{
	assert(parser->pushback_length < SIZEOF(parser->pushback));
	parser->pushback[parser->pushback_length++] = c;
}

static void skip_comment(struct parser_st *parser)
{
	int c;

	c = parser_getc(parser);
	if (c == EOF) {
		return;
	}

	if (c == '#') {
		for (;;) {
			c = parser_getc(parser);
			if (c == EOF) {
				return;
			}
			if (c == '\n') {
				break;
			}
		}
	}
	parser_ungetc(parser, c);
}

static void skip_chars(struct parser_st *parser, const char *chars)
{
	int c;

	for (;;) {
		c = parser_getc(parser);
		if (c == EOF) {
			return;
		}
		if (!strchr(chars, c)) {
			break;
		}
	}
	parser_ungetc(parser, c);
}

static void skip_comments_and_whitespaces(struct parser_st *parser)
{
	int c;

	for (;;) {
		c = parser_getc(parser);
		if (c == EOF) {
			return;
		}
		parser_ungetc(parser, c);
		if (c == '#') {
			skip_comment(parser);
		} else if (strchr(WHITESPACE, c)) {
			skip_chars(parser, WHITESPACE);
		} else {
			break;
		}
	}
}

/* Read the name part of an option.  Returns NULL if it fails.  */
static char *read_name(struct parser_st *parser)
{
	struct buffer_st buffer;
	int c;

	memset(&buffer, 0, sizeof(buffer));

	skip_comments_and_whitespaces(parser);

	c = parser_getc(parser);
	if (c == EOF) {
		return NULL;
	}

	if (!strchr(NAME_FIRST_CHARS, c)) {
		parser_ungetc(parser, c);
		return NULL;
	}

	buffer_append(&buffer, c);
	for (;;) {
		c = parser_getc(parser);
		if (c == EOF) {
			break;
		}
		if (!strchr(VALUE_NAME_CHARS, c)) {
			parser_ungetc(parser, c);
			break;
		}
		buffer_append(&buffer, c);
	}
	assert(buffer.data);
	if (buffer.data[buffer.length - 1] == ':') {
		buffer.data[buffer.length - 1] = '\0';
		buffer.length--;
		parser_ungetc(parser, ':');
	}

	/* NUL terminate */
	buffer_append(&buffer, '\0');
	return buffer.data;
}

static char *read_quoted_value(struct parser_st *parser)
{
	struct buffer_st buffer;
	int c, quote_char;

	memset(&buffer, 0, sizeof(buffer));

	c = parser_getc(parser);
	if (c == EOF) {
		assert(false);
		return NULL;
	}

	if (c == '"' || c == '\'') {
		quote_char = c;
	} else {
		assert(false);
		return NULL;
	}

	for (;;) {
		c = parser_getc(parser);
		if (c == EOF) {
			break;
		}
		if (c == '\\') {
			c = parser_getc(parser);
			if (c == EOF) {
				/* unmatched quote */
				free(buffer.data);
				return NULL;
			}
			if (c == '\n') {
				buffer_append(&buffer, ' ');
			} else if (c == quote_char) {
				buffer_append(&buffer, c);
			}
		} else if (c == quote_char) {
			break;
		} else {
			buffer_append(&buffer, c);
		}
	}

	/* NUL terminate */
	buffer_append(&buffer, '\0');
	return buffer.data;
}

/* Read the value part of an option.  Returns NULL if it fails.  */
static char *read_value(struct parser_st *parser)
{
	struct buffer_st buffer;
	int c;

	memset(&buffer, 0, sizeof(buffer));

	skip_chars(parser, HORIZONTAL_WHITESPACE);

	c = parser_getc(parser);
	if (c == EOF) {
		goto out;
	}

	/* skip delimiter if any, followed by horizontal whitespaces */
	if (c == ':' || c == '=') {
		c = parser_getc(parser);
		if (c == EOF) {
			goto out;
		}
		parser_ungetc(parser, c);
		skip_chars(parser, HORIZONTAL_WHITESPACE);
		c = parser_getc(parser);
		if (c == EOF) {
			goto out;
		}
	}

	if (c == '\n') {
		return strdup(""); /* empty value */
	} else if (c == '"' || c == '\'') {
		parser_ungetc(parser, c);
		return read_quoted_value(parser);
	}

	buffer_append(&buffer, c);
	for (;;) {
		c = parser_getc(parser);
		if (c == EOF) {
			break;
		}
		if (c == '\\') {
			c = parser_getc(parser);
			if (c == EOF) {
				break;
			}
			if (c == '\n') {
				buffer_append(&buffer, c);
			}
		} else if (c == '\n') {
			break;
		} else {
			buffer_append(&buffer, c);
		}
	}

out:
	/* NUL terminate */
	buffer_append(&buffer, '\0');
	return buffer.data;
}

/* Append OPTION to OPTIONS.  Take ownership of the fields of OPTION.  */
static int take_option(struct options_st *options, struct cfg_option_st *option)
{
	size_t new_length = xsum(options->length, 1);
	if (size_overflow_p(new_length)) {
		return -EINVAL;
	}
	if (options->capacity < new_length) {
		size_t new_capacity;
		struct cfg_option_st *new_array;

		new_capacity = xtimes(xsum(options->capacity, 1), 2);
		if (size_overflow_p(new_capacity)) {
			return -EINVAL;
		}
		new_array = reallocarray(options->data, new_capacity,
					 sizeof(*option));
		if (!new_array) {
			return -errno;
		}
		options->capacity = new_capacity;
		options->data = new_array;
	}

	assert(options->data);

	options->data[options->length].name = option->name;
	options->data[options->length].value = option->value;

	options->length++;

	option->name = NULL;
	option->value = NULL;

	return 0;
}

static void clear_options(struct options_st *options)
{
	for (size_t i = 0; options->length; i++) {
		clear_option(&options->data[i]);
	}
}

cfg_option_t cfg_load(const char *filename)
{
	struct parser_st parser;
	struct options_st options;
	struct cfg_option_st null_option = { NULL, NULL };

	memset(&parser, 0, sizeof(parser));
	memset(&options, 0, sizeof(options));

	parser.fp = fopen(filename, "r");
	if (!parser.fp) {
		return NULL;
	}

	for (;;) {
		struct cfg_option_st option;

		option.name = read_name(&parser);
		if (!option.name) {
			break;
		}

		option.value = read_value(&parser);
		if (!option.value) {
			clear_option(&option);
			goto error;
		}

		if (take_option(&options, &option) < 0) {
			clear_option(&option);
			goto error;
		}
		assert(!option.name && !option.value);
	}

	fclose(parser.fp);
	/* NUL terminate */
	take_option(&options, &null_option);
	return options.data;

error:
	clear_options(&options);
	fclose(parser.fp);
	return NULL;
}

cfg_option_t cfg_next(const cfg_option_t options, const char *name)
{
	for (size_t i = 0; options[i].name; i++) {
		if (strcmp(options[i].name, name) == 0) {
			return &options[i];
		}
	}
	return NULL;
}

#ifdef TEST
int main(int argc, char **argv)
{
	cfg_option_t opts;

	assert(argc == 2);

	opts = cfg_load(argv[1]);
	for (size_t i = 0; opts[i].name; i++) {
		printf("%s: %s\n", opts[i].name, opts[i].value);
	}
	cfg_free(opts);

	return 0;
}
#endif
