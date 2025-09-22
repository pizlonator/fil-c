/*
 * journal.c --- code for handling the "ext3" journal
 *
 * Copyright (C) 2000 Andreas Dilger
 * Copyright (C) 2000 Theodore Ts'o
 *
 * Parts of the code are based on fs/jfs/journal.c by Stephen C. Tweedie
 * Copyright (C) 1999 Red Hat Software
 *
 * This file may be redistributed under the terms of the
 * GNU General Public License version 2 or at your discretion
 * any later version.
 */

#include "config.h"
#ifdef HAVE_SYS_MOUNT_H
#include <sys/param.h>
#include <sys/mount.h>
#define MNT_FL (MS_MGC_VAL | MS_RDONLY)
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#define E2FSCK_INCLUDE_INLINE_FUNCS
#include "jfs_user.h"
#include "problem.h"
#include "uuid/uuid.h"

static int bh_count = 0;

/*
 * Define USE_INODE_IO to use the inode_io.c / fileio.c codepaths.
 * This creates a larger static binary, and a smaller binary using
 * shared libraries.  It's also probably slightly less CPU-efficient,
 * which is why it's not on by default.  But, it's a good way of
 * testing the functions in inode_io.c and fileio.c.
 */
#undef USE_INODE_IO

/* Checksumming functions */
static int e2fsck_journal_verify_csum_type(journal_t *j,
					   journal_superblock_t *jsb)
{
	if (!jbd2_journal_has_csum_v2or3(j))
		return 1;

	return jsb->s_checksum_type == JBD2_CRC32C_CHKSUM;
}

static __u32 e2fsck_journal_sb_csum(journal_superblock_t *jsb)
{
	__u32 crc, old_crc;

	old_crc = jsb->s_checksum;
	jsb->s_checksum = 0;
	crc = ext2fs_crc32c_le(~0, (unsigned char *)jsb,
			       sizeof(journal_superblock_t));
	jsb->s_checksum = old_crc;

	return crc;
}

static int e2fsck_journal_sb_csum_verify(journal_t *j,
					 journal_superblock_t *jsb)
{
	__u32 provided, calculated;

	if (!jbd2_journal_has_csum_v2or3(j))
		return 1;

	provided = ext2fs_be32_to_cpu(jsb->s_checksum);
	calculated = e2fsck_journal_sb_csum(jsb);

	return provided == calculated;
}

static errcode_t e2fsck_journal_sb_csum_set(journal_t *j,
					    journal_superblock_t *jsb)
{
	__u32 crc;

	if (!jbd2_journal_has_csum_v2or3(j))
		return 0;

	crc = e2fsck_journal_sb_csum(jsb);
	jsb->s_checksum = ext2fs_cpu_to_be32(crc);
	return 0;
}

/* Kernel compatibility functions for handling the journal.  These allow us
 * to use the recovery.c file virtually unchanged from the kernel, so we
 * don't have to do much to keep kernel and user recovery in sync.
 */
int jbd2_journal_bmap(journal_t *journal, unsigned long block,
		      unsigned long long *phys)
{
#ifdef USE_INODE_IO
	*phys = block;
	return 0;
#else
	struct inode 	*inode = journal->j_inode;
	errcode_t	retval;
	blk64_t		pblk;

	if (!inode) {
		*phys = block;
		return 0;
	}

	retval= ext2fs_bmap2(inode->i_ctx->fs, inode->i_ino,
			     &inode->i_ext2, NULL, 0, (blk64_t) block,
			     0, &pblk);
	*phys = pblk;
	return -1 * ((int) retval);
#endif
}

struct buffer_head *getblk(kdev_t kdev, unsigned long long blocknr,
			   int blocksize)
{
	struct buffer_head *bh;
	int bufsize = sizeof(*bh) + kdev->k_ctx->fs->blocksize -
		sizeof(bh->b_data);

	bh = e2fsck_allocate_memory(kdev->k_ctx, bufsize, "block buffer");
	if (!bh)
		return NULL;

	if (journal_enable_debug >= 3)
		bh_count++;
	jfs_debug(4, "getblk for block %llu (%d bytes)(total %d)\n",
		  blocknr, blocksize, bh_count);

	bh->b_ctx = kdev->k_ctx;
	if (kdev->k_dev == K_DEV_FS)
		bh->b_io = kdev->k_ctx->fs->io;
	else
		bh->b_io = kdev->k_ctx->journal_io;
	bh->b_size = blocksize;
	bh->b_blocknr = blocknr;

	return bh;
}

int sync_blockdev(kdev_t kdev)
{
	io_channel	io;

	if (kdev->k_dev == K_DEV_FS)
		io = kdev->k_ctx->fs->io;
	else
		io = kdev->k_ctx->journal_io;

	return io_channel_flush(io) ? -EIO : 0;
}

void ll_rw_block(int rw, int op_flags EXT2FS_ATTR((unused)), int nr,
		 struct buffer_head *bhp[])
{
	errcode_t retval;
	struct buffer_head *bh;

	for (; nr > 0; --nr) {
		bh = *bhp++;
		if (rw == REQ_OP_READ && !bh->b_uptodate) {
			jfs_debug(3, "reading block %llu/%p\n",
				  bh->b_blocknr, (void *) bh);
			retval = io_channel_read_blk64(bh->b_io,
						     bh->b_blocknr,
						     1, bh->b_data);
			if (retval) {
				com_err(bh->b_ctx->device_name, retval,
					"while reading block %llu\n",
					bh->b_blocknr);
				bh->b_err = (int) retval;
				continue;
			}
			bh->b_uptodate = 1;
		} else if (rw == REQ_OP_WRITE && bh->b_dirty) {
			jfs_debug(3, "writing block %llu/%p\n",
				  bh->b_blocknr,
				  (void *) bh);
			retval = io_channel_write_blk64(bh->b_io,
						      bh->b_blocknr,
						      1, bh->b_data);
			if (retval) {
				com_err(bh->b_ctx->device_name, retval,
					"while writing block %llu\n",
					bh->b_blocknr);
				bh->b_err = (int) retval;
				continue;
			}
			bh->b_dirty = 0;
			bh->b_uptodate = 1;
		} else {
			jfs_debug(3, "no-op %s for block %llu\n",
				  rw == REQ_OP_READ ? "read" : "write",
				  bh->b_blocknr);
		}
	}
}

void mark_buffer_dirty(struct buffer_head *bh)
{
	bh->b_dirty = 1;
}

static void mark_buffer_clean(struct buffer_head * bh)
{
	bh->b_dirty = 0;
}

void brelse(struct buffer_head *bh)
{
	if (bh->b_dirty)
		ll_rw_block(REQ_OP_WRITE, 0, 1, &bh);
	jfs_debug(3, "freeing block %llu/%p (total %d)\n",
		  bh->b_blocknr, (void *) bh, --bh_count);
	ext2fs_free_mem(&bh);
}

int buffer_uptodate(struct buffer_head *bh)
{
	return bh->b_uptodate;
}

void mark_buffer_uptodate(struct buffer_head *bh, int val)
{
	bh->b_uptodate = val;
}

void wait_on_buffer(struct buffer_head *bh)
{
	if (!bh->b_uptodate)
		ll_rw_block(REQ_OP_READ, 0, 1, &bh);
}


static void e2fsck_clear_recover(e2fsck_t ctx, int error)
{
	ext2fs_clear_feature_journal_needs_recovery(ctx->fs->super);

	/* if we had an error doing journal recovery, we need a full fsck */
	if (error)
		ctx->fs->super->s_state &= ~EXT2_VALID_FS;
	ext2fs_mark_super_dirty(ctx->fs);
}

/*
 * This is a helper function to check the validity of the journal.
 */
struct process_block_struct {
	e2_blkcnt_t	last_block;
};

static int process_journal_block(ext2_filsys fs,
				 blk64_t	*block_nr,
				 e2_blkcnt_t blockcnt,
				 blk64_t ref_block EXT2FS_ATTR((unused)),
				 int ref_offset EXT2FS_ATTR((unused)),
				 void *priv_data)
{
	struct process_block_struct *p;
	blk64_t	blk = *block_nr;

	p = (struct process_block_struct *) priv_data;

	if (!blk || blk < fs->super->s_first_data_block ||
	    blk >= ext2fs_blocks_count(fs->super))
		return BLOCK_ABORT;

	if (blockcnt >= 0)
		p->last_block = blockcnt;
	return 0;
}

