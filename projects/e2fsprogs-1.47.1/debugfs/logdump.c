/*
 * logdump.c --- dump the contents of the journal out to a file
 *
 * Author: Stephen C. Tweedie, 2001  <sct@redhat.com>
 * Copyright (C) 2001 Red Hat, Inc.
 * Based on portions  Copyright (C) 1994 Theodore Ts'o.
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"
#include "blkid/blkid.h"
#include "jfs_user.h"
#if __GNUC_PREREQ (4, 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "ext2fs/fast_commit.h"
#if __GNUC_PREREQ (4, 6)
#pragma GCC diagnostic pop
#endif
#include <uuid/uuid.h>

enum journal_location {JOURNAL_IS_INTERNAL, JOURNAL_IS_EXTERNAL};

#define ANY_BLOCK ((blk64_t) -1)

static int		dump_all, dump_super, dump_old, dump_contents, dump_descriptors;
static int64_t		dump_counts;
static blk64_t		block_to_dump, bitmap_to_dump, inode_block_to_dump;
static unsigned int	group_to_dump, inode_offset_to_dump;
static ext2_ino_t	inode_to_dump;
static bool		wrapped_flag;

struct journal_source
{
	enum journal_location where;
	int fd;
	ext2_file_t file;
};

static void dump_journal(char *, FILE *, struct journal_source *);

static void dump_descriptor_block(FILE *, struct journal_source *,
				  char *, journal_superblock_t *,
				  unsigned int *, unsigned int, __u32, tid_t);

static void dump_revoke_block(FILE *, char *, journal_superblock_t *,
				  unsigned int, unsigned int, tid_t);

static void dump_metadata_block(FILE *, struct journal_source *,
				journal_superblock_t*,
				unsigned int, unsigned int, unsigned int,
				unsigned int, tid_t);

static void dump_fc_block(FILE *out_file, char *buf, int blocksize,
			  tid_t transaction, int *fc_done);

static void do_hexdump (FILE *, char *, int);

#define WRAP(jsb, blocknr, maxlen)					\
	if (blocknr >= (maxlen)) {					\
		blocknr -= (maxlen - be32_to_cpu((jsb)->s_first));	\
		wrapped_flag = true;					\
	}

void do_logdump(int argc, ss_argv_t argv, int sci_idx EXT2FS_ATTR((unused)),
		    void *infop EXT2FS_ATTR((unused)))
{
	int		c;
	int		retval;
	char		*out_fn;
	FILE		*out_file;

	char		*inode_spec = NULL;
	char		*journal_fn = NULL;
	int		journal_fd = -1;
	int		use_sb = 0;
	ext2_ino_t	journal_inum;
	struct ext2_inode journal_inode;
	ext2_file_t 	journal_file = NULL;
	char		*tmp;
	struct journal_source journal_source;
	struct ext2_super_block *es = NULL;

	journal_source.where = JOURNAL_IS_INTERNAL;
	journal_source.fd = 0;
	journal_source.file = 0;
	dump_all = 0;
	dump_old = 0;
	dump_contents = 0;
	dump_super = 0;
	dump_descriptors = 1;
	block_to_dump = ANY_BLOCK;
	bitmap_to_dump = -1;
	inode_block_to_dump = ANY_BLOCK;
	inode_to_dump = -1;
	dump_counts = -1;
	wrapped_flag = false;

	reset_getopt();
	while ((c = getopt (argc, argv, "ab:ci:f:OsSn:")) != EOF) {
		switch (c) {
		case 'a':
			dump_all++;
			break;
		case 'b':
			block_to_dump = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(argv[0], 0,
					"Bad block number - %s", optarg);
				return;
			}
			dump_descriptors = 0;
			break;
		case 'c':
			dump_contents++;
			break;
		case 'f':
			journal_fn = optarg;
			break;
		case 'i':
			inode_spec = optarg;
			dump_descriptors = 0;
			break;
		case 'O':
			dump_old++;
			break;
		case 's':
			use_sb++;
			break;
		case 'S':
			dump_super++;
			break;
		case 'n':
			dump_counts = strtol(optarg, &tmp, 10);
			if (*tmp) {
				com_err(argv[0], 0,
					"Bad log counts number - %s", optarg);
				return;
			}
			break;
		default:
			goto print_usage;
		}
	}
	if (optind != argc && optind != argc-1) {
		goto print_usage;
	}

	if (inode_spec) {
		int inode_group, group_offset, inodes_per_block;

		if (check_fs_open(argv[0]))
			return;

		inode_to_dump = string_to_inode(inode_spec);
		if (!inode_to_dump)
			return;

		es = current_fs->super;
		inode_group = ((inode_to_dump - 1)
			       / es->s_inodes_per_group);
		group_offset = ((inode_to_dump - 1)
				% es->s_inodes_per_group);
		inodes_per_block = (current_fs->blocksize
				    / sizeof(struct ext2_inode));

		inode_block_to_dump =
			ext2fs_inode_table_loc(current_fs, inode_group) +
			(group_offset / inodes_per_block);
		inode_offset_to_dump = ((group_offset % inodes_per_block)
					* sizeof(struct ext2_inode));
		printf("Inode %u is at group %u, block %llu, offset %u\n",
		       inode_to_dump, inode_group,
		       (unsigned long long) inode_block_to_dump,
		       inode_offset_to_dump);
	}

	if (optind == argc) {
		out_file = stdout;
	} else {
		out_fn = argv[optind];
		out_file = fopen(out_fn, "w");
		if (!out_file) {
			com_err(argv[0], errno, "while opening %s for logdump",
				out_fn);
			goto cleanup;
		}
	}

	if (block_to_dump != ANY_BLOCK) {
		if (check_fs_open(argv[0]))
			goto cleanup;
		es = current_fs->super;
		group_to_dump = ((block_to_dump -
				  es->s_first_data_block)
				 / es->s_blocks_per_group);
		bitmap_to_dump = ext2fs_block_bitmap_loc(current_fs, group_to_dump);
	}

	if (journal_fn) {
		/* Set up to read journal from a regular file somewhere */
		journal_fd = open(journal_fn, O_RDONLY, 0);
		if (journal_fd < 0) {
			com_err(argv[0], errno, "while opening %s for logdump",
				journal_fn);
			goto cleanup;
		}
		journal_source.where = JOURNAL_IS_EXTERNAL;
		journal_source.fd = journal_fd;
		dump_journal(argv[0], out_file, &journal_source);
		goto cleanup;

	}
	if (check_fs_open(argv[0]))
		goto cleanup;
	es = current_fs->super;

	if ((journal_inum = es->s_journal_inum)) {
		if (use_sb) {
			if (es->s_jnl_backup_type != EXT3_JNL_BACKUP_BLOCKS) {
				com_err(argv[0], 0,
					"no journal backup in super block\n");
				goto cleanup;
			}
			memset(&journal_inode, 0, sizeof(struct ext2_inode));
			memcpy(&journal_inode.i_block[0], es->s_jnl_blocks,
			       EXT2_N_BLOCKS*4);
			journal_inode.i_size_high = es->s_jnl_blocks[15];
			journal_inode.i_size = es->s_jnl_blocks[16];
			journal_inode.i_links_count = 1;
			journal_inode.i_mode = LINUX_S_IFREG | 0600;
		} else {
			if (debugfs_read_inode(journal_inum, &journal_inode,
					       argv[0]))
				goto cleanup;
		}

		retval = ext2fs_file_open2(current_fs, journal_inum,
					   &journal_inode, 0, &journal_file);
		if (retval) {
			com_err(argv[0], retval, "while opening ext2 file");
			goto cleanup;
		}
		journal_source.where = JOURNAL_IS_INTERNAL;
		journal_source.file = journal_file;
	} else {
		char uuid[37];

		uuid_unparse(es->s_journal_uuid, uuid);
		journal_fn = blkid_get_devname(NULL, "UUID", uuid);
		if (!journal_fn)
				journal_fn = blkid_devno_to_devname(es->s_journal_dev);
		if (!journal_fn) {
			com_err(argv[0], 0, "filesystem has no journal");
			goto cleanup;
		}
		journal_fd = open(journal_fn, O_RDONLY, 0);
		if (journal_fd < 0) {
			com_err(argv[0], errno, "while opening %s for logdump",
				journal_fn);
			free(journal_fn);
			goto cleanup;
		}
		fprintf(out_file, "Using external journal found at %s\n",
			journal_fn);
		free(journal_fn);
		journal_source.where = JOURNAL_IS_EXTERNAL;
		journal_source.fd = journal_fd;
	}
	dump_journal(argv[0], out_file, &journal_source);
