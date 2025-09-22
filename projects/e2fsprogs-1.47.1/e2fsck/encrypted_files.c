/*
 * encrypted_files.c --- save information about encrypted files
 *
 * Copyright 2019 Google LLC
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * e2fsck pass 1 (inode table scan) creates a map from inode number to
 * encryption policy for all encrypted inodes.  But it's optimized so that the
 * full xattrs aren't saved but rather only 32-bit "policy IDs", since usually
 * many inodes share the same encryption policy.  This requires also maintaining
 * a second map, from policy to policy ID.  See add_encrypted_file().
 *
 * We also use run-length encoding to save memory when many adjacent inodes
 * share the same encryption policy, which is often the case too.
 *
 * e2fsck pass 2 (directory structure check) uses the inode => policy ID map to
 * verify that all regular files, directories, and symlinks in encrypted
 * directories use the directory's encryption policy.
 */

#include "config.h"

#include "e2fsck.h"
#include "problem.h"
#include "ext2fs/rbtree.h"

#define FSCRYPT_KEY_DESCRIPTOR_SIZE	8
#define FSCRYPT_KEY_IDENTIFIER_SIZE	16
#define FS_KEY_DERIVATION_NONCE_SIZE	16

struct fscrypt_context_v1 {
	__u8 version;
	__u8 contents_encryption_mode;
	__u8 filenames_encryption_mode;
	__u8 flags;
	__u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
	__u8 nonce[FS_KEY_DERIVATION_NONCE_SIZE];
};

struct fscrypt_context_v2 {
	__u8 version;
	__u8 contents_encryption_mode;
	__u8 filenames_encryption_mode;
	__u8 flags;
	__u8 __reserved[4];
	__u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
	__u8 nonce[FS_KEY_DERIVATION_NONCE_SIZE];
};

/* On-disk format of encryption xattr */
union fscrypt_context {
	__u8 version;
	struct fscrypt_context_v1 v1;
	struct fscrypt_context_v2 v2;
};

struct fscrypt_policy_v1 {
	__u8 version;
	__u8 contents_encryption_mode;
	__u8 filenames_encryption_mode;
	__u8 flags;
	__u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
};

struct fscrypt_policy_v2 {
	__u8 version;
	__u8 contents_encryption_mode;
	__u8 filenames_encryption_mode;
	__u8 flags;
	__u8 __reserved[4];
	__u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
};

/* The encryption "policy" is the fscrypt_context excluding the nonce. */
union fscrypt_policy {
	__u8 version;
	struct fscrypt_policy_v1 v1;
	struct fscrypt_policy_v2 v2;
};

/* A range of inodes which share the same encryption policy */
struct encrypted_file_range {
	ext2_ino_t		first_ino;
	ext2_ino_t		last_ino;
	__u32			policy_id;
};

/* Information about the encrypted files which have been seen so far */
struct encrypted_file_info {
	/*
	 * Map from inode number to encryption policy ID, implemented as a
	 * sorted array of inode ranges, each of which shares the same policy.
	 * Inodes are added in order of increasing inode number.
	 *
	 * Freed after pass 2.
	 */
	struct encrypted_file_range	*file_ranges;
	size_t				file_ranges_count;
	size_t				file_ranges_capacity;

	/*
	 * Map from encryption policy to encryption policy ID, for the unique
	 * encryption policies that have been seen so far.  next_policy_id is
	 * the next available ID, starting at 0.
	 *
	 * Freed after pass 1.
	 */
	struct rb_root		policies;
	__u32			next_policy_id;
};

/* Entry in encrypted_file_info::policies */
struct policy_map_entry {
	union fscrypt_policy	policy;
	__u32			policy_id;
	struct rb_node		node;
};

