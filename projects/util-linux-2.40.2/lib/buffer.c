/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com>
 */
#include "buffer.h"
#include "mbsalign.h"
#include "strutils.h"

void ul_buffer_reset_data(struct ul_buffer *buf)
{
	if (buf->begin)
		memset(buf->begin, 0, buf->sz);
	buf->end = buf->begin;

	if (buf->ptrs && buf->nptrs)
		memset(buf->ptrs, 0, buf->nptrs * sizeof(char *));
}

void ul_buffer_free_data(struct ul_buffer *buf)
{
	assert(buf);

	free(buf->begin);
	buf->begin = NULL;
	buf->end = NULL;
	buf->sz = 0;

	free(buf->ptrs);
	buf->ptrs = NULL;
	buf->nptrs = 0;

	free(buf->encoded);
	buf->encoded = NULL;
	buf->encoded_sz = 0;
}

void ul_buffer_set_chunksize(struct ul_buffer *buf, size_t sz)
{
	buf->chunksize = sz;
}

int ul_buffer_is_empty(struct ul_buffer *buf)
{
	return buf->begin == buf->end;
}

int ul_buffer_save_pointer(struct ul_buffer *buf, unsigned short ptr_idx)
{
	if (ptr_idx >= buf->nptrs) {
		char **tmp = reallocarray(buf->ptrs, ptr_idx + 1, sizeof(char *));

		if (!tmp)
			return -EINVAL;
		buf->ptrs = tmp;
		buf->nptrs = ptr_idx + 1;
	}

	buf->ptrs[ptr_idx] = buf->end;
	return 0;
}


char *ul_buffer_get_pointer(struct ul_buffer *buf, unsigned short ptr_idx)
{
	if (ptr_idx < buf->nptrs)
		return buf->ptrs[ptr_idx];
	return NULL;
}

/* returns length from begin to the pointer */
size_t ul_buffer_get_pointer_length(struct ul_buffer *buf, unsigned short ptr_idx)
{
	char *ptr = ul_buffer_get_pointer(buf, ptr_idx);

	if (ptr && ptr > buf->begin)
		return ptr - buf->begin;
	return 0;
}

/* returns width of data in safe encoding (from the begin to the pointer) */
size_t ul_buffer_get_safe_pointer_width(struct ul_buffer *buf, unsigned short ptr_idx)
{
	size_t len = ul_buffer_get_pointer_length(buf, ptr_idx);

	if (!len)
		return 0;

	return mbs_safe_nwidth(buf->begin, len, NULL);
}

void ul_buffer_refer_string(struct ul_buffer *buf, char *str)
{
	if (buf->sz)
		ul_buffer_free_data(buf);
	buf->begin = str;
	buf->sz = str ? strlen(str) : 0;
	buf->end = buf->begin ? buf->begin + buf->sz : buf->begin;
}

int ul_buffer_alloc_data(struct ul_buffer *buf, size_t sz)
{
	char *tmp;
	size_t len = 0;

	assert(buf);

	if (sz <= buf->sz)
		return 0;

	if (buf->end && buf->begin)
		len = buf->end - buf->begin;

	if (buf->chunksize)
		sz = ((sz + buf->chunksize) / buf->chunksize) * buf->chunksize + 1;

	tmp = realloc(buf->begin, sz);
	if (!tmp)
		return -ENOMEM;

	buf->begin = tmp;
	buf->end = buf->begin + len;
	buf->sz = sz;

	memset(buf->end, '\0', sz - len);

	return 0;
}

int ul_buffer_append_data(struct ul_buffer *buf, const char *data, size_t sz)
{
	size_t maxsz = 0;

	if (!buf)
		return -EINVAL;
	if (!data)
		return 0;

	if (buf->begin && buf->end)
		maxsz = buf->sz - (buf->end - buf->begin);
	if (maxsz <= sz + 1) {
		int rc = ul_buffer_alloc_data(buf, buf->sz + sz + 1);
		if (rc)
			return rc;
	}
	if (!buf->end)
		return -EINVAL;	/* make static analyzers happy */

	buf->end = mempcpy(buf->end, data, sz);
	*buf->end = '\0';	/* make sure it's terminated */
	return 0;
}

int ul_buffer_append_string(struct ul_buffer *buf, const char *str)
{
	if (!str)
		return 0;

	return ul_buffer_append_data(buf, str, strlen(str));
}

int ul_buffer_append_ntimes(struct ul_buffer *buf, size_t n, const char *str)
{
	size_t i;
	size_t len = strlen(str);

	for (i = 0; len && i < n; i++) {
		int rc = ul_buffer_append_data(buf, str, len);
		if (rc)
			return rc;
	}
	return 0;
}