cleanup:
	if (journal_fd >= 0)
		close(journal_fd);
	if (journal_file)
		ext2fs_file_close(journal_file);
	if (out_file && (out_file != stdout))
		fclose(out_file);

	return;

print_usage:
	fprintf(stderr, "%s: Usage: logdump [-acsOS] [-n<num_trans>] [-b<block>] [-i<filespec>]\n\t"
		"[-f<journal_file>] [output_file]\n", argv[0]);
}


static int read_journal_block(const char *cmd, struct journal_source *source,
			      ext2_loff_t offset, char *buf, unsigned int size)
{
	int retval;
	unsigned int got;

	if (source->where == JOURNAL_IS_EXTERNAL) {
		if (lseek(source->fd, offset, SEEK_SET) < 0) {
			retval = errno;
			goto seek_err;
		}
		retval = read(source->fd, buf, size);
		if (retval < 0) {
			retval = errno;
			goto read_err;
		}
		got = retval;
		retval = 0;
	} else {
		retval = ext2fs_file_llseek(source->file, offset,
					    EXT2_SEEK_SET, NULL);
		if (retval) {
		seek_err:
			com_err(cmd, retval, "while seeking in reading journal");
			return retval;
		}
		retval = ext2fs_file_read(source->file, buf, size, &got);
		if (retval) {
		read_err:
			com_err(cmd, retval, "while reading journal");
			return retval;
		}
	}
	if (got != size) {
		com_err(cmd, 0, "short read (read %u, expected %u) "
			"while reading journal", got, size);
		retval = -1;
	}
	return retval;
}