static int cmp_fscrypt_policies(e2fsck_t ctx, const union fscrypt_policy *a,
				const union fscrypt_policy *b)
{
	if (a->version != b->version)
		return (int)a->version - (int)b->version;

	switch (a->version) {
	case 1:
		return memcmp(a, b, sizeof(a->v1));
	case 2:
		return memcmp(a, b, sizeof(a->v2));
	}
	fatal_error(ctx, "Unhandled encryption policy version");
	return 0;
}

/* Read an inode's encryption xattr. */
static errcode_t read_encryption_xattr(e2fsck_t ctx, ext2_ino_t ino,
				       void **value, size_t *value_len)
{
	struct ext2_xattr_handle *h;
	errcode_t retval;

	retval = ext2fs_xattrs_open(ctx->fs, ino, &h);
	if (retval)
		return retval;

	retval = ext2fs_xattrs_read(h);
	if (retval == 0)
		retval = ext2fs_xattr_get(h, "c", value, value_len);

	ext2fs_xattrs_close(&h);
	return retval;
}

/*
 * Convert an fscrypt_context to an fscrypt_policy.  Returns 0,
 * CORRUPT_ENCRYPTION_POLICY, or UNRECOGNIZED_ENCRYPTION_POLICY.
 */
static __u32 fscrypt_context_to_policy(const void *xattr, size_t xattr_size,
				       union fscrypt_policy *policy_u)
{
	const union fscrypt_context *ctx_u = xattr;

	if (xattr_size < 1)
		return CORRUPT_ENCRYPTION_POLICY;
	switch (ctx_u->version) {
	case 0:
		return CORRUPT_ENCRYPTION_POLICY;
	case 1: {
		struct fscrypt_policy_v1 *policy = &policy_u->v1;
		const struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		if (xattr_size != sizeof(*ctx))
			return CORRUPT_ENCRYPTION_POLICY;
		policy->version = ctx->version;
		policy->contents_encryption_mode =
			ctx->contents_encryption_mode;
		policy->filenames_encryption_mode =
			ctx->filenames_encryption_mode;
		policy->flags = ctx->flags;
		memcpy(policy->master_key_descriptor,
		       ctx->master_key_descriptor,
		       sizeof(policy->master_key_descriptor));
		return 0;
	}
	case 2: {
		struct fscrypt_policy_v2 *policy = &policy_u->v2;
		const struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		if (xattr_size != sizeof(*ctx))
			return CORRUPT_ENCRYPTION_POLICY;
		policy->version = ctx->version;
		policy->contents_encryption_mode =
			ctx->contents_encryption_mode;
		policy->filenames_encryption_mode =
			ctx->filenames_encryption_mode;
		policy->flags = ctx->flags;
		memcpy(policy->__reserved, ctx->__reserved,
		       sizeof(policy->__reserved));
		memcpy(policy->master_key_identifier,
		       ctx->master_key_identifier,
		       sizeof(policy->master_key_identifier));
		return 0;
	}
	}
	return UNRECOGNIZED_ENCRYPTION_POLICY;
}

/*
 * Read an inode's encryption xattr and get/allocate its encryption policy ID,
 * or alternatively use one of the special IDs NO_ENCRYPTION_POLICY,
 * CORRUPT_ENCRYPTION_POLICY, or UNRECOGNIZED_ENCRYPTION_POLICY.
 *
 * Returns nonzero only if out of memory.
 */