static int ext4_fc_replay_scan(journal_t *j, struct buffer_head *bh,
				int off, tid_t expected_tid)
{
	e2fsck_t ctx = j->j_fs_dev->k_ctx;
	struct e2fsck_fc_replay_state *state;
	int ret = JBD2_FC_REPLAY_CONTINUE;
	struct ext4_fc_add_range ext;
	struct ext4_fc_tl tl;
	struct ext4_fc_tail tail;
	__u8 *start, *cur, *end, *val;
	struct ext4_fc_head head;
	struct ext2fs_extent ext2fs_ex = {0};

	state = &ctx->fc_replay_state;

	start = (__u8 *)bh->b_data;
	end = (__u8 *)bh->b_data + j->j_blocksize - 1;

	jbd_debug(1, "Scan phase starting, expected %d", expected_tid);
	if (state->fc_replay_expected_off == 0) {
		memset(state, 0, sizeof(*state));
		/* Check if we can stop early */
		if (le16_to_cpu(((struct ext4_fc_tl *)start)->fc_tag)
			!= EXT4_FC_TAG_HEAD) {
			jbd_debug(1, "Ending early!, not a head tag");
			return 0;
		}
	}

	if (off != state->fc_replay_expected_off) {
		ret = -EFSCORRUPTED;
		goto out_err;
	}

	state->fc_replay_expected_off++;
	for (cur = start; cur < end; cur = cur + le16_to_cpu(tl.fc_len) + sizeof(tl)) {
		memcpy(&tl, cur, sizeof(tl));
		val = cur + sizeof(tl);

		jbd_debug(3, "Scan phase, tag:%s, blk %lld\n",
			  tag2str(le16_to_cpu(tl.fc_tag)), bh->b_blocknr);
		switch (le16_to_cpu(tl.fc_tag)) {
		case EXT4_FC_TAG_ADD_RANGE:
			memcpy(&ext, val, sizeof(ext));
			ret = ext2fs_decode_extent(&ext2fs_ex,
						   (void *)&ext.fc_ex,
						   sizeof(ext.fc_ex));
			if (ret)
				ret = JBD2_FC_REPLAY_STOP;
			else
				ret = JBD2_FC_REPLAY_CONTINUE;
			/* fallthrough */
		case EXT4_FC_TAG_DEL_RANGE:
		case EXT4_FC_TAG_LINK:
		case EXT4_FC_TAG_UNLINK:
		case EXT4_FC_TAG_CREAT:
		case EXT4_FC_TAG_INODE:
		case EXT4_FC_TAG_PAD:
			state->fc_cur_tag++;
			state->fc_crc = jbd2_chksum(j, state->fc_crc, cur,
					sizeof(tl) + ext4_fc_tag_len(&tl));
			break;
		case EXT4_FC_TAG_TAIL:
			state->fc_cur_tag++;
			memcpy(&tail, val, sizeof(tail));
			state->fc_crc = jbd2_chksum(j, state->fc_crc, cur,
						sizeof(tl) +
						offsetof(struct ext4_fc_tail,
						fc_crc));
			jbd_debug(1, "tail tid %d, expected %d\n",
				  le32_to_cpu(tail.fc_tid), expected_tid);
			if (le32_to_cpu(tail.fc_tid) == expected_tid &&
			    le32_to_cpu(tail.fc_crc) == state->fc_crc) {
				state->fc_replay_num_tags = state->fc_cur_tag;
			} else {
				ret = state->fc_replay_num_tags ?
					JBD2_FC_REPLAY_STOP : -EFSBADCRC;
			}
			state->fc_crc = 0;
			break;
		case EXT4_FC_TAG_HEAD:
			memcpy(&head, val, sizeof(head));
			if (le32_to_cpu(head.fc_features) &
			    ~EXT4_FC_SUPPORTED_FEATURES) {
				ret = -EOPNOTSUPP;
				break;
			}
			if (le32_to_cpu(head.fc_tid) != expected_tid) {
				ret = -EINVAL;
				break;
			}
			state->fc_cur_tag++;
			state->fc_crc = jbd2_chksum(j, state->fc_crc, cur,
					sizeof(tl) + ext4_fc_tag_len(&tl));
			break;
		default:
			ret = state->fc_replay_num_tags ?
				JBD2_FC_REPLAY_STOP : -ECANCELED;
		}
		if (ret < 0 || ret == JBD2_FC_REPLAY_STOP)
			break;
	}

out_err:
	return ret;
}

static int __errcode_to_errno(errcode_t err, const char *func, int line)
{
	if (err == 0)
		return 0;
	fprintf(stderr, "Error \"%s\" encountered in function %s at line %d\n",
		error_message(err), func, line);
	if (err <= 256)
		return -err;
	return -EFAULT;
}

#define errcode_to_errno(err)	__errcode_to_errno(err, __func__, __LINE__)

#define ex_end(__ex) ((__ex)->e_lblk + (__ex)->e_len - 1)
#define ex_pend(__ex) ((__ex)->e_pblk + (__ex)->e_len - 1)

static int make_room(struct extent_list *list, int i)
{
	int ret;

	if (list->count == list->size) {
		unsigned int new_size = (list->size + 341) *
					sizeof(struct ext2fs_extent);
		ret = errcode_to_errno(ext2fs_resize_mem(0, new_size, &list->extents));
		if (ret)
			return ret;
		list->size += 341;
	}

	memmove(&list->extents[i + 1], &list->extents[i],
			sizeof(list->extents[0]) * (list->count - i));
	list->count++;
	return 0;
}

static int ex_compar(const void *arg1, const void *arg2)
{
	const struct ext2fs_extent *ex1 = (const struct ext2fs_extent *)arg1;
	const struct ext2fs_extent *ex2 = (const struct ext2fs_extent *)arg2;

	if (ex1->e_lblk < ex2->e_lblk)
		return -1;
	if (ex1->e_lblk > ex2->e_lblk)
		return 1;
	return ex1->e_len - ex2->e_len;
}

static int ex_len_compar(const void *arg1, const void *arg2)
{
	const struct ext2fs_extent *ex1 = (const struct ext2fs_extent *)arg1;
	const struct ext2fs_extent *ex2 = (const struct ext2fs_extent *)arg2;

	if (ex1->e_len < ex2->e_len)
		return 1;

	if (ex1->e_lblk > ex2->e_lblk)
		return -1;

	return 0;
}

static void ex_sort_and_merge(struct extent_list *list)
{
	unsigned int i, j;

	if (list->count < 2)
		return;

	/*
	 * Reverse sort by length, that way we strip off all the 0 length
	 * extents
	 */
	qsort(list->extents, list->count, sizeof(struct ext2fs_extent),
		ex_len_compar);

	for (i = 0; i < list->count; i++) {
		if (list->extents[i].e_len == 0) {
			list->count = i;
			break;
		}
	}

	if (list->count == 0)
		return;

	/* Now sort by logical offset */
	qsort(list->extents, list->count, sizeof(list->extents[0]),
		ex_compar);

	/* Merge adjacent extents if they are logically and physically contiguous */
	i = 0;
	while (i < list->count - 1) {
		if (ex_end(&list->extents[i]) + 1 != list->extents[i + 1].e_lblk ||
			ex_pend(&list->extents[i]) + 1 != list->extents[i + 1].e_pblk ||
			(list->extents[i].e_flags & EXT2_EXTENT_FLAGS_UNINIT) !=
				(list->extents[i + 1].e_flags & EXT2_EXTENT_FLAGS_UNINIT)) {
			i++;
			continue;
		}

		list->extents[i].e_len += list->extents[i + 1].e_len;
		for (j = i + 1; j < list->count - 1; j++)
			list->extents[j] = list->extents[j + 1];
		list->count--;
	}
}

/* must free blocks that are released */
static int ext4_modify_extent_list(e2fsck_t ctx, struct extent_list *list,
					struct ext2fs_extent *ex, int del)
{
	int ret, offset;
	unsigned int i;
	struct ext2fs_extent add_ex = *ex;

	/* First let's create a hole from ex->e_lblk of length ex->e_len */
	for (i = 0; i < list->count; i++) {
		if (ex_end(&list->extents[i]) < add_ex.e_lblk)
			continue;

		/* Case 1: No overlap */
		if (list->extents[i].e_lblk > ex_end(&add_ex))
			break;
		/*
		 * Unmark all the blocks in bb now. All the blocks get marked
		 * before we exit this function.
		 */
		ext2fs_unmark_block_bitmap_range2(ctx->fs->block_map,
			list->extents[i].e_pblk, list->extents[i].e_len);
		/* Case 2: Split */
		if (list->extents[i].e_lblk < add_ex.e_lblk &&
			ex_end(&list->extents[i]) > ex_end(&add_ex)) {
			ret = make_room(list, i + 1);
			if (ret)
				return ret;
			list->extents[i + 1] = list->extents[i];
			offset = ex_end(&add_ex) + 1 - list->extents[i].e_lblk;
			list->extents[i + 1].e_lblk += offset;
			list->extents[i + 1].e_pblk += offset;
			list->extents[i + 1].e_len -= offset;
			list->extents[i].e_len =
				add_ex.e_lblk - list->extents[i].e_lblk;
			break;
		}

		/* Case 3: Exact overlap */
		if (add_ex.e_lblk <= list->extents[i].e_lblk  &&
			ex_end(&list->extents[i]) <= ex_end(&add_ex)) {

			list->extents[i].e_len = 0;
			continue;
		}

		/* Case 4: Partial overlap */
		if (ex_end(&list->extents[i]) > ex_end(&add_ex)) {
			offset = ex_end(&add_ex) + 1 - list->extents[i].e_lblk;
			list->extents[i].e_lblk += offset;
			list->extents[i].e_pblk += offset;
			list->extents[i].e_len -= offset;
			break;
		}

		if (ex_end(&add_ex) >= ex_end(&list->extents[i]))
			list->extents[i].e_len =
				add_ex.e_lblk > list->extents[i].e_lblk ?
				add_ex.e_lblk - list->extents[i].e_lblk : 0;
	}