static const char *type_to_name(int btype)
{
	switch (btype) {
	case JBD2_DESCRIPTOR_BLOCK:
		return "descriptor block";
	case JBD2_COMMIT_BLOCK:
		return "commit block";
	case JBD2_SUPERBLOCK_V1:
		return "V1 superblock";
	case JBD2_SUPERBLOCK_V2:
		return "V2 superblock";
	case JBD2_REVOKE_BLOCK:
		return "revoke table";
	}
	return "unrecognised type";
}


static void dump_journal(char *cmdname, FILE *out_file,
			 struct journal_source *source)
{
	struct ext2_super_block *sb;
	char			jsb_buffer[1024];
	char			buf[EXT2_MAX_BLOCK_SIZE];
	journal_superblock_t	*jsb;
	unsigned int		blocksize = 1024;
	int			retval;
	__u32			magic, sequence, blocktype;
	journal_header_t	*header;
	tid_t			transaction;
	unsigned int		blocknr = 0;
	unsigned int		first_transaction_blocknr;
	int			fc_done;
	__u64			total_len;
	__u32			maxlen;
	int64_t			cur_counts = 0;
	bool			exist_no_magic = false;

	/* First, check to see if there's an ext2 superblock header */
	retval = read_journal_block(cmdname, source, 0, buf, 2048);
	if (retval)
		return;