static errcode_t get_encryption_policy_id(e2fsck_t ctx, ext2_ino_t ino,
					  __u32 *policy_id_ret)
{
	struct encrypted_file_info *info = ctx->encrypted_files;
	struct rb_node **new = &info->policies.rb_node;
	struct rb_node *parent = NULL;
	void *xattr;
	size_t xattr_size;
	union fscrypt_policy policy;
	__u32 policy_id;
	struct policy_map_entry *entry;
	errcode_t retval;

	retval = read_encryption_xattr(ctx, ino, &xattr, &xattr_size);
	if (retval == EXT2_ET_NO_MEMORY)
		return retval;
	if (retval) {
		*policy_id_ret = NO_ENCRYPTION_POLICY;
		return 0;
	}

	/* Translate the xattr to an fscrypt_policy, if possible. */
	policy_id = fscrypt_context_to_policy(xattr, xattr_size, &policy);
	ext2fs_free_mem(&xattr);
	if (policy_id != 0)
		goto out;

	/* Check if the policy was already seen. */
	while (*new) {
		int res;

		parent = *new;
		entry = ext2fs_rb_entry(parent, struct policy_map_entry, node);
		res = cmp_fscrypt_policies(ctx, &policy, &entry->policy);
		if (res < 0) {
			new = &parent->rb_left;
		} else if (res > 0) {
			new = &parent->rb_right;
		} else {
			/* Policy already seen.  Use existing ID. */
			policy_id = entry->policy_id;
			goto out;
		}
	}

	/* First time seeing this policy.  Allocate a new policy ID. */
	retval = ext2fs_get_mem(sizeof(*entry), &entry);
	if (retval)
		goto out;
	policy_id = info->next_policy_id++;
	entry->policy_id = policy_id;
	entry->policy = policy;
	ext2fs_rb_link_node(&entry->node, parent, new);
	ext2fs_rb_insert_color(&entry->node, &info->policies);
out:
	*policy_id_ret = policy_id;
	return retval;
}

static int handle_nomem(e2fsck_t ctx, struct problem_context *pctx,
			size_t size_needed)
{
	pctx->num = size_needed;
	fix_problem(ctx, PR_1_ALLOCATE_ENCRYPTED_INODE_LIST, pctx);
	/* Should never get here */
	ctx->flags |= E2F_FLAG_ABORT;
	return 0;
}

static int append_ino_and_policy_id(e2fsck_t ctx, struct problem_context *pctx,
				    ext2_ino_t ino, __u32 policy_id)
{
	struct encrypted_file_info *info = ctx->encrypted_files;
	struct encrypted_file_range *range;

	/* See if we can just extend the last range. */
	if (info->file_ranges_count > 0) {
		range = &info->file_ranges[info->file_ranges_count - 1];

		if (ino <= range->last_ino) {
			/* Should never get here */
			fatal_error(ctx,
				    "Encrypted inodes processed out of order");
		}

		if (ino == range->last_ino + 1 &&
		    policy_id == range->policy_id) {
			range->last_ino++;
			return 0;
		}
	}
	/* Nope, a new range is needed. */

	if (info->file_ranges_count == info->file_ranges_capacity) {
		/* Double the capacity by default. */
		size_t new_capacity = info->file_ranges_capacity * 2;

		/* ... but go from 0 to 128 right away. */
		if (new_capacity < 128)
			new_capacity = 128;

		/* We won't need more than the filesystem's inode count. */
		if (new_capacity > ctx->fs->super->s_inodes_count)
			new_capacity = ctx->fs->super->s_inodes_count;

		/* To be safe, ensure the capacity really increases. */
		if (new_capacity < info->file_ranges_capacity + 1)
			new_capacity = info->file_ranges_capacity + 1;

		if (ext2fs_resize_mem(info->file_ranges_capacity *
					sizeof(*range),
				      new_capacity * sizeof(*range),
				      &info->file_ranges) != 0)
			return handle_nomem(ctx, pctx,
					    new_capacity * sizeof(*range));

		info->file_ranges_capacity = new_capacity;
	}
	range = &info->file_ranges[info->file_ranges_count++];
	range->first_ino = ino;
	range->last_ino = ino;
	range->policy_id = policy_id;
	return 0;
}

