
/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_FILEEQ
#define UTIL_LINUX_FILEEQ

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__linux__) && defined(HAVE_LINUX_IF_ALG_H)
# define USE_FILEEQ_CRYPTOAPI 1
#endif

/* Number of bytes from the beginning of the file we always
 * compare by memcmp() */
#define UL_FILEEQ_INTROSIZ	32

struct ul_fileeq_data {
	unsigned char intro[UL_FILEEQ_INTROSIZ];
	unsigned char *blocks;
	size_t nblocks;
	size_t maxblocks;
	int fd;
	const char *name;
	bool is_eof;
};

struct ul_fileeq {
	int fd_api;	/* Crypto socket */
	int fd_cip;	/* Cipher handler */

	size_t readsiz;
	uint64_t filesiz;
	uint64_t blocksmax;
	const struct ul_fileeq_method *method;

	/* UL_FILEEQ_MEMCMP buffers */
	unsigned char *buf_a;
	unsigned char *buf_b;
	unsigned char *buf_last;
};

extern int ul_fileeq_init(struct ul_fileeq *eq, const char *method);
extern void ul_fileeq_deinit(struct ul_fileeq *eq);


extern int ul_fileeq_data_associated(struct ul_fileeq_data *data);
extern void ul_fileeq_data_close_file(struct ul_fileeq_data *data);
extern void ul_fileeq_data_init(struct ul_fileeq_data *data);
extern void ul_fileeq_data_deinit(struct ul_fileeq_data *data);
extern void ul_fileeq_data_set_file(struct ul_fileeq_data *data,
				    const char *name);
extern size_t ul_fileeq_set_size(struct ul_fileeq *eq, uint64_t filesiz,
                                 size_t readsiz, size_t memsiz);

extern int ul_fileeq(struct ul_fileeq *eq,
              struct ul_fileeq_data *a, struct ul_fileeq_data *b);

#endif /* UTIL_LINUX_FILEEQ */