	jsb = (journal_superblock_t *) buf;
	sb = (struct ext2_super_block *) (buf+1024);
#ifdef WORDS_BIGENDIAN
	if (sb->s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
		ext2fs_swap_super(sb);
#endif

	if ((be32_to_cpu(jsb->s_header.h_magic) != JBD2_MAGIC_NUMBER) &&
	    (sb->s_magic == EXT2_SUPER_MAGIC) &&
	    ext2fs_has_feature_journal_dev(sb)) {
		blocksize = EXT2_BLOCK_SIZE(sb);
		blocknr = (blocksize == 1024) ? 2 : 1;
		uuid_unparse(sb->s_uuid, jsb_buffer);
		fprintf(out_file, "Ext2 superblock header found.\n");
		if (dump_all) {
			fprintf(out_file, "\tuuid=%s\n", jsb_buffer);
			fprintf(out_file, "\tblocksize=%d\n", blocksize);
			fprintf(out_file, "\tjournal data size %lu\n",
				(unsigned long) ext2fs_blocks_count(sb));
		}
	}

	/* Next, read the journal superblock */
	retval = read_journal_block(cmdname, source,
				    ((ext2_loff_t) blocknr) * blocksize,
				    jsb_buffer, 1024);
	if (retval)
		return;

	if (dump_super) {
		e2p_list_journal_super(out_file, jsb_buffer,
				       current_fs->blocksize, 0);
		fputc('\n', out_file);
	}

	jsb = (journal_superblock_t *) jsb_buffer;
	if (be32_to_cpu(jsb->s_header.h_magic) != JBD2_MAGIC_NUMBER) {
		fprintf(out_file,
			"Journal superblock magic number invalid!\n");
		return;
	}
	blocksize = be32_to_cpu(jsb->s_blocksize);
	if ((current_fs && (blocksize != current_fs->blocksize)) ||
	    (!current_fs && (!blocksize || (blocksize & (blocksize - 1)) ||
			     (blocksize > EXT2_MAX_BLOCK_SIZE)))) {
		fprintf(out_file,
			"Journal block size invalid: %u (%u)\n",
			be32_to_cpu(jsb->s_blocksize), blocksize);
		return;
	}
	transaction = be32_to_cpu(jsb->s_sequence);
	blocknr = be32_to_cpu(jsb->s_start);
	if (source->where == JOURNAL_IS_INTERNAL) {
		retval = ext2fs_file_get_lsize(source->file, &total_len);
		if (retval) {
		stat_err:
			com_err("dump_journal", retval,
				"while getting journal inode size");
			return;
		}
		total_len /= blocksize;
	} else {
			struct stat st;

			if (fstat(source->fd, &st) < 0)
				goto stat_err;
			total_len = st.st_size / blocksize;
	}
	maxlen = be32_to_cpu(jsb->s_maxlen);
	if (maxlen > total_len)
		maxlen = total_len;

	fprintf(out_file, "Journal starts at block %u, transaction %u\n",
		blocknr, transaction);

	if (!blocknr) {
		/* Empty journal, nothing to do. */
		if (!dump_old)
			goto fc;
		else
			blocknr = 1;
	}

	first_transaction_blocknr = blocknr;

	while (1) {
		if (dump_old && (dump_counts != -1) && (cur_counts >= dump_counts))
			break;

		if ((blocknr == first_transaction_blocknr) && dump_old && wrapped_flag) {
			fprintf(out_file, "Dump all %lld journal records.\n",
				(long long) cur_counts);
			break;
		}

		retval = read_journal_block(cmdname, source,
				((ext2_loff_t) blocknr) * blocksize,
				buf, blocksize);
		if (retval)
			break;

		header = (journal_header_t *) buf;

		magic = be32_to_cpu(header->h_magic);
		sequence = be32_to_cpu(header->h_sequence);
		blocktype = be32_to_cpu(header->h_blocktype);

		if (magic != JBD2_MAGIC_NUMBER) {
			if (exist_no_magic == false) {
				exist_no_magic = true;
				fprintf(out_file, "No magic number at block %u: "
					"end of journal.\n", blocknr);
			}
			if (dump_old && (dump_counts != -1)) {
				blocknr++;
				WRAP(jsb, blocknr, maxlen);
				continue;
			}
			break;
		}

		if (sequence != transaction) {
			fprintf (out_file, "Found sequence %u (not %u) at "
				 "block %u: end of journal.\n",
				 sequence, transaction, blocknr);
			if (!dump_old)
				break;
		}

		if (dump_descriptors) {
			fprintf (out_file, "Found expected sequence %u, "
				 "type %u (%s) at block %u\n",
				 sequence, blocktype,
				 type_to_name(blocktype), blocknr);
		}

		switch (blocktype) {
		case JBD2_DESCRIPTOR_BLOCK:
			dump_descriptor_block(out_file, source, buf, jsb,
					      &blocknr, blocksize, maxlen,
					      transaction);
			continue;

		case JBD2_COMMIT_BLOCK:
			cur_counts++;
			transaction++;
			blocknr++;
			WRAP(jsb, blocknr, maxlen);
			continue;

		case JBD2_REVOKE_BLOCK:
			dump_revoke_block(out_file, buf, jsb,
					  blocknr, blocksize,
					  transaction);
			blocknr++;
			WRAP(jsb, blocknr, maxlen);
			continue;

		default:
			fprintf (out_file, "Unexpected block type %u at "
				 "block %u.\n", blocktype, blocknr);
			break;
		}
	}

fc:
	blocknr = maxlen - jbd2_journal_get_num_fc_blks(jsb) + 1;
	while (blocknr <= maxlen) {
		retval = read_journal_block(cmdname, source,
				((ext2_loff_t) blocknr) * blocksize,
				buf, blocksize);
		if (retval)
			return;

		dump_fc_block(out_file, buf, blocksize, transaction, &fc_done);
		if (!dump_old && fc_done)
			break;
		blocknr++;
	}
}

static inline size_t journal_super_tag_bytes(journal_superblock_t *jsb)
{
	size_t sz;

	if (JSB_HAS_INCOMPAT_FEATURE(jsb, JBD2_FEATURE_INCOMPAT_CSUM_V3))
		return sizeof(journal_block_tag3_t);

	sz = sizeof(journal_block_tag_t);

	if (JSB_HAS_INCOMPAT_FEATURE(jsb, JBD2_FEATURE_INCOMPAT_CSUM_V2))
		sz += sizeof(__u16);

	if (JSB_HAS_INCOMPAT_FEATURE(jsb, JBD2_FEATURE_INCOMPAT_64BIT))
		return sz;

	return sz - sizeof(__u32);
}

static void dump_fc_block(FILE *out_file, char *buf, int blocksize,
			  tid_t transaction, int *fc_done)
{
	struct ext4_fc_tl	tl;
	struct ext4_fc_head	*head;
	struct ext4_fc_add_range	*add_range;
	struct ext4_fc_del_range	*del_range;
	struct ext4_fc_dentry_info	*dentry_info;
	struct ext4_fc_tail		*tail;
	struct ext3_extent	*ex;
	__u8			*cur, *val;

	*fc_done = 0;
	for (cur = (__u8 *)buf; cur < (__u8 *)buf + blocksize;
	     cur = cur + sizeof(tl) + le16_to_cpu(tl.fc_len)) {
		memcpy(&tl, cur, sizeof(tl));
		val = cur + sizeof(tl);

		switch (le16_to_cpu(tl.fc_tag)) {
		case EXT4_FC_TAG_ADD_RANGE:
			add_range = (struct ext4_fc_add_range *)val;
			ex = (struct ext3_extent *)add_range->fc_ex;
			fprintf(out_file,
				"tag %s, inode %d, lblk %u, pblk %llu, len %lu\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(add_range->fc_ino),
				le32_to_cpu(ex->ee_block),
				le32_to_cpu(ex->ee_start) +
				(((unsigned long long) le16_to_cpu(ex->ee_start_hi)) << 32),
				le16_to_cpu(ex->ee_len) > EXT_INIT_MAX_LEN ?
				le16_to_cpu(ex->ee_len) - EXT_INIT_MAX_LEN :
				le16_to_cpu(ex->ee_len));
			break;
		case EXT4_FC_TAG_DEL_RANGE:
			del_range = (struct ext4_fc_del_range *)val;
			fprintf(out_file, "tag %s, inode %d, lblk %d, len %d\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(del_range->fc_ino),
				le32_to_cpu(del_range->fc_lblk),
				le32_to_cpu(del_range->fc_len));
			break;
		case EXT4_FC_TAG_LINK:
		case EXT4_FC_TAG_UNLINK:
		case EXT4_FC_TAG_CREAT:
			dentry_info = (struct ext4_fc_dentry_info *)val;
			fprintf(out_file,
				"tag %s, parent %d, ino %d, name \"%s\"\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(dentry_info->fc_parent_ino),
				le32_to_cpu(dentry_info->fc_ino),
				dentry_info->fc_dname);
			break;
		case EXT4_FC_TAG_INODE:
			fprintf(out_file, "tag %s, inode %d\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(((struct ext4_fc_inode *)val)->fc_ino));
			break;
		case EXT4_FC_TAG_PAD:
			fprintf(out_file, "tag %s\n", tag2str(tl.fc_tag));
			break;
		case EXT4_FC_TAG_TAIL:
			tail = (struct ext4_fc_tail *)val;
			fprintf(out_file, "tag %s, tid %d\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(tail->fc_tid));
			if (!dump_old &&
				le32_to_cpu(tail->fc_tid) < transaction) {
				*fc_done = 1;
				return;
			}
			break;
		case EXT4_FC_TAG_HEAD:
			fprintf(out_file, "\n*** Fast Commit Area ***\n");
			head = (struct ext4_fc_head *)val;
			fprintf(out_file, "tag %s, features 0x%x, tid %d\n",
				tag2str(tl.fc_tag),
				le32_to_cpu(head->fc_features),
				le32_to_cpu(head->fc_tid));
			if (!dump_old &&
				le32_to_cpu(head->fc_tid) < transaction) {
				*fc_done = 1;
				return;
			}
			break;
		default:
			*fc_done = 1;
			break;
		}
	}
}

static void dump_descriptor_block(FILE *out_file,
				  struct journal_source *source,
				  char *buf,
				  journal_superblock_t *jsb,
				  unsigned int *blockp, unsigned blocksize,
				  __u32 maxlen,
				  tid_t transaction)
{
	unsigned		offset, tag_size, csum_size = 0;
	char			*tagp;
	journal_block_tag_t	*tag;
	unsigned int		blocknr;
	__u32			tag_block;
	__u32			tag_flags;

	tag_size = journal_super_tag_bytes(jsb);
	offset = sizeof(journal_header_t);
	blocknr = *blockp;

	if (JSB_HAS_INCOMPAT_FEATURE(jsb, JBD2_FEATURE_INCOMPAT_CSUM_V3) ||
	    JSB_HAS_INCOMPAT_FEATURE(jsb, JBD2_FEATURE_INCOMPAT_CSUM_V2))
		csum_size = sizeof(struct jbd2_journal_block_tail);

	if (dump_all)
		fprintf(out_file, "Dumping descriptor block, sequence %u, at "
			"block %u:\n", transaction, blocknr);

	++blocknr;
	WRAP(jsb, blocknr, maxlen);

	do {
		/* Work out the location of the current tag, and skip to
		 * the next one... */
		tagp = &buf[offset];
		tag = (journal_block_tag_t *) tagp;
		offset += tag_size;

		/* ... and if we have gone too far, then we've reached the
		   end of this block. */
		if (offset > blocksize - csum_size)
			break;

		tag_block = be32_to_cpu(tag->t_blocknr);
		tag_flags = be16_to_cpu(tag->t_flags);

		if (!(tag_flags & JBD2_FLAG_SAME_UUID))
			offset += 16;

		dump_metadata_block(out_file, source, jsb,
				    blocknr, tag_block, tag_flags, blocksize,
				    transaction);

		++blocknr;
		WRAP(jsb, blocknr, maxlen);

	} while (!(tag_flags & JBD2_FLAG_LAST_TAG));

	*blockp = blocknr;
}


static void dump_revoke_block(FILE *out_file, char *buf,
			      journal_superblock_t *jsb EXT2FS_ATTR((unused)),
			      unsigned int blocknr,
			      unsigned int blocksize,
			      tid_t transaction)
{
	unsigned int		offset, max;
	jbd2_journal_revoke_header_t *header;
	unsigned long long	rblock;
	int			tag_size = sizeof(__u32);

	if (dump_all)
		fprintf(out_file, "Dumping revoke block, sequence %u, at "
			"block %u:\n", transaction, blocknr);

	if (be32_to_cpu(jsb->s_feature_incompat) & JBD2_FEATURE_INCOMPAT_64BIT)
		tag_size = sizeof(__u64);

	header = (jbd2_journal_revoke_header_t *) buf;
	offset = sizeof(jbd2_journal_revoke_header_t);
	max = be32_to_cpu(header->r_count);
	if (max > blocksize) {
		fprintf(out_file, "Revoke block's r_count invalid: %u\b",
			max);
		max = blocksize;
	}

	while (offset < max) {
		if (tag_size == sizeof(__u32)) {
			__u32 *entry = (__u32 *) (buf + offset);
			rblock = be32_to_cpu(*entry);
		} else {
			__u64 *entry = (__u64 *) (buf + offset);
			rblock = ext2fs_be64_to_cpu(*entry);
		}
		if (dump_all || rblock == block_to_dump) {
			fprintf(out_file, "  Revoke FS block %llu",
				(unsigned long long) rblock);
			if (dump_all)
				fprintf(out_file, "\n");
			else
				fprintf(out_file," at block %u, sequence %u\n",
					blocknr, transaction);
		}
		offset += tag_size;
	}
}


static void show_extent(FILE *out_file, int start_extent, int end_extent,
			__u32 first_block)
{
	if (start_extent >= 0 && first_block != 0)
		fprintf(out_file, "(%d+%u): %u ",
			start_extent, end_extent-start_extent, first_block);
}

static void show_indirect(FILE *out_file, const char *name, __u32 where)
{
	if (where)
		fprintf(out_file, "(%s): %u ", name, where);
}


static void dump_metadata_block(FILE *out_file, struct journal_source *source,
				journal_superblock_t *jsb EXT2FS_ATTR((unused)),
				unsigned int log_blocknr,
				unsigned int fs_blocknr,
				unsigned int log_tag_flags,
				unsigned int blocksize,
				tid_t transaction)
{
	int		retval;
	char 		buf[EXT2_MAX_BLOCK_SIZE];

	if (!(dump_all
	      || (fs_blocknr == block_to_dump)
	      || (fs_blocknr == inode_block_to_dump)
	      || (fs_blocknr == bitmap_to_dump)))
		return;

	fprintf(out_file, "  FS block %u logged at ", fs_blocknr);
	if (!dump_all)
		fprintf(out_file, "sequence %u, ", transaction);
	fprintf(out_file, "journal block %u (flags 0x%x)\n", log_blocknr,
		log_tag_flags);

	/* There are two major special cases to parse:
	 *
	 * If this block is a block
	 * bitmap block, we need to give it special treatment so that we
	 * can log any allocates and deallocates which affect the
	 * block_to_dump query block.
	 *
	 * If the block is an inode block for the inode being searched
	 * for, then we need to dump the contents of that inode
	 * structure symbolically.
	 */

	if (!(dump_contents && dump_all)
	    && fs_blocknr != block_to_dump
	    && fs_blocknr != bitmap_to_dump
	    && fs_blocknr != inode_block_to_dump)
		return;

	retval = read_journal_block("logdump", source,
				    ((ext2_loff_t) log_blocknr) * blocksize,
				    buf, blocksize);
	if (retval)
		return;

	if (fs_blocknr == bitmap_to_dump) {
		struct ext2_super_block *super;
		int offset;

		super = current_fs->super;
		offset = ((block_to_dump - super->s_first_data_block) %
			  super->s_blocks_per_group);

		fprintf(out_file, "    (block bitmap for block %llu: "
			"block is %s)\n",
			(unsigned long long) block_to_dump,
			ext2fs_test_bit(offset, buf) ? "SET" : "CLEAR");
	}

	if (fs_blocknr == inode_block_to_dump) {
		struct ext2_inode *inode;
		int first, prev, this, start_extent, i;

		fprintf(out_file, "    (inode block for inode %u):\n",
			inode_to_dump);

		inode = (struct ext2_inode *) (buf + inode_offset_to_dump);
		internal_dump_inode(out_file, "    ", inode_to_dump, inode, 0);

		/* Dump out the direct/indirect blocks here:
		 * internal_dump_inode can only dump them from the main
		 * on-disk inode, not from the journaled copy of the
		 * inode. */

		fprintf (out_file, "    Blocks:  ");
		first = prev = start_extent = -1;

		for (i=0; i<EXT2_NDIR_BLOCKS; i++) {
			this = inode->i_block[i];
			if (start_extent >= 0  && this == prev+1) {
				prev = this;
				continue;
			} else {
				show_extent(out_file, start_extent, i, first);
				start_extent = i;
				first = prev = this;
			}
		}
		show_extent(out_file, start_extent, i, first);
		show_indirect(out_file, "IND", inode->i_block[i++]);
		show_indirect(out_file, "DIND", inode->i_block[i++]);
		show_indirect(out_file, "TIND", inode->i_block[i++]);

		fprintf(out_file, "\n");
	}

	if (dump_contents)
		do_hexdump(out_file, buf, blocksize);

}

static void do_hexdump (FILE *out_file, char *buf, int blocksize)
{
	int i,j;
	int *intp;
	char *charp;
	unsigned char c;

	intp = (int *) buf;
	charp = (char *) buf;

	for (i=0; i<blocksize; i+=16) {
		fprintf(out_file, "    %04x:  ", i);
		for (j=0; j<16; j+=4)
			fprintf(out_file, "%08x ", *intp++);
		for (j=0; j<16; j++) {
			c = *charp++;
			if (c < ' ' || c >= 127)
				c = '.';
			fprintf(out_file, "%c", c);
		}
		fprintf(out_file, "\n");
	}
}

