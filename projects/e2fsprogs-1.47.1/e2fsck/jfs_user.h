/*
 * Compatibility header file for e2fsck which should be included
 * instead of linux/jfs.h
 *
 * Copyright (C) 2000 Stephen C. Tweedie
 *
 * This file may be redistributed under the terms of the
 * GNU General Public License version 2 or at your discretion
 * any later version.
 */
#ifndef _JFS_USER_H
#define _JFS_USER_H

#include "config.h"

#ifdef DEBUGFS
#include <stdio.h>
#include <stdlib.h>
#if EXT2_FLAT_INCLUDES
#include "ext2_fs.h"
#include "ext2fs.h"
#include "blkid.h"
#else
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"
#endif
#else
/*
 * Pull in the definition of the e2fsck context structure
 */
#include "e2fsck.h"
#endif

#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2 || _MSC_VER >= 1300
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

struct buffer_head {
#ifdef DEBUGFS
	ext2_filsys	b_fs;
#else
	e2fsck_t	b_ctx;
#endif
	io_channel	b_io;
	int		b_size;
	int		b_err;
	unsigned int	b_dirty:1;
	unsigned int	b_uptodate:1;
	unsigned long long b_blocknr;
	char		b_data[4096];
};

struct inode {
#ifdef DEBUGFS
	ext2_filsys	i_fs;
#else
	e2fsck_t	i_ctx;
#endif
	ext2_ino_t	i_ino;
	struct ext2_inode i_ext2;
};

struct kdev_s {
#ifdef DEBUGFS
	ext2_filsys	k_fs;
#else
	e2fsck_t	k_ctx;
#endif
	int		k_dev;
};

#define K_DEV_FS	1
#define K_DEV_JOURNAL	2

#define lock_buffer(bh) do {} while (0)
#define unlock_buffer(bh) do {} while (0)
#define buffer_req(bh) 1
#define do_readahead(journal, start) do {} while (0)

struct kmem_cache {
	unsigned int	object_size;
};

#define cond_resched()	do { } while (0)

#define __init

/*
 * Now pull in the real linux/jfs.h definitions.
 */
#include <ext2fs/kernel-jbd.h>

/*
 * We use the standard libext2fs portability tricks for inline
 * functions.
 */
#ifdef NO_INLINE_FUNCS
extern struct kmem_cache *kmem_cache_create(const char *name,
					    unsigned int size,
					    unsigned int align,
					    unsigned int flags,
					    void (*ctor)(void *));
extern void kmem_cache_destroy(struct kmem_cache *s);
extern void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);
extern void kmem_cache_free(struct kmem_cache *s, void *objp);
extern void *kmalloc(size_t size, gfp_t flags);
extern void kfree(const void *objp);
extern size_t journal_tag_bytes(journal_t *journal);
extern __u32 __hash_32(__u32 val);
extern __u32 hash_32(__u32 val, unsigned int bits);
extern __u32 hash_64(__u64 val, unsigned int bits);
extern void *kmalloc_array(unsigned n, unsigned size, int flags);
extern __u32 jbd2_chksum(journal_t *j, __u32 crc, const void *address,
			 unsigned int length);
extern void jbd2_descriptor_block_csum_set(journal_t *j,
					   struct buffer_head *bh);
#endif

#if (defined(E2FSCK_INCLUDE_INLINE_FUNCS) || !defined(NO_INLINE_FUNCS))
#ifdef E2FSCK_INCLUDE_INLINE_FUNCS
#if (__STDC_VERSION__ >= 199901L)
#define _INLINE_ extern inline
#else
#define _INLINE_ inline
#endif
#else /* !E2FSCK_INCLUDE_INLINE FUNCS */
#if (__STDC_VERSION__ >= 199901L)
#define _INLINE_ inline
#else /* not C99 */
#ifdef __GNUC__
#define _INLINE_ extern __inline__
#else				/* For Watcom C */
#define _INLINE_ extern inline
#endif /* __GNUC__ */
#endif /* __STDC_VERSION__ >= 199901L */
#endif /* E2FSCK_INCLUDE_INLINE_FUNCS */

_INLINE_ struct kmem_cache *
kmem_cache_create(const char *name EXT2FS_ATTR((unused)),
		  unsigned int size,
		  unsigned int align EXT2FS_ATTR((unused)),
		  unsigned int flags EXT2FS_ATTR((unused)),
		  void (*ctor)(void *) EXT2FS_ATTR((unused)))
{
	struct kmem_cache *new_cache;

	new_cache = malloc(sizeof(*new_cache));
	if (new_cache)
		new_cache->object_size = size;
	return new_cache;
}

_INLINE_ void kmem_cache_destroy(struct kmem_cache *s)
{
	free(s);
}

_INLINE_ void *kmem_cache_alloc(struct kmem_cache *cachep,
				gfp_t flags EXT2FS_ATTR((unused)))
{
	return malloc(cachep->object_size);
}

_INLINE_ void kmem_cache_free(struct kmem_cache *s EXT2FS_ATTR((unused)),
			      void *objp)
{
	free(objp);
}

_INLINE_ void *kmalloc(size_t size, gfp_t flags EXT2FS_ATTR((unused)))
{
	return malloc(size);
}