	if (add_ex.e_len && !del) {
		make_room(list, list->count);
		list->extents[list->count - 1] = add_ex;
	}

	ex_sort_and_merge(list);

	/* Mark all occupied blocks allocated */
	for (i = 0; i < list->count; i++)
		ext2fs_mark_block_bitmap_range2(ctx->fs->block_map,
			list->extents[i].e_pblk, list->extents[i].e_len);
	ext2fs_mark_bb_dirty(ctx->fs);

	return 0;
}

static int ext4_add_extent_to_list(e2fsck_t ctx, struct extent_list *list,
					struct ext2fs_extent *ex)
{
	return ext4_modify_extent_list(ctx, list, ex, 0 /* add */);
}

static int ext4_del_extent_from_list(e2fsck_t ctx, struct extent_list *list,
					struct ext2fs_extent *ex)
{
	return ext4_modify_extent_list(ctx, list, ex, 1 /* delete */);
}

static int ext4_fc_read_extents(e2fsck_t ctx, ext2_ino_t ino)
{
	struct extent_list *extent_list = &ctx->fc_replay_state.fc_extent_list;

	if (extent_list->ino == ino)
		return 0;

	extent_list->ino = ino;
	return errcode_to_errno(e2fsck_read_extents(ctx, extent_list));
}

/*
 * Flush extents in replay state on disk. @ino is the inode that is going
 * to be processed next. So, we hold back flushing of the extent list
 * if the next inode that's going to be processed is same as the one with
 * cached extents in our replay state. That allows us to gather multiple extents
 * for the inode so that we can flush all of them at once and it also saves us
 * from continuously growing and shrinking the extent tree.
 */
static void ext4_fc_flush_extents(e2fsck_t ctx, ext2_ino_t ino)
{
	struct extent_list *extent_list = &ctx->fc_replay_state.fc_extent_list;

	if (extent_list->ino == ino || extent_list->ino == 0)
		return;
	e2fsck_rewrite_extent_tree(ctx, extent_list);
	ext2fs_free_mem(&extent_list->extents);
	memset(extent_list, 0, sizeof(*extent_list));
}

/* Helper struct for dentry replay routines */
struct dentry_info_args {
	ext2_ino_t	parent_ino;
	ext2_ino_t	ino;
	int		dname_len;
	char		*dname;
};

static inline int tl_to_darg(struct dentry_info_args *darg,
			     struct  ext4_fc_tl *tl, __u8 *val)
{
	struct ext4_fc_dentry_info fcd;

	memcpy(&fcd, val, sizeof(fcd));

	darg->parent_ino = le32_to_cpu(fcd.fc_parent_ino);
	darg->ino = le32_to_cpu(fcd.fc_ino);
	darg->dname_len = ext4_fc_tag_len(tl) -
			sizeof(struct ext4_fc_dentry_info);
	darg->dname = malloc(darg->dname_len + 1);
	if (!darg->dname)
		return -ENOMEM;
	memcpy(darg->dname,
	       val + sizeof(struct ext4_fc_dentry_info),
	       darg->dname_len);
	darg->dname[darg->dname_len] = 0;
	jbd_debug(1, "%s: %s, ino %u, parent %u\n",
		  le16_to_cpu(tl->fc_tag) == EXT4_FC_TAG_CREAT ? "create" :
		  (le16_to_cpu(tl->fc_tag) == EXT4_FC_TAG_LINK ? "link" :
		   (le16_to_cpu(tl->fc_tag) == EXT4_FC_TAG_UNLINK ? "unlink" :
		    "error")), darg->dname, darg->ino, darg->parent_ino);
	return 0;
}

static int ext4_fc_handle_unlink(e2fsck_t ctx, struct ext4_fc_tl *tl, __u8 *val)
{
	struct dentry_info_args darg;
	int ret;

	ret = tl_to_darg(&darg, tl, val);
	if (ret)
		return ret;
	ext4_fc_flush_extents(ctx, darg.ino);
	ret = errcode_to_errno(ext2fs_unlink(ctx->fs, darg.parent_ino,
					     darg.dname, darg.ino, 0));
	/* It's okay if the above call fails */
	free(darg.dname);

	return ret;
}

