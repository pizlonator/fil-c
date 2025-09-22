/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_BUFFER
#define UTIL_LINUX_BUFFER

#include "c.h"

struct ul_buffer {
	char *begin;		/* begin of the data */
	char *end;		/* current end of data */

	size_t sz;		/* allocated space for data */
	size_t chunksize;

	char *encoded;		/* encoded data (from mbs_safe_encode_to_buffer)) */
	size_t encoded_sz;	/* space allocated for encoded data */

	char **ptrs;		/* saved pointers */
	size_t nptrs;		/* number of saved pointers */
};

#define UL_INIT_BUFFER { .begin = NULL }

void ul_buffer_reset_data(struct ul_buffer *buf);
void ul_buffer_free_data(struct ul_buffer *buf);
int ul_buffer_is_empty(struct ul_buffer *buf);
void ul_buffer_set_chunksize(struct ul_buffer *buf, size_t sz);
void ul_buffer_refer_string(struct ul_buffer *buf, char *str);
int ul_buffer_alloc_data(struct ul_buffer *buf, size_t sz);
int ul_buffer_append_data(struct ul_buffer *buf, const char *data, size_t sz);
int ul_buffer_append_string(struct ul_buffer *buf, const char *str);
int ul_buffer_append_ntimes(struct ul_buffer *buf, size_t n, const char *str);
int ul_buffer_set_data(struct ul_buffer *buf, const char *data, size_t sz);

char *ul_buffer_get_data(struct ul_buffer *buf,  size_t *sz, size_t *width);
char *ul_buffer_get_string(struct ul_buffer *buf,  size_t *sz, size_t *width);
char *ul_buffer_get_safe_data(struct ul_buffer *buf, size_t *sz, size_t *width, const char *safechars);

size_t ul_buffer_get_bufsiz(struct ul_buffer *buf);

int ul_buffer_save_pointer(struct ul_buffer *buf, unsigned short ptr_idx);
char *ul_buffer_get_pointer(struct ul_buffer *buf, unsigned short  ptr_idx);
size_t ul_buffer_get_pointer_length(struct ul_buffer *buf, unsigned short ptr_idx);
size_t ul_buffer_get_safe_pointer_width(struct ul_buffer *buf, unsigned short ptr_idx);


#endif /* UTIL_LINUX_BUFFER */
