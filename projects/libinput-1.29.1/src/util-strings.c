/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdint.h>

#include "util-strings.h"

/**
 * Return the next word in a string pointed to by state before the first
 * separator character. Call repeatedly to tokenize a whole string.
 *
 * @param state Current state
 * @param len String length of the word returned
 * @param separators List of separator characters
 *
 * @return The first word in *state, NOT null-terminated
 */
static const char *
next_word(const char **state, size_t *len, const char *separators)
{
	assert(state != NULL);

	const char *next = *state;
	size_t l;

	if (!*next)
		return NULL;

	next += strspn(next, separators);
	if (!*next) {
		*state = next;
		return NULL;
	}

	l = strcspn(next, separators);
	*state = next + l;
	*len = l;

	return next;
}

size_t
strv_len(char **strv)
{
	if (!strv)
		return 0;

	size_t size = 1;
	while (*strv) {
		size++;
		strv++;
	}
	return size;
}

char **
strv_append_vprintf(char **strv, const char *fmt, va_list args)
{
	char *dup = strdup_vprintf(fmt, args);
	char **s = strv_append_take(strv, &dup);
	return s;
}

char **
strv_append_printf(char **strv, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char **s = strv_append_vprintf(strv, fmt, args);
	va_end(args);
	return s;
}

char **
strv_append_strdup(char **strv, const char *str)
{
	char *dup = safe_strdup(str);
	return strv_append_take(strv, &dup);
}

char **
strv_append_take(char **strv, char **str)
{
	if (str && *str) {
		size_t len = strv_len(strv) + 1;
		len = max(len, 2);

		char **s = realloc(strv, len * sizeof(*strv));
		if (!s)
			abort();
		s[len - 1] = NULL;
		s[len - 2] = *str;
		*str = NULL;
		return s;
	} else {
		return strv;
	}
}

/**
 * Return a null-terminated string array with the contents of argv
 * duplicated.
 *
 * Use strv_free() to free the array.
 *
 * @return A null-terminated string array or NULL on errors
 */
char **
strv_from_argv(int argc, char **argv)
{
	char **strv = NULL;

	assert(argc >= 0);
	assert(argv != NULL);

	if (argc == 0)
		return NULL;

	strv = zalloc((argc + 1) * sizeof *strv);
	for (int i = 0; i < argc; i++) {
		char *copy = safe_strdup(argv[i]);
		if (!copy) {
			strv_free(strv);
			return NULL;
		}
		strv[i] = copy;
	}
	return strv;
}

/**
 * Return a null-terminated string array with the tokens in the input
 * string, e.g. "one two\tthree" with a separator list of " \t" will return
 * an array [ "one", "two", "three", NULL ] and num elements 3.
 *
 * Use strv_free() to free the array.
 *
 * Another example:
 *   result = strv_from_string("+1-2++3--4++-+5-+-", "+-", &nelem)
 *   result == [ "1", "2", "3", "4", "5", NULL ] and nelem == 5
 *
 * @param in Input string
 * @param separators List of separator characters
 * @param num_elements Number of elements found in the input string
 *
 * @return A null-terminated string array or NULL on errors
 */
char **
strv_from_string(const char *in, const char *separators, size_t *num_elements)
{
	assert(in != NULL);
	assert(separators != NULL);

	const char *s = in;
	size_t l, nelems = 0;
	while (next_word(&s, &l, separators) != NULL)
		nelems++;

	if (nelems == 0) {
		if (num_elements)
			*num_elements = 0;
		return NULL;
	}

	size_t strv_len = nelems + 1; /* NULL-terminated */
	char **strv = zalloc(strv_len * sizeof *strv);

	size_t idx = 0;
	const char *word;
	s = in;
	while ((word = next_word(&s, &l, separators)) != NULL) {
		char *copy = strndup(word, l);
		if (!copy) {
			strv_free(strv);
			if (num_elements)
				*num_elements = 0;
			return NULL;
		}

		strv[idx++] = copy;
	}

	if (num_elements)
		*num_elements = nelems;

	return strv;
}