static int ext4_fc_handle_link_and_create(e2fsck_t ctx, struct ext4_fc_tl *tl, __u8 *val)
{
	struct dentry_info_args darg;
	ext2_filsys fs = ctx->fs;
	struct ext2_inode_large inode_large;
	int ret, filetype, mode;

	ret = tl_to_darg(&darg, tl, val);
	if (ret)
		return ret;
	ext4_fc_flush_extents(ctx, 0);
	ret = errcode_to_errno(ext2fs_read_inode(fs, darg.ino,
						 (struct ext2_inode *)&inode_large));
	if (ret)
		goto out;

	mode = inode_large.i_mode;

	if (LINUX_S_ISREG(mode))
		filetype = EXT2_FT_REG_FILE;
	else if (LINUX_S_ISDIR(mode))
		filetype = EXT2_FT_DIR;
	else if (LINUX_S_ISCHR(mode))
		filetype = EXT2_FT_CHRDEV;
	else if (LINUX_S_ISBLK(mode))
		filetype = EXT2_FT_BLKDEV;
	else if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;
	else if (LINUX_S_ISFIFO(mode))
		filetype = EXT2_FT_FIFO;
	else if (LINUX_S_ISSOCK(mode))
		filetype = EXT2_FT_SOCK;
	else {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Forcefully unlink if the same name is present and ignore the error
	 * if any, since this dirent might not exist
	 */
	ext2fs_unlink(fs, darg.parent_ino, darg.dname, darg.ino,
			EXT2FS_UNLINK_FORCE);

	ret = errcode_to_errno(
		       ext2fs_link(fs, darg.parent_ino, darg.dname, darg.ino,
				   filetype));
out:
	free(darg.dname);
	return ret;

}

/* This function fixes the i_blocks field in the replayed indoe */
static void ext4_fc_replay_fixup_iblocks(struct ext2_inode_large *ondisk_inode,
	struct ext2_inode_large *fc_inode)
{
	if (ondisk_inode->i_flags & EXT4_EXTENTS_FL) {
		struct ext3_extent_header *eh;

		eh = (struct ext3_extent_header *)(&ondisk_inode->i_block[0]);
		if (le16_to_cpu(eh->eh_magic) != EXT3_EXT_MAGIC) {
			memset(eh, 0, sizeof(*eh));
			eh->eh_magic = cpu_to_le16(EXT3_EXT_MAGIC);
			eh->eh_max = cpu_to_le16(
				(sizeof(ondisk_inode->i_block) -
					sizeof(struct ext3_extent_header)) /
				sizeof(struct ext3_extent));
		}
	} else if (ondisk_inode->i_flags & EXT4_INLINE_DATA_FL) {
		memcpy(ondisk_inode->i_block, fc_inode->i_block,
			sizeof(fc_inode->i_block));
	}
}

static int ext4_fc_handle_inode(e2fsck_t ctx, __u8 *val)
{
	int ino, inode_len = EXT2_GOOD_OLD_INODE_SIZE;
	struct ext2_inode_large *inode = NULL, *fc_inode = NULL;
	__le32 fc_ino;
	__u8 *fc_raw_inode;
	errcode_t err;
	blk64_t blks;

	memcpy(&fc_ino, val, sizeof(fc_ino));
	fc_raw_inode = val + sizeof(fc_ino);
	ino = le32_to_cpu(fc_ino);

	if (EXT2_INODE_SIZE(ctx->fs->super) > EXT2_GOOD_OLD_INODE_SIZE) {
		__u16 extra_isize = ext2fs_le16_to_cpu(
			((struct ext2_inode_large *)fc_raw_inode)->i_extra_isize);

		if ((extra_isize < (sizeof(inode->i_extra_isize) +
				    sizeof(inode->i_checksum_hi))) ||
		    (extra_isize > (EXT2_INODE_SIZE(ctx->fs->super) -
				    EXT2_GOOD_OLD_INODE_SIZE))) {
			err = EFSCORRUPTED;
			goto out;
		}
		inode_len += extra_isize;
	}
	err = ext2fs_get_mem(inode_len, &inode);
	if (err)
		goto out;
	err = ext2fs_get_mem(inode_len, &fc_inode);
	if (err)
		goto out;
	ext4_fc_flush_extents(ctx, ino);

	err = ext2fs_read_inode_full(ctx->fs, ino, (struct ext2_inode *)inode,
					inode_len);
	if (err)
		goto out;
	memcpy(fc_inode, fc_raw_inode, inode_len);
#ifdef WORDS_BIGENDIAN
	ext2fs_swap_inode_full(ctx->fs, fc_inode, fc_inode, 0, inode_len);
#endif
	memcpy(inode, fc_inode, offsetof(struct ext2_inode_large, i_block));
	memcpy(&inode->i_generation, &fc_inode->i_generation,
		inode_len - offsetof(struct ext2_inode_large, i_generation));
	ext4_fc_replay_fixup_iblocks(inode, fc_inode);
	err = ext2fs_count_blocks(ctx->fs, ino, EXT2_INODE(inode), &blks);
	if (err)
		goto out;
	ext2fs_iblk_set(ctx->fs, EXT2_INODE(inode), blks);
	ext2fs_inode_csum_set(ctx->fs, ino, inode);

	err = ext2fs_write_inode_full(ctx->fs, ino, (struct ext2_inode *)inode,
					inode_len);
	if (err)
		goto out;
	if (inode->i_links_count)
		ext2fs_mark_inode_bitmap2(ctx->fs->inode_map, ino);
	else
		ext2fs_unmark_inode_bitmap2(ctx->fs->inode_map, ino);
	ext2fs_mark_ib_dirty(ctx->fs);

out:
	ext2fs_free_mem(&inode);
	ext2fs_free_mem(&fc_inode);
	return errcode_to_errno(err);
}

/*
 * Handle add extent replay tag.
 */
static int ext4_fc_handle_add_extent(e2fsck_t ctx, __u8 *val)
{
	struct ext2fs_extent extent;
	struct ext4_fc_add_range add_range;
	ext2_ino_t ino;
	int ret = 0;

	memcpy(&add_range, val, sizeof(add_range));
	ino = le32_to_cpu(add_range.fc_ino);
	ext4_fc_flush_extents(ctx, ino);

	ret = ext4_fc_read_extents(ctx, ino);
	if (ret)
		return ret;
	memset(&extent, 0, sizeof(extent));
	ret = errcode_to_errno(ext2fs_decode_extent(
			&extent, (void *)add_range.fc_ex,
			sizeof(add_range.fc_ex)));
	if (ret)
		return ret;
	return ext4_add_extent_to_list(ctx,
		&ctx->fc_replay_state.fc_extent_list, &extent);
}

/*
 * Handle delete logical range replay tag.
 */
static int ext4_fc_handle_del_range(e2fsck_t ctx, __u8 *val)
{
	struct ext2fs_extent extent;
	struct ext4_fc_del_range del_range;
	int ret, ino;

	memcpy(&del_range, val, sizeof(del_range));
	ino = le32_to_cpu(del_range.fc_ino);
	ext4_fc_flush_extents(ctx, ino);

	memset(&extent, 0, sizeof(extent));
	extent.e_lblk = le32_to_cpu(del_range.fc_lblk);
	extent.e_len = le32_to_cpu(del_range.fc_len);
	ret = ext4_fc_read_extents(ctx, ino);
	if (ret)
		return ret;
	return ext4_del_extent_from_list(ctx,
		&ctx->fc_replay_state.fc_extent_list, &extent);
}

/*
 * Main recovery path entry point. This function returns JBD2_FC_REPLAY_CONTINUE
 * to indicate that it is expecting more fast commit blocks. It returns
 * JBD2_FC_REPLAY_STOP to indicate that replay is done.
 */
static int ext4_fc_replay(journal_t *journal, struct buffer_head *bh,
				enum passtype pass, int off, tid_t expected_tid)
{
	e2fsck_t ctx = journal->j_fs_dev->k_ctx;
	struct e2fsck_fc_replay_state *state = &ctx->fc_replay_state;
	int ret = JBD2_FC_REPLAY_CONTINUE;
	struct ext4_fc_tl tl;
	__u8 *start, *end, *cur, *val;

	if (pass == PASS_SCAN) {
		state->fc_current_pass = PASS_SCAN;
		return ext4_fc_replay_scan(journal, bh, off, expected_tid);
	}

	if (state->fc_replay_num_tags == 0)
		goto replay_done;

	if (state->fc_current_pass != pass) {
		/* Starting replay phase */
		state->fc_current_pass = pass;
		/* We will reset checksums */
		ctx->fs->flags |= EXT2_FLAG_IGNORE_CSUM_ERRORS;
		ret = errcode_to_errno(ext2fs_read_bitmaps(ctx->fs));
		if (ret) {
			jbd_debug(1, "Error %d while reading bitmaps\n", ret);
			return ret;
		}
		state->fc_super_state = ctx->fs->super->s_state;
		/*
		 * Mark the file system to indicate it contains errors. That's
		 * because the updates performed by fast commit replay code are
		 * not atomic and may result in inconsistent file system if it
		 * crashes before the replay is complete.
		 */
		ctx->fs->super->s_state |= EXT2_ERROR_FS;
		ctx->fs->super->s_state |= EXT4_FC_REPLAY;
		ext2fs_mark_super_dirty(ctx->fs);
		ext2fs_flush(ctx->fs);
	}

	start = (__u8 *)bh->b_data;
	end = (__u8 *)bh->b_data + journal->j_blocksize - 1;

	for (cur = start; cur < end; cur = cur + le16_to_cpu(tl.fc_len) + sizeof(tl)) {
		memcpy(&tl, cur, sizeof(tl));
		val = cur + sizeof(tl);

		if (state->fc_replay_num_tags == 0)
			goto replay_done;
		jbd_debug(3, "Replay phase processing %s tag\n",
				tag2str(le16_to_cpu(tl.fc_tag)));
		state->fc_replay_num_tags--;
		switch (le16_to_cpu(tl.fc_tag)) {
		case EXT4_FC_TAG_CREAT:
		case EXT4_FC_TAG_LINK:
			ret = ext4_fc_handle_link_and_create(ctx, &tl, val);
			break;
		case EXT4_FC_TAG_UNLINK:
			ret = ext4_fc_handle_unlink(ctx, &tl, val);
			break;
		case EXT4_FC_TAG_ADD_RANGE:
			ret = ext4_fc_handle_add_extent(ctx, val);
			break;
		case EXT4_FC_TAG_DEL_RANGE:
			ret = ext4_fc_handle_del_range(ctx, val);
			break;
		case EXT4_FC_TAG_INODE:
			ret = ext4_fc_handle_inode(ctx, val);
			break;
		case EXT4_FC_TAG_TAIL:
			ext4_fc_flush_extents(ctx, 0);
		case EXT4_FC_TAG_PAD:
		case EXT4_FC_TAG_HEAD:
			break;
		default:
			ret = -ECANCELED;
			break;
		}
		if (ret < 0)
			break;
		ret = JBD2_FC_REPLAY_CONTINUE;
	}
	return ret;
replay_done:
	jbd_debug(1, "End of fast commit replay\n");
	if (state->fc_current_pass != pass)
		return JBD2_FC_REPLAY_STOP;

	ext2fs_calculate_summary_stats(ctx->fs, 0 /* update bg also */);
	ext2fs_write_block_bitmap(ctx->fs);
	ext2fs_write_inode_bitmap(ctx->fs);
	ext2fs_mark_super_dirty(ctx->fs);
	ext2fs_set_gdt_csum(ctx->fs);
	ctx->fs->super->s_state = state->fc_super_state;
	ext2fs_flush(ctx->fs);

	return JBD2_FC_REPLAY_STOP;
}

static errcode_t e2fsck_get_journal(e2fsck_t ctx, journal_t **ret_journal)
{
	struct process_block_struct pb;
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_super_block jsuper;
	struct problem_context	pctx;
	struct buffer_head 	*bh;
	struct inode		*j_inode = NULL;
	struct kdev_s		*dev_fs = NULL, *dev_journal;
	const char		*journal_name = 0;
	journal_t		*journal = NULL;
	errcode_t		retval = 0;
	io_manager		io_ptr = 0;
	unsigned long long	start = 0;
	int			ret;
	int			ext_journal = 0;
	int			tried_backup_jnl = 0;

	clear_problem_context(&pctx);

	journal = e2fsck_allocate_memory(ctx, sizeof(journal_t), "journal");
	if (!journal) {
		return EXT2_ET_NO_MEMORY;
	}

	dev_fs = e2fsck_allocate_memory(ctx, 2*sizeof(struct kdev_s), "kdev");
	if (!dev_fs) {
		retval = EXT2_ET_NO_MEMORY;
		goto errout;
	}
	dev_journal = dev_fs+1;

	dev_fs->k_ctx = dev_journal->k_ctx = ctx;
	dev_fs->k_dev = K_DEV_FS;
	dev_journal->k_dev = K_DEV_JOURNAL;

	journal->j_dev = dev_journal;
	journal->j_fs_dev = dev_fs;
	journal->j_inode = NULL;
	journal->j_blocksize = ctx->fs->blocksize;

	if (uuid_is_null(sb->s_journal_uuid)) {
		/*
		 * The full set of superblock sanity checks haven't
		 * been performed yet, so we need to do some basic
		 * checks here to avoid potential array overruns.
		 */
		if (!sb->s_journal_inum ||
		    (sb->s_journal_inum >
		     (ctx->fs->group_desc_count * sb->s_inodes_per_group))) {
			retval = EXT2_ET_BAD_INODE_NUM;
			goto errout;
		}
		j_inode = e2fsck_allocate_memory(ctx, sizeof(*j_inode),
						 "journal inode");
		if (!j_inode) {
			retval = EXT2_ET_NO_MEMORY;
			goto errout;
		}

		j_inode->i_ctx = ctx;
		j_inode->i_ino = sb->s_journal_inum;

		if ((retval = ext2fs_read_inode(ctx->fs,
						sb->s_journal_inum,
						&j_inode->i_ext2))) {
		try_backup_journal:
			if (sb->s_jnl_backup_type != EXT3_JNL_BACKUP_BLOCKS ||
			    tried_backup_jnl)
				goto errout;
			memset(&j_inode->i_ext2, 0, sizeof(struct ext2_inode));
			memcpy(&j_inode->i_ext2.i_block[0], sb->s_jnl_blocks,
			       EXT2_N_BLOCKS*4);
			j_inode->i_ext2.i_size_high = sb->s_jnl_blocks[15];
			j_inode->i_ext2.i_size = sb->s_jnl_blocks[16];
			j_inode->i_ext2.i_links_count = 1;
			j_inode->i_ext2.i_mode = LINUX_S_IFREG | 0600;
			e2fsck_use_inode_shortcuts(ctx, 1);
			ctx->stashed_ino = j_inode->i_ino;
			ctx->stashed_inode = &j_inode->i_ext2;
			tried_backup_jnl++;
		}
		if (!j_inode->i_ext2.i_links_count ||
		    !LINUX_S_ISREG(j_inode->i_ext2.i_mode) ||
		    (j_inode->i_ext2.i_flags & EXT4_ENCRYPT_FL)) {
			retval = EXT2_ET_NO_JOURNAL;
			goto try_backup_journal;
		}
		if (EXT2_I_SIZE(&j_inode->i_ext2) / journal->j_blocksize <
		    JBD2_MIN_JOURNAL_BLOCKS) {
			retval = EXT2_ET_JOURNAL_TOO_SMALL;
			goto try_backup_journal;
		}
		pb.last_block = -1;
		retval = ext2fs_block_iterate3(ctx->fs, j_inode->i_ino,
					       BLOCK_FLAG_HOLE, 0,
					       process_journal_block, &pb);
		if ((pb.last_block + 1) * ctx->fs->blocksize <
		    (int) EXT2_I_SIZE(&j_inode->i_ext2)) {
			retval = EXT2_ET_JOURNAL_TOO_SMALL;
			goto try_backup_journal;
		}
		if (tried_backup_jnl && !(ctx->options & E2F_OPT_READONLY)) {
			retval = ext2fs_write_inode(ctx->fs, sb->s_journal_inum,
						    &j_inode->i_ext2);
			if (retval)
				goto errout;
		}

		journal->j_total_len = EXT2_I_SIZE(&j_inode->i_ext2) /
			journal->j_blocksize;

#ifdef USE_INODE_IO
		retval = ext2fs_inode_io_intern2(ctx->fs, sb->s_journal_inum,
						 &j_inode->i_ext2,
						 &journal_name);
		if (retval)
			goto errout;

		io_ptr = inode_io_manager;
#else
		journal->j_inode = j_inode;
		ctx->journal_io = ctx->fs->io;
		if ((ret = jbd2_journal_bmap(journal, 0, &start)) != 0) {
			retval = (errcode_t) (-1 * ret);
			goto errout;
		}
#endif
	} else {
		ext_journal = 1;
		if (!ctx->journal_name) {
			char uuid[37];

			uuid_unparse(sb->s_journal_uuid, uuid);
			ctx->journal_name = blkid_get_devname(ctx->blkid,
							      "UUID", uuid);
			if (!ctx->journal_name)
				ctx->journal_name = blkid_devno_to_devname(sb->s_journal_dev);
		}
		journal_name = ctx->journal_name;

		if (!journal_name) {
			fix_problem(ctx, PR_0_CANT_FIND_JOURNAL, &pctx);
			retval = EXT2_ET_LOAD_EXT_JOURNAL;
			goto errout;
		}

		jfs_debug(1, "Using journal file %s\n", journal_name);
		io_ptr = unix_io_manager;
	}

#if 0
	test_io_backing_manager = io_ptr;
	io_ptr = test_io_manager;
#endif
#ifndef USE_INODE_IO
	if (ext_journal)
#endif
	{
		int flags = IO_FLAG_RW;
		if (!(ctx->mount_flags & EXT2_MF_ISROOT &&
		      ctx->mount_flags & EXT2_MF_READONLY))
			flags |= IO_FLAG_EXCLUSIVE;
		if ((ctx->mount_flags & EXT2_MF_READONLY) &&
		    (ctx->options & E2F_OPT_FORCE))
			flags &= ~IO_FLAG_EXCLUSIVE;


		retval = io_ptr->open(journal_name, flags,
				      &ctx->journal_io);
	}
	if (retval)
		goto errout;

	io_channel_set_blksize(ctx->journal_io, ctx->fs->blocksize);

	if (ext_journal) {
		blk64_t maxlen;

		start = ext2fs_journal_sb_start(ctx->fs->blocksize) - 1;
		bh = getblk(dev_journal, start, ctx->fs->blocksize);
		if (!bh) {
			retval = EXT2_ET_NO_MEMORY;
			goto errout;
		}
		ll_rw_block(REQ_OP_READ, 0, 1, &bh);
		if ((retval = bh->b_err) != 0) {
			brelse(bh);
			goto errout;
		}
		memcpy(&jsuper, start ? bh->b_data :  bh->b_data + SUPERBLOCK_OFFSET,
		       sizeof(jsuper));
#ifdef WORDS_BIGENDIAN
		if (jsuper.s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
			ext2fs_swap_super(&jsuper);
#endif
		if (jsuper.s_magic != EXT2_SUPER_MAGIC ||
		    !ext2fs_has_feature_journal_dev(&jsuper)) {
			fix_problem(ctx, PR_0_EXT_JOURNAL_BAD_SUPER, &pctx);
			retval = EXT2_ET_LOAD_EXT_JOURNAL;
			brelse(bh);
			goto errout;
		}
		/* Make sure the journal UUID is correct */
		if (memcmp(jsuper.s_uuid, ctx->fs->super->s_journal_uuid,
			   sizeof(jsuper.s_uuid))) {
			fix_problem(ctx, PR_0_JOURNAL_BAD_UUID, &pctx);
			retval = EXT2_ET_LOAD_EXT_JOURNAL;
			brelse(bh);
			goto errout;
		}

		/* Check the superblock checksum */
		if (ext2fs_has_feature_metadata_csum(&jsuper)) {
			struct struct_ext2_filsys fsx;
			struct ext2_super_block	superx;
			void *p;

			p = start ? bh->b_data : bh->b_data + SUPERBLOCK_OFFSET;
			memcpy(&fsx, ctx->fs, sizeof(fsx));
			memcpy(&superx, ctx->fs->super, sizeof(superx));
			fsx.super = &superx;
			ext2fs_set_feature_metadata_csum(fsx.super);
			if (!ext2fs_superblock_csum_verify(&fsx, p) &&
			    fix_problem(ctx, PR_0_EXT_JOURNAL_SUPER_CSUM_INVALID,
					&pctx)) {
				ext2fs_superblock_csum_set(&fsx, p);
				mark_buffer_dirty(bh);
			}
		}
		brelse(bh);

		maxlen = ext2fs_blocks_count(&jsuper);
		journal->j_total_len = (maxlen < 1ULL << 32) ? maxlen : (1ULL << 32) - 1;
		start++;
	}

	if (!(bh = getblk(dev_journal, start, journal->j_blocksize))) {
		retval = EXT2_ET_NO_MEMORY;
		goto errout;
	}

	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;
	if (ext2fs_has_feature_fast_commit(ctx->fs->super))
		journal->j_fc_replay_callback = ext4_fc_replay;
	else
		journal->j_fc_replay_callback = NULL;

#ifdef USE_INODE_IO
	if (j_inode)
		ext2fs_free_mem(&j_inode);
#endif

	*ret_journal = journal;
	e2fsck_use_inode_shortcuts(ctx, 0);
	return 0;

errout:
	e2fsck_use_inode_shortcuts(ctx, 0);
	if (dev_fs)
		ext2fs_free_mem(&dev_fs);
	if (j_inode)
		ext2fs_free_mem(&j_inode);
	if (journal)
		ext2fs_free_mem(&journal);
	return retval;
}

static errcode_t e2fsck_journal_fix_bad_inode(e2fsck_t ctx,
					      struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	int recover = ext2fs_has_feature_journal_needs_recovery(ctx->fs->super);
	int has_journal = ext2fs_has_feature_journal(ctx->fs->super);

	if (has_journal || sb->s_journal_inum) {
		/* The journal inode is bogus, remove and force full fsck */
		pctx->ino = sb->s_journal_inum;
		if (fix_problem(ctx, PR_0_JOURNAL_BAD_INODE, pctx)) {
			if (has_journal && sb->s_journal_inum)
				printf("*** journal has been deleted ***\n\n");
			ext2fs_clear_feature_journal(sb);
			sb->s_journal_inum = 0;
			memset(sb->s_jnl_blocks, 0, sizeof(sb->s_jnl_blocks));
			ctx->flags |= E2F_FLAG_JOURNAL_INODE;
			ctx->fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
			e2fsck_clear_recover(ctx, 1);
			return 0;
		}
		return EXT2_ET_CORRUPT_JOURNAL_SB;
	} else if (recover) {
		if (fix_problem(ctx, PR_0_JOURNAL_RECOVER_SET, pctx)) {
			e2fsck_clear_recover(ctx, 1);
			return 0;
		}
		return EXT2_ET_UNSUPP_FEATURE;
	}
	return 0;
}

#define V1_SB_SIZE	0x0024
static void clear_v2_journal_fields(journal_t *journal)
{
	e2fsck_t ctx = journal->j_dev->k_ctx;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	if (!fix_problem(ctx, PR_0_CLEAR_V2_JOURNAL, &pctx))
		return;

	ctx->flags |= E2F_FLAG_PROBLEMS_FIXED;
	memset(((char *) journal->j_superblock) + V1_SB_SIZE, 0,
	       ctx->fs->blocksize-V1_SB_SIZE);
	mark_buffer_dirty(journal->j_sb_buffer);
}


static errcode_t e2fsck_journal_load(journal_t *journal)
{
	e2fsck_t ctx = journal->j_dev->k_ctx;
	journal_superblock_t *jsb;
	struct buffer_head *jbh = journal->j_sb_buffer;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	ll_rw_block(REQ_OP_READ, 0, 1, &jbh);
	if (jbh->b_err) {
		com_err(ctx->device_name, jbh->b_err, "%s",
			_("reading journal superblock\n"));
		return jbh->b_err;
	}

	jsb = journal->j_superblock;
	/* If we don't even have JBD2_MAGIC, we probably have a wrong inode */
	if (jsb->s_header.h_magic != htonl(JBD2_MAGIC_NUMBER))
		return e2fsck_journal_fix_bad_inode(ctx, &pctx);

	switch (ntohl(jsb->s_header.h_blocktype)) {
	case JBD2_SUPERBLOCK_V1:
		journal->j_format_version = 1;
		if (jsb->s_feature_compat ||
		    jsb->s_feature_incompat ||
		    jsb->s_feature_ro_compat ||
		    jsb->s_nr_users)
			clear_v2_journal_fields(journal);
		break;

	case JBD2_SUPERBLOCK_V2:
		journal->j_format_version = 2;
		if (ntohl(jsb->s_nr_users) > 1 &&
		    uuid_is_null(ctx->fs->super->s_journal_uuid))
			clear_v2_journal_fields(journal);
		if (ntohl(jsb->s_nr_users) > 1) {
			fix_problem(ctx, PR_0_JOURNAL_UNSUPP_MULTIFS, &pctx);
			return EXT2_ET_JOURNAL_UNSUPP_VERSION;
		}
		break;

	/*
	 * These should never appear in a journal super block, so if
	 * they do, the journal is badly corrupted.
	 */
	case JBD2_DESCRIPTOR_BLOCK:
	case JBD2_COMMIT_BLOCK:
	case JBD2_REVOKE_BLOCK:
		return EXT2_ET_CORRUPT_JOURNAL_SB;

	/* If we don't understand the superblock major type, but there
	 * is a magic number, then it is likely to be a new format we
	 * just don't understand, so leave it alone. */
	default:
		return EXT2_ET_JOURNAL_UNSUPP_VERSION;
	}

	if (JBD2_HAS_INCOMPAT_FEATURE(journal, ~JBD2_KNOWN_INCOMPAT_FEATURES))
		return EXT2_ET_UNSUPP_FEATURE;

	if (JBD2_HAS_RO_COMPAT_FEATURE(journal, ~JBD2_KNOWN_ROCOMPAT_FEATURES))
		return EXT2_ET_RO_UNSUPP_FEATURE;

	/* Checksum v1-3 are mutually exclusive features. */
	if (jbd2_has_feature_csum2(journal) && jbd2_has_feature_csum3(journal))
		return EXT2_ET_CORRUPT_JOURNAL_SB;

	if (jbd2_journal_has_csum_v2or3(journal) &&
	    jbd2_has_feature_checksum(journal))
		return EXT2_ET_CORRUPT_JOURNAL_SB;

	if (!e2fsck_journal_verify_csum_type(journal, jsb) ||
	    !e2fsck_journal_sb_csum_verify(journal, jsb))
		return EXT2_ET_CORRUPT_JOURNAL_SB;

	if (jbd2_journal_has_csum_v2or3(journal))
		journal->j_csum_seed = jbd2_chksum(journal, ~0, jsb->s_uuid,
						   sizeof(jsb->s_uuid));

	/* We have now checked whether we know enough about the journal
	 * format to be able to proceed safely, so any other checks that
	 * fail we should attempt to recover from. */
	if (jsb->s_blocksize != htonl(journal->j_blocksize)) {
		com_err(ctx->program_name, EXT2_ET_CORRUPT_JOURNAL_SB,
			_("%s: no valid journal superblock found\n"),
			ctx->device_name);
		return EXT2_ET_CORRUPT_JOURNAL_SB;
	}

	if (ntohl(jsb->s_maxlen) < journal->j_total_len)
		journal->j_total_len = ntohl(jsb->s_maxlen);
	else if (ntohl(jsb->s_maxlen) > journal->j_total_len) {
		com_err(ctx->program_name, EXT2_ET_CORRUPT_JOURNAL_SB,
			_("%s: journal too short\n"),
			ctx->device_name);
		return EXT2_ET_CORRUPT_JOURNAL_SB;
	}

	journal->j_tail_sequence = ntohl(jsb->s_sequence);
	journal->j_transaction_sequence = journal->j_tail_sequence;
	journal->j_tail = ntohl(jsb->s_start);
	journal->j_first = ntohl(jsb->s_first);
	if (jbd2_has_feature_fast_commit(journal)) {
		if (ntohl(jsb->s_maxlen) - jbd2_journal_get_num_fc_blks(jsb)
			< JBD2_MIN_JOURNAL_BLOCKS) {
			com_err(ctx->program_name, EXT2_ET_CORRUPT_JOURNAL_SB,
				_("%s: incorrect fast commit blocks\n"),
				ctx->device_name);
			return EXT2_ET_CORRUPT_JOURNAL_SB;
		}
		journal->j_fc_last = ntohl(jsb->s_maxlen);
		journal->j_last = journal->j_fc_last -
					jbd2_journal_get_num_fc_blks(jsb);
		journal->j_fc_first = journal->j_last + 1;
	} else {
		journal->j_last = ntohl(jsb->s_maxlen);
	}

	return 0;
}

static void e2fsck_journal_reset_super(e2fsck_t ctx, journal_superblock_t *jsb,
				       journal_t *journal)
{
	char *p;
	union {
		uuid_t uuid;
		__u32 val[4];
	} u;
	__u32 new_seq = 0;
	int i;

	/* Leave a valid existing V1 superblock signature alone.
	 * Anything unrecognisable we overwrite with a new V2
	 * signature. */

	if (jsb->s_header.h_magic != htonl(JBD2_MAGIC_NUMBER) ||
	    jsb->s_header.h_blocktype != htonl(JBD2_SUPERBLOCK_V1)) {
		jsb->s_header.h_magic = htonl(JBD2_MAGIC_NUMBER);
		jsb->s_header.h_blocktype = htonl(JBD2_SUPERBLOCK_V2);
	}

	/* Zero out everything else beyond the superblock header */

	p = ((char *) jsb) + sizeof(journal_header_t);
	memset (p, 0, ctx->fs->blocksize-sizeof(journal_header_t));

	jsb->s_blocksize = htonl(ctx->fs->blocksize);
	jsb->s_maxlen = htonl(journal->j_total_len);
	jsb->s_first = htonl(1);

	/* Initialize the journal sequence number so that there is "no"
	 * chance we will find old "valid" transactions in the journal.
	 * This avoids the need to zero the whole journal (slow to do,
	 * and risky when we are just recovering the filesystem).
	 */
	uuid_generate(u.uuid);
	for (i = 0; i < 4; i ++)
		new_seq ^= u.val[i];
	jsb->s_sequence = htonl(new_seq);
	e2fsck_journal_sb_csum_set(journal, jsb);

	mark_buffer_dirty(journal->j_sb_buffer);
	ll_rw_block(REQ_OP_WRITE, 0, 1, &journal->j_sb_buffer);
}

static errcode_t e2fsck_journal_fix_corrupt_super(e2fsck_t ctx,
						  journal_t *journal,
						  struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	int recover = ext2fs_has_feature_journal_needs_recovery(ctx->fs->super);

	if (ext2fs_has_feature_journal(sb)) {
		if (fix_problem(ctx, PR_0_JOURNAL_BAD_SUPER, pctx)) {
			e2fsck_journal_reset_super(ctx, journal->j_superblock,
						   journal);
			journal->j_transaction_sequence = 1;
			e2fsck_clear_recover(ctx, recover);
			return 0;
		}
		return EXT2_ET_CORRUPT_JOURNAL_SB;
	} else if (e2fsck_journal_fix_bad_inode(ctx, pctx))
		return EXT2_ET_CORRUPT_JOURNAL_SB;

	return 0;
}

static void e2fsck_journal_release(e2fsck_t ctx, journal_t *journal,
				   int reset, int drop)
{
	journal_superblock_t *jsb;

	if (drop)
		mark_buffer_clean(journal->j_sb_buffer);
	else if (!(ctx->options & E2F_OPT_READONLY)) {
		jsb = journal->j_superblock;
		jsb->s_sequence = htonl(journal->j_tail_sequence);
		if (reset)
			jsb->s_start = 0; /* this marks the journal as empty */
		e2fsck_journal_sb_csum_set(journal, jsb);
		mark_buffer_dirty(journal->j_sb_buffer);
	}
	brelse(journal->j_sb_buffer);

	if (ctx->journal_io) {
		if (ctx->fs && ctx->fs->io != ctx->journal_io)
			io_channel_close(ctx->journal_io);
		ctx->journal_io = 0;
	}

#ifndef USE_INODE_IO
	if (journal->j_inode)
		ext2fs_free_mem(&journal->j_inode);
#endif
	if (journal->j_fs_dev)
		ext2fs_free_mem(&journal->j_fs_dev);
	ext2fs_free_mem(&journal);
}

/*
 * This function makes sure that the superblock fields regarding the
 * journal are consistent.
 */
errcode_t e2fsck_check_ext3_journal(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	journal_t *journal;
	int recover = ext2fs_has_feature_journal_needs_recovery(ctx->fs->super);
	struct problem_context pctx;
	problem_t problem;
	int reset = 0, force_fsck = 0;
	errcode_t retval;

	/* If we don't have any journal features, don't do anything more */
	if (!ext2fs_has_feature_journal(sb) &&
	    !recover && sb->s_journal_inum == 0 && sb->s_journal_dev == 0 &&
	    uuid_is_null(sb->s_journal_uuid))
 		return 0;

	clear_problem_context(&pctx);
	pctx.num = sb->s_journal_inum;

	retval = e2fsck_get_journal(ctx, &journal);
	if (retval) {
		if ((retval == EXT2_ET_BAD_INODE_NUM) ||
		    (retval == EXT2_ET_BAD_BLOCK_NUM) ||
		    (retval == EXT2_ET_JOURNAL_TOO_SMALL) ||
		    (retval == EXT2_ET_NO_JOURNAL))
			return e2fsck_journal_fix_bad_inode(ctx, &pctx);
		return retval;
	}

	retval = e2fsck_journal_load(journal);
	if (retval) {
		if ((retval == EXT2_ET_CORRUPT_JOURNAL_SB) ||
		    ((retval == EXT2_ET_UNSUPP_FEATURE) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_INCOMPAT,
				  &pctx))) ||
		    ((retval == EXT2_ET_RO_UNSUPP_FEATURE) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_ROCOMPAT,
				  &pctx))) ||
		    ((retval == EXT2_ET_JOURNAL_UNSUPP_VERSION) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_VERSION, &pctx))))
			retval = e2fsck_journal_fix_corrupt_super(ctx, journal,
								  &pctx);
		e2fsck_journal_release(ctx, journal, 0, 1);
		return retval;
	}

	/*
	 * We want to make the flags consistent here.  We will not leave with
	 * needs_recovery set but has_journal clear.  We can't get in a loop
	 * with -y, -n, or -p, only if a user isn't making up their mind.
	 */