/*
 * Handle an inode that has EXT4_ENCRYPT_FL set during pass 1.  Normally this
 * just finds the unique ID that identifies the inode's encryption policy
 * (allocating a new ID if needed), and adds the inode number and its policy ID
 * to the encrypted_file_info so that it's available in pass 2.
 *
 * But this also handles:
 * - If the inode doesn't have an encryption xattr at all, offer to clear the
 *   encrypt flag.
 * - If the encryption xattr is clearly corrupt, tell the caller that the whole
 *   inode should be cleared.
 * - To be future-proof: if the encryption xattr has an unrecognized version
 *   number, it *might* be valid, so we don't consider it invalid.  But we can't
 *   do much with it, so give all such policies the same ID,
 *   UNRECOGNIZED_ENCRYPTION_POLICY.
 *
 * Returns -1 if the inode should be cleared, otherwise 0.
 */
int add_encrypted_file(e2fsck_t ctx, struct problem_context *pctx)
{
	struct encrypted_file_info *info = ctx->encrypted_files;
	ext2_ino_t ino = pctx->ino;
	__u32 policy_id;

	/* Allocate the encrypted_file_info if needed. */
	if (info == NULL) {
		if (ext2fs_get_memzero(sizeof(*info), &info) != 0)
			return handle_nomem(ctx, pctx, sizeof(*info));
		ctx->encrypted_files = info;
	}

	/* Get a unique ID for this inode's encryption policy. */
	if (get_encryption_policy_id(ctx, ino, &policy_id) != 0)
		return handle_nomem(ctx, pctx, 0 /* unknown size */);
	if (policy_id == NO_ENCRYPTION_POLICY) {
		if (fix_problem(ctx, PR_1_MISSING_ENCRYPTION_XATTR, pctx)) {
			pctx->inode->i_flags &= ~EXT4_ENCRYPT_FL;
			e2fsck_write_inode(ctx, ino, pctx->inode, "pass1");
		}
		return 0;
	} else if (policy_id == CORRUPT_ENCRYPTION_POLICY) {
		if (fix_problem(ctx, PR_1_CORRUPT_ENCRYPTION_XATTR, pctx))
			return -1;
		return 0;
	}

	/* Store this ino => policy_id mapping in the encrypted_file_info. */
	return append_ino_and_policy_id(ctx, pctx, ino, policy_id);
}

/*
 * Find the ID of an inode's encryption policy, using the information saved
 * earlier.
 *
 * If the inode is encrypted, returns the policy ID or
 * UNRECOGNIZED_ENCRYPTION_POLICY.  Else, returns NO_ENCRYPTION_POLICY.
 */
__u32 find_encryption_policy(e2fsck_t ctx, ext2_ino_t ino)
{
	const struct encrypted_file_info *info = ctx->encrypted_files;
	size_t l, r;

	if (info == NULL)
		return NO_ENCRYPTION_POLICY;
	l = 0;
	r = info->file_ranges_count;
	while (l < r) {
		size_t m = l + (r - l) / 2;
		const struct encrypted_file_range *range =
			&info->file_ranges[m];

		if (ino < range->first_ino)
			r = m;
		else if (ino > range->last_ino)
			l = m + 1;
		else
			return range->policy_id;
	}
	return NO_ENCRYPTION_POLICY;
}

/* Destroy ctx->encrypted_files->policies */
void destroy_encryption_policy_map(e2fsck_t ctx)
{
	struct encrypted_file_info *info = ctx->encrypted_files;

	if (info) {
		struct rb_root *policies = &info->policies;

		while (!ext2fs_rb_empty_root(policies)) {
			struct policy_map_entry *entry;

			entry = ext2fs_rb_entry(policies->rb_node,
						struct policy_map_entry, node);
			ext2fs_rb_erase(&entry->node, policies);
			ext2fs_free_mem(&entry);
		}
		info->next_policy_id = 0;
	}
}

/* Destroy ctx->encrypted_files */
void destroy_encrypted_file_info(e2fsck_t ctx)
{
	struct encrypted_file_info *info = ctx->encrypted_files;

	if (info) {
		destroy_encryption_policy_map(ctx);
		ext2fs_free_mem(&info->file_ranges);
		ext2fs_free_mem(&info);
		ctx->encrypted_files = NULL;
	}
}