int ul_buffer_set_data(struct ul_buffer *buf, const char *data, size_t sz)
{
	ul_buffer_reset_data(buf);
	return ul_buffer_append_data(buf, data, sz);
}

char *ul_buffer_get_data(struct ul_buffer *buf, size_t *sz, size_t *width)
{
	if (sz)
		*sz = buf->end - buf->begin;
	if (width)
		*width = buf->begin && *buf->begin ? mbs_width(buf->begin) : 0;
	return buf->begin;
}

char *ul_buffer_get_string(struct ul_buffer *buf, size_t *sz, size_t *width)
{
	char *ret;

	ret = ul_buffer_get_data(buf, sz, width);

	/* data in buffer is already zero-terminated */
	if (sz)
		*sz = *sz + 1;

	return ret;
}

/* size of allocated area (!= size of stored data */
size_t ul_buffer_get_bufsiz(struct ul_buffer *buf)
{
	return buf->sz;
}

/* encode data by mbs_safe_encode() to avoid control and non-printable chars */
char *ul_buffer_get_safe_data(struct ul_buffer *buf, size_t *sz, size_t *width, const char *safechars)
{
	char *data = ul_buffer_get_data(buf, NULL, NULL);
	size_t encsz, wsz = 0;
	char *res = NULL;

	if (!data)
		goto nothing;

	encsz = mbs_safe_encode_size(buf->sz) + 1;
	if (encsz > buf->encoded_sz) {
		char *tmp = realloc(buf->encoded, encsz);
		if (!tmp)
			goto nothing;
		buf->encoded = tmp;
		buf->encoded_sz = encsz;
	}

	res = mbs_safe_encode_to_buffer(data, &wsz, buf->encoded, safechars);
	if (!res || !wsz || wsz == (size_t) -1)
		goto nothing;

	if (width)
		*width = wsz;
	if (sz)
		*sz = strlen(res);
	return res;
nothing:
	if (width)
		*width = 0;
	if (sz)
		*sz = 0;
	return NULL;
}


#ifdef TEST_PROGRAM_BUFFER

enum {
	PTR_AAA  = 0,
	PTR_BBB,
};

int main(void)
{
	struct ul_buffer buf = UL_INIT_BUFFER;
	char *str;
	size_t sz = 0;

	ul_buffer_set_chunksize(&buf, 16);

	ul_buffer_append_string(&buf, "AAA");
	ul_buffer_append_data(&buf, "=", 1);
	ul_buffer_append_string(&buf, "aaa");
	ul_buffer_save_pointer(&buf, PTR_AAA);

	ul_buffer_append_data(&buf, ",", 1);
	ul_buffer_append_string(&buf, "BBB");
	ul_buffer_append_string(&buf, "=");
	ul_buffer_append_string(&buf, "bbb");
	ul_buffer_save_pointer(&buf, PTR_BBB);

	str = ul_buffer_get_data(&buf, &sz, NULL);
	printf("data [%zu] '%s'\n", sz, str);

	printf(" pointer data len: AAA=%zu, BBB=%zu\n",
			ul_buffer_get_pointer_length(&buf, PTR_AAA),
			ul_buffer_get_pointer_length(&buf, PTR_BBB));
	printf(" pointer data width: AAA=%zu, BBB=%zu\n",
			ul_buffer_get_safe_pointer_width(&buf, PTR_AAA),
			ul_buffer_get_safe_pointer_width(&buf, PTR_BBB));

	ul_buffer_reset_data(&buf);
	ul_buffer_append_string(&buf, "This is really long string to test the buffer function.");
	ul_buffer_save_pointer(&buf, PTR_AAA);
	ul_buffer_append_string(&buf, " YES!");
	str = ul_buffer_get_data(&buf, &sz, NULL);
	printf("data [%zu] '%s'\n", sz, str);
	printf(" pointer data len: AAA=%zu\n", ul_buffer_get_pointer_length(&buf, PTR_AAA));

	ul_buffer_free_data(&buf);
	str = strdup("foo");
	ul_buffer_refer_string(&buf, str);
	ul_buffer_append_data(&buf, ",", 1);
	ul_buffer_append_string(&buf, "bar");
	str = ul_buffer_get_data(&buf, &sz, NULL);
	printf("data [%zu] '%s'\n", sz, str);

	ul_buffer_free_data(&buf);

        return EXIT_SUCCESS;
}
#endif /* TEST_PROGRAM_BUFFER */