no_has_journal:
	if (!ext2fs_has_feature_journal(sb)) {
		recover = ext2fs_has_feature_journal_needs_recovery(sb);
		if (fix_problem(ctx, PR_0_JOURNAL_HAS_JOURNAL, &pctx)) {
			if (recover &&
			    !fix_problem(ctx, PR_0_JOURNAL_RECOVER_SET, &pctx))
				goto no_has_journal;
			/*
			 * Need a full fsck if we are releasing a
			 * journal stored on a reserved inode.
			 */
			force_fsck = recover ||
				(sb->s_journal_inum < EXT2_FIRST_INODE(sb));
			/* Clear all of the journal fields */
			sb->s_journal_inum = 0;
			sb->s_journal_dev = 0;
			memset(sb->s_journal_uuid, 0,
			       sizeof(sb->s_journal_uuid));
			e2fsck_clear_recover(ctx, force_fsck);
		} else if (!(ctx->options & E2F_OPT_READONLY)) {
			ext2fs_set_feature_journal(sb);
			ctx->fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
			ext2fs_mark_super_dirty(ctx->fs);
		}
	}

	if (ext2fs_has_feature_journal(sb) &&
	    !ext2fs_has_feature_journal_needs_recovery(sb) &&
	    journal->j_superblock->s_start != 0) {
		/* Print status information */
		fix_problem(ctx, PR_0_JOURNAL_RECOVERY_CLEAR, &pctx);
		if (ctx->superblock)
			problem = PR_0_JOURNAL_RUN_DEFAULT;
		else
			problem = PR_0_JOURNAL_RUN;
		if (fix_problem(ctx, problem, &pctx)) {
			ctx->options |= E2F_OPT_FORCE;
			ext2fs_set_feature_journal_needs_recovery(sb);
			ext2fs_mark_super_dirty(ctx->fs);
		} else if (fix_problem(ctx,
				       PR_0_JOURNAL_RESET_JOURNAL, &pctx)) {
			reset = 1;
			sb->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(ctx->fs);
		}
		/*
		 * If the user answers no to the above question, we
		 * ignore the fact that journal apparently has data;
		 * accidentally replaying over valid data would be far
		 * worse than skipping a questionable recovery.
		 *
		 * XXX should we abort with a fatal error here?  What
		 * will the ext3 kernel code do if a filesystem with
		 * !NEEDS_RECOVERY but with a non-zero
		 * journal->j_superblock->s_start is mounted?
		 */
	}

	/*
	 * If we don't need to do replay the journal, check to see if
	 * the journal's errno is set; if so, we need to mark the file
	 * system as being corrupt and clear the journal's s_errno.
	 */
	if (!ext2fs_has_feature_journal_needs_recovery(sb) &&
	    journal->j_superblock->s_errno) {
		ctx->fs->super->s_state |= EXT2_ERROR_FS;
		ext2fs_mark_super_dirty(ctx->fs);
		journal->j_superblock->s_errno = 0;
		e2fsck_journal_sb_csum_set(journal, journal->j_superblock);
		mark_buffer_dirty(journal->j_sb_buffer);
	}

	e2fsck_journal_release(ctx, journal, reset, 0);
	return retval;
}

