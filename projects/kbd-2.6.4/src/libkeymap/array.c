#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <keymap/array.h>

int
lk_array_init(struct lk_array *a, ssize_t memb, ssize_t size)
{
	if (!a || memb < 0 || size < 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	memset(a, 0, sizeof(struct lk_array));

	a->array = calloc((size_t) size, (size_t) memb);
	a->memb  = memb;
	a->total = size;

	if (size && !a->array) {
		errno = ENOMEM;
		return -ENOMEM;
	}

	return 0;
}

int
lk_array_free(struct lk_array *a)
{
	if (!a) {
		errno = EINVAL;
		return -EINVAL;
	}
	free(a->array);
	memset(a, 0, sizeof(struct lk_array));
	return 0;
}

int
lk_array_empty(struct lk_array *a)
{
	if (!a) {
		errno = EINVAL;
		return -EINVAL;
	}

	memset(a->array, 0, (size_t) (a->memb * a->total));
	a->count = 0;

	return 0;
}

int
lk_array_exists(struct lk_array *a, ssize_t i)
{
	char *s;
	ssize_t k;

	if (!a || i < 0 || i >= a->total) {
		errno = EINVAL;
		return 0;
	}

	s = a->array + (a->memb * i);

	for (k = 0; k < a->memb; k++) {
		if (s[k] != 0)
			return 1;
	}

	return 0;
}

void *
lk_array_get(struct lk_array *a, ssize_t i)
{
	if (!a || i < 0 || i >= a->total) {
		errno = EINVAL;
		return NULL;
	}
	return a->array + (a->memb * i);
}

void *
lk_array_get_ptr(struct lk_array *a, ssize_t i)
{
	void **ptr;
	if (!a || i < 0 || i >= a->total) {
		errno = EINVAL;
		return NULL;
	}
	ptr = (void **) a->array;
	return *(ptr + i);
}

static int
array_resize(struct lk_array *a, ssize_t i)
{
	if (!a || i < 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	if (i >= a->total) {
		char *tmp = realloc(a->array, (size_t) (a->memb * (i + 1)));
		if (!tmp) {
			errno = ENOMEM;
			return -ENOMEM;
		}

		memset(tmp + (a->memb * a->total), 0, (size_t) (a->memb * (i + 1 - a->total)));

		a->array = tmp;
		a->total = i + 1;
	}
	return 0;
}

int
lk_array_set(struct lk_array *a, ssize_t i, const void *e)
{
	int ret = array_resize(a, i);

	if (ret < 0)
		return ret;

	memcpy(a->array + (a->memb * i), e, (size_t) a->memb);
	a->count++;

	return 0;
}

int
lk_array_unset(struct lk_array *a, ssize_t i)
{
	if (!a || i >= a->total) {
		errno = EINVAL;
		return -EINVAL;
	}

	if (lk_array_exists(a, i)) {
		memset(a->array + (a->memb * i), 0, (size_t) a->memb);
		a->count--;
	}

	return 0;
}

int
lk_array_append(struct lk_array *a, const void *e)
{
	int ret = array_resize(a, a->count);

	if (ret < 0)
		return ret;

	memcpy(a->array + (a->memb * a->count), e, (size_t) a->memb);
	a->count++;

	return 0;
}