/**
 * Return a newly allocated string with all elements joined by the
 * joiner, same as Python's string.join() basically.
 * A strv of ["one", "two", "three", NULL] with a joiner of ", " results
 * in "one, two, three".
 *
 * An empty strv ([NULL]) returns NULL, same for passing NULL as either
 * argument.
 *
 * @param strv Input string array
 * @param joiner Joiner between the elements in the final string
 *
 * @return A null-terminated string joining all elements
 */
char *
strv_join(char **strv, const char *joiner)
{
	char **s;
	char *str;
	size_t slen = 0;
	size_t count = 0;

	if (!strv || !joiner)
		return NULL;

	if (strv[0] == NULL)
		return NULL;

	for (s = strv, count = 0; *s; s++, count++) {
		slen += strlen(*s);
	}

	assert(slen < 1000);
	assert(strlen(joiner) < 1000);
	assert(count > 0);
	assert(count < 100);

	slen += (count - 1) * strlen(joiner);

	str = zalloc(slen + 1); /* trailing \0 */
	for (s = strv; *s; s++) {
		strcat(str, *s); // NOLINT: security.insecureAPI.strcpy
		--count;
		if (count > 0)
			strcat(str, joiner); // NOLINT: security.insecureAPI.strcpy
	}

	return str;
}

/**
 * Iterate through strv, calling func with each string and its respective index.
 * Iteration stops successfully after max elements or at the last element,
 * whichever occurs first.
 *
 * If func returns non-zero, iteration stops and strv_for_each returns
 * that value.
 *
 * @return zero on success, otherwise the error returned by the callback
 */
int
strv_for_each_n(const char **strv, size_t max, strv_foreach_callback_t func, void *data)
{
	for (size_t i = 0; i < max && strv && strv[i]; i++) {
		int ret = func(strv[i], i, data);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * Iterate through strv, calling func with each string and its respective index.
 * If func returns non-zero, iteration stops and strv_for_each returns
 * that value.
 *
 * @return zero on success, otherwise the error returned by the callback
 */
int
strv_for_each(const char **strv, strv_foreach_callback_t func, void *data)
{
	return strv_for_each_n(strv, SIZE_MAX, func, data);
}

bool
strv_find(char **strv, const char *needle, size_t *index_out)
{
	if (!strv)
		return false;

	size_t index = 0;
	char **s = strv;
	while (*s != NULL) {
		if (streq(*s, needle)) {
			if (index_out)
				*index_out = index;
			return true;
		}
		s++;
		index++;
	}

	return false;
}

bool
strv_find_substring(char **strv, const char *needle, size_t *index_out)
{
	if (!strv || !needle)
		return false;

	size_t index = 0;
	char **s = strv;
	while (*s != NULL) {
		if (strstr(*s, needle)) {
			if (index_out)
				*index_out = index;
			return true;
		}
		s++;
		index++;
	}

	return false;
}

/**
 * Return a pointer to the basename within filename.
 * If the filename the empty string or a directory (i.e. the last char of
 * filename is '/') NULL is returned.
 */
const char *
safe_basename(const char *filename)
{
	assert(filename != NULL);

	const char *basename;

	if (*filename == '\0')
		return NULL;

	basename = strrchr(filename, '/');
	if (basename == NULL)
		return filename;

	if (*(basename + 1) == '\0')
		return NULL;

	return basename + 1;
}

/**
 * Similar to basename() but returns the trunk only without the (last)
 * trailing suffix, so that:
 *
 * - foo.c returns foo
 * - foo.a.b returns foo.a
 * - foo returns foo
 * - foo/ returns ""
 *
 * @return an allocated string representing the trunk name of the file
 */
char *
trunkname(const char *filename)
{
	assert(filename != NULL);

	const char *base = safe_basename(filename);
	char *suffix;

	if (base == NULL)
		return safe_strdup("");

	suffix = rindex(base, '.');
	if (suffix == NULL)
		return safe_strdup(base);
	else
		return strndup(base, suffix - base);
}