static errcode_t recover_ext3_journal(e2fsck_t ctx)
{
	struct problem_context	pctx;
	journal_t *journal;
	errcode_t retval;

	clear_problem_context(&pctx);

	retval = jbd2_journal_init_revoke_record_cache();
	if (retval)
		return retval;

	retval = jbd2_journal_init_revoke_table_cache();
	if (retval)
		return retval;

	retval = e2fsck_get_journal(ctx, &journal);
	if (retval)
		return retval;

	retval = e2fsck_journal_load(journal);
	if (retval)
		goto errout;

	retval = jbd2_journal_init_revoke(journal, 1024);
	if (retval)
		goto errout;

	retval = -jbd2_journal_recover(journal);
	if (retval)
		goto errout;

	if (journal->j_failed_commit) {
		pctx.ino = journal->j_failed_commit;
		fix_problem(ctx, PR_0_JNL_TXN_CORRUPT, &pctx);
		journal->j_superblock->s_errno = -EINVAL;
		mark_buffer_dirty(journal->j_sb_buffer);
	}

	journal->j_tail_sequence = journal->j_transaction_sequence;

errout:
	jbd2_journal_destroy_revoke(journal);
	jbd2_journal_destroy_revoke_record_cache();
	jbd2_journal_destroy_revoke_table_cache();
	e2fsck_journal_release(ctx, journal, 1, 0);
	return retval;
}