_INLINE_ void kfree(const void *objp)
{
#ifdef HAVE_INTPTR_T
	/*
	 * Work around a botch in the C standard, which triggers
	 * compiler warnings.  For better or for worse, the kernel
	 * uses const void * for kfree, while the C standard mandates
	 * the use of void *.  See: https://yarchive.net/comp/const.html
	 */
	free((void *)(intptr_t)objp);
#else
	free((void *)objp);
#endif
}

/* generic hashing taken from the Linux kernel */
#define GOLDEN_RATIO_32 0x61C88647
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

_INLINE_ __u32 __hash_32(__u32 val)
{
	return val * GOLDEN_RATIO_32;
}

_INLINE_ __u32 hash_32(__u32 val, unsigned int bits)
{
	/* High bits are more random, so use them. */
	return __hash_32(val) >> (32 - bits);
}

_INLINE_ __u32 hash_64(__u64 val, unsigned int bits)
{
	if (sizeof(long) >= 8) {
		/* 64x64-bit multiply is efficient on all 64-bit processors */
		return val * GOLDEN_RATIO_64 >> (64 - bits);
	} else {
		/* Hash 64 bits using only 32x32-bit multiply. */
		return hash_32((__u32)val ^ __hash_32(val >> 32), bits);
	}
}

_INLINE_ void *kmalloc_array(unsigned n, unsigned size,
			     int flags EXT2FS_ATTR((unused)))
{
	if (n && (~0U)/n < size)
		return NULL;
	return malloc(n * size);
}

_INLINE_ __u32 jbd2_chksum(journal_t *j EXT2FS_ATTR((unused)),
			   __u32 crc, const void *address,
			   unsigned int length)
{
	return ext2fs_crc32c_le(crc, address, length);
}

_INLINE_ void jbd2_descriptor_block_csum_set(journal_t *j,
					     struct buffer_head *bh)
{
	struct jbd2_journal_block_tail *tail;
	__u32 csum;

	if (!jbd2_journal_has_csum_v2or3(j))
		return;

	tail = (struct jbd2_journal_block_tail *)(bh->b_data + j->j_blocksize -
			sizeof(struct jbd2_journal_block_tail));
	tail->t_checksum = 0;
	csum = jbd2_chksum(j, j->j_csum_seed, bh->b_data, j->j_blocksize);
	tail->t_checksum = cpu_to_be32(csum);
}
#undef _INLINE_
#endif

/*
 * Kernel compatibility functions are defined in journal.c
 */
int jbd2_journal_bmap(journal_t *journal, unsigned long block,
		      unsigned long long *phys);
struct buffer_head *getblk(kdev_t ctx, unsigned long long blocknr,
			   int blocksize);
int sync_blockdev(kdev_t kdev);
void ll_rw_block(int rw, int op_flags, int nr, struct buffer_head *bh[]);
void mark_buffer_dirty(struct buffer_head *bh);
void mark_buffer_uptodate(struct buffer_head *bh, int val);
void brelse(struct buffer_head *bh);
int buffer_uptodate(struct buffer_head *bh);
void wait_on_buffer(struct buffer_head *bh);

/*
 * Define newer 2.5 interfaces
 */
#define __getblk(dev, blocknr, blocksize) getblk(dev, blocknr, blocksize)
#define set_buffer_uptodate(bh) mark_buffer_uptodate(bh, 1)

#ifdef DEBUGFS
#include <assert.h>
#undef J_ASSERT
#define J_ASSERT(x)	assert(x)

#define JSB_HAS_INCOMPAT_FEATURE(jsb, mask)				\
	((jsb)->s_header.h_blocktype == ext2fs_cpu_to_be32(JBD2_SUPERBLOCK_V2) &&	\
	 ((jsb)->s_feature_incompat & ext2fs_cpu_to_be32((mask))))
#else  /* !DEBUGFS */

extern e2fsck_t e2fsck_global_ctx;  /* Try your very best not to use this! */

#define J_ASSERT(assert)						\
	do { if (!(assert)) {						\
		printf ("Assertion failure in %s() at %s line %d: "	\
			"\"%s\"\n",					\
			__func__, __FILE__, __LINE__, # assert);	\
		fatal_error(e2fsck_global_ctx, 0);			\
	} } while (0)

#endif /* DEBUGFS */

#ifndef EFSBADCRC
#define EFSBADCRC	EXT2_ET_BAD_CRC
#endif
#ifndef EFSCORRUPTED
#define EFSCORRUPTED	EXT2_ET_FILESYSTEM_CORRUPTED
#endif

/* recovery.c */
extern int	jbd2_journal_recover    (journal_t *journal);
extern int	jbd2_journal_skip_recovery (journal_t *);

/* revoke.c */
extern int	jbd2_journal_init_revoke(journal_t *, int);
extern void	jbd2_journal_destroy_revoke(journal_t *);
extern void	jbd2_journal_destroy_revoke_record_cache(void);
extern void	jbd2_journal_destroy_revoke_table_cache(void);
extern int	jbd2_journal_init_revoke_record_cache(void);
extern int	jbd2_journal_init_revoke_table_cache(void);


extern int	jbd2_journal_set_revoke(journal_t *, unsigned long long, tid_t);
extern int	jbd2_journal_test_revoke(journal_t *, unsigned long long, tid_t);
extern void	jbd2_journal_clear_revoke(journal_t *);

#endif /* _JFS_USER_H */