errcode_t e2fsck_run_ext3_journal(e2fsck_t ctx)
{
	io_manager io_ptr = ctx->fs->io->manager;
	int blocksize = ctx->fs->blocksize;
	errcode_t	retval, recover_retval;
	io_stats	stats = 0;
	unsigned long long kbytes_written = 0;
	__u16 s_error_state;

	printf(_("%s: recovering journal\n"), ctx->device_name);
	if (ctx->options & E2F_OPT_READONLY) {
		printf(_("%s: won't do journal recovery while read-only\n"),
		       ctx->device_name);
		return EXT2_ET_FILE_RO;
	}

	if (ctx->fs->flags & EXT2_FLAG_DIRTY)
		ext2fs_flush(ctx->fs);	/* Force out any modifications */

	recover_retval = recover_ext3_journal(ctx);

	/*
	 * Reload the filesystem context to get up-to-date data from disk
	 * because journal recovery will change the filesystem under us.
	 */
	if (ctx->fs->super->s_kbytes_written &&
	    ctx->fs->io->manager->get_stats)
		ctx->fs->io->manager->get_stats(ctx->fs->io, &stats);
	if (stats && stats->bytes_written)
		kbytes_written = stats->bytes_written >> 10;
	s_error_state = ctx->fs->super->s_state & EXT2_ERROR_FS;

	ext2fs_mmp_stop(ctx->fs);
	ext2fs_free(ctx->fs);
	retval = ext2fs_open(ctx->filesystem_name, ctx->openfs_flags,
			     ctx->superblock, blocksize, io_ptr,
			     &ctx->fs);
	if (retval) {
		com_err(ctx->program_name, retval,
			_("while trying to re-open %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}
	ctx->fs->priv_data = ctx;
	ctx->fs->now = ctx->now;
	ctx->fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;
	ctx->fs->super->s_kbytes_written += kbytes_written;
	ctx->fs->super->s_state |= s_error_state;

	/* Set the superblock flags */
	e2fsck_clear_recover(ctx, recover_retval != 0);

	/*
	 * Do one last sanity check, and propagate journal->s_errno to
	 * the EXT2_ERROR_FS flag in the fs superblock if needed.
	 */
	retval = e2fsck_check_ext3_journal(ctx);
	return retval ? retval : recover_retval;
}

/*
 * This function will move the journal inode from a visible file in
 * the filesystem directory hierarchy to the reserved inode if necessary.
 */
static const char * const journal_names[] = {
	".journal", "journal", ".journal.dat", "journal.dat", 0 };

void e2fsck_move_ext3_journal(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct problem_context	pctx;
	struct ext2_inode 	inode;
	ext2_filsys		fs = ctx->fs;
	ext2_ino_t		ino;
	errcode_t		retval;
	const char * const *	cpp;
	dgrp_t			group;
	int			mount_flags;

	clear_problem_context(&pctx);

	/*
	 * If the filesystem is opened read-only, or there is no
	 * journal, then do nothing.
	 */
	if ((ctx->options & E2F_OPT_READONLY) ||
	    (sb->s_journal_inum == 0) ||
	    !ext2fs_has_feature_journal(sb))
		return;

	/*
	 * Read in the journal inode
	 */
	if (ext2fs_read_inode(fs, sb->s_journal_inum, &inode) != 0)
		return;

	/*
	 * If it's necessary to backup the journal inode, do so.
	 */
	if ((sb->s_jnl_backup_type == 0) ||
	    ((sb->s_jnl_backup_type == EXT3_JNL_BACKUP_BLOCKS) &&
	     memcmp(inode.i_block, sb->s_jnl_blocks, EXT2_N_BLOCKS*4))) {
		if (fix_problem(ctx, PR_0_BACKUP_JNL, &pctx)) {
			memcpy(sb->s_jnl_blocks, inode.i_block,
			       EXT2_N_BLOCKS*4);
			sb->s_jnl_blocks[15] = inode.i_size_high;
			sb->s_jnl_blocks[16] = inode.i_size;
			sb->s_jnl_backup_type = EXT3_JNL_BACKUP_BLOCKS;
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}

	/*
	 * If the journal is already the hidden inode, then do nothing
	 */
	if (sb->s_journal_inum == EXT2_JOURNAL_INO)
		return;

	/*
	 * The journal inode had better have only one link and not be readable.
	 */
	if (inode.i_links_count != 1)
		return;

	/*
	 * If the filesystem is mounted, or we can't tell whether
	 * or not it's mounted, do nothing.
	 */
	retval = ext2fs_check_if_mounted(ctx->filesystem_name, &mount_flags);
	if (retval || (mount_flags & EXT2_MF_MOUNTED))
		return;

	/*
	 * If we can't find the name of the journal inode, then do
	 * nothing.
	 */
	for (cpp = journal_names; *cpp; cpp++) {
		retval = ext2fs_lookup(fs, EXT2_ROOT_INO, *cpp,
				       strlen(*cpp), 0, &ino);
		if ((retval == 0) && (ino == sb->s_journal_inum))
			break;
	}
	if (*cpp == 0)
		return;

	/* We need the inode bitmap to be loaded */
	retval = ext2fs_read_bitmaps(fs);
	if (retval)
		return;

	pctx.str = *cpp;
	if (!fix_problem(ctx, PR_0_MOVE_JOURNAL, &pctx))
		return;

	/*
	 * OK, we've done all the checks, let's actually move the
	 * journal inode.  Errors at this point mean we need to force
	 * an ext2 filesystem check.
	 */
	if ((retval = ext2fs_unlink(fs, EXT2_ROOT_INO, *cpp, ino, 0)) != 0)
		goto err_out;
	if ((retval = ext2fs_write_inode(fs, EXT2_JOURNAL_INO, &inode)) != 0)
		goto err_out;
	sb->s_journal_inum = EXT2_JOURNAL_INO;
	ext2fs_mark_super_dirty(fs);
	fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	inode.i_links_count = 0;
	ext2fs_set_dtime(fs, &inode);
	if ((retval = ext2fs_write_inode(fs, ino, &inode)) != 0)
		goto err_out;

	group = ext2fs_group_of_ino(fs, ino);
	ext2fs_unmark_inode_bitmap2(fs->inode_map, ino);
	ext2fs_mark_ib_dirty(fs);
	ext2fs_bg_free_inodes_count_set(fs, group, ext2fs_bg_free_inodes_count(fs, group) + 1);
	ext2fs_group_desc_csum_set(fs, group);
	fs->super->s_free_inodes_count++;
	return;

err_out:
	pctx.errcode = retval;
	fix_problem(ctx, PR_0_ERR_MOVE_JOURNAL, &pctx);
	fs->super->s_state &= ~EXT2_VALID_FS;
	ext2fs_mark_super_dirty(fs);
	return;
}

/*
 * This function makes sure the superblock hint for the external
 * journal is correct.
 */
int e2fsck_fix_ext3_journal_hint(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct problem_context pctx;
	char uuid[37], *journal_name;
	struct stat st;

	if (!ext2fs_has_feature_journal(sb) ||
	    uuid_is_null(sb->s_journal_uuid))
 		return 0;

	uuid_unparse(sb->s_journal_uuid, uuid);
	journal_name = blkid_get_devname(ctx->blkid, "UUID", uuid);
	if (!journal_name)
		return 0;

	if (stat(journal_name, &st) < 0) {
		free(journal_name);
		return 0;
	}

	if (st.st_rdev != sb->s_journal_dev) {
		clear_problem_context(&pctx);
		pctx.num = st.st_rdev;
		if (fix_problem(ctx, PR_0_EXTERNAL_JOURNAL_HINT, &pctx)) {
			sb->s_journal_dev = st.st_rdev;
			ext2fs_mark_super_dirty(ctx->fs);
		}
	}

	free(journal_name);
	return 0;
}
