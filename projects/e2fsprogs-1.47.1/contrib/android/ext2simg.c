/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ext2fs/ext2fs.h>
#include <et/com_err.h>
#include <sparse/sparse.h>

struct {
	bool	crc;
	bool	sparse;
	bool	gzip;
	char	*in_file;
	char	*out_file;
	bool	overwrite_input;
} params = {
	.sparse	    = true,
};

#define ext2fs_fatal(Retval, Format, ...) \
	do { \
		com_err("error", Retval, Format, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while(0)

#define sparse_fatal(Format) \
	do { \
		fprintf(stderr, "sparse: "Format); \
		exit(EXIT_FAILURE); \
	} while(0)

static void usage(char *path)
{
	char *progname = basename(path);

	fprintf(stderr, "%s [ options ] <image or block device> <output image>\n"
			"  -c include CRC block\n"
			"  -z gzip output\n"
			"  -S don't use sparse output format\n", progname);
}

static struct buf_item {
	struct buf_item	    *next;
	void		    *buf[];
} *buf_list;

/*
 * Add @num_blks blocks, starting at index @chunk_start, of the filesystem @fs
 * to the sparse file @s.
 */
static void add_chunk(ext2_filsys fs, struct sparse_file *s,
		      blk_t chunk_start, int num_blks)
{
	uint64_t len = (uint64_t)num_blks * fs->blocksize;
	int64_t offset = (int64_t)chunk_start * fs->blocksize;
	struct buf_item *bi;
	int retval;

	if (!params.overwrite_input) {
		if (sparse_file_add_file(s, params.in_file, offset, len, chunk_start) < 0)
			sparse_fatal("adding data to the sparse file");
		return;
	}

	/* The input file will be overwritten, so make a copy of the blocks. */
	if (len > SIZE_MAX - sizeof(*bi))
		sparse_fatal("filesystem is too large");
	bi = calloc(1, sizeof(*bi) + len);
	if (!bi)
		sparse_fatal("out of memory");
	bi->next = buf_list;
	buf_list = bi;
	retval = io_channel_read_blk64(fs->io, chunk_start, num_blks, bi->buf);
	if (retval)
		ext2fs_fatal(retval, "reading data from %s", params.in_file);

	if (sparse_file_add_data(s, bi->buf, len, chunk_start) < 0)
		sparse_fatal("adding data to the sparse file");
}

static void free_chunks(void)
{
	struct buf_item *bi;

	while (buf_list) {
		bi = buf_list->next;
		free(buf_list);
		buf_list = bi;
	}
}

static blk_t fs_blocks_count(ext2_filsys fs)
{
	blk64_t blks = ext2fs_blocks_count(fs->super);

	/* libsparse assumes 32-bit block numbers. */
	if ((blk_t)blks != blks)
		sparse_fatal("filesystem is too large");
	return blks;
}

static struct sparse_file *ext_to_sparse(const char *in_file)
{
	errcode_t retval;
	ext2_filsys fs;
	struct sparse_file *s;
	int64_t chunk_start = -1;
	blk_t fs_blks, cur_blk;

	retval = ext2fs_open(in_file, 0, 0, 0, unix_io_manager, &fs);
	if (retval)
		ext2fs_fatal(retval, "while reading %s", in_file);

	retval = ext2fs_read_block_bitmap(fs);
	if (retval)
		ext2fs_fatal(retval, "while reading block bitmap of %s", in_file);

	fs_blks = fs_blocks_count(fs);

	s = sparse_file_new(fs->blocksize, (uint64_t)fs_blks * fs->blocksize);
	if (!s)
		sparse_fatal("creating sparse file");

	/*
	 * The sparse format encodes the size of a chunk (and its header) in a
	 * 32-bit unsigned integer (UINT32_MAX)
	 * When writing the chunk, the library uses a single call to write().
	 * Linux's implementation of the 'write' syscall does not allow transfers
	 * larger than INT32_MAX (32-bit _and_ 64-bit systems).
	 * Make sure we do not create chunks larger than this limit.
	 */
	int32_t max_blk_per_chunk = (INT32_MAX - 12) / fs->blocksize;

	/*
	 * Iterate through the filesystem's blocks, identifying "chunks" that
	 * are contiguous ranges of blocks that are in-use by the filesystem.
	 * Add each chunk to the sparse_file.
	 */
	for (cur_blk = ext2fs_get_block_bitmap_start2(fs->block_map);
	     cur_blk < fs_blks; ++cur_blk) {
		if (ext2fs_test_block_bitmap2(fs->block_map, cur_blk)) {
			/*
			 * @cur_blk is in-use.  Append it to the pending chunk
			 * if there is one, otherwise start a new chunk.
			 */
			if (chunk_start == -1) {
				chunk_start = cur_blk;
			} else if (cur_blk - chunk_start + 1 == max_blk_per_chunk) {
				/*
				 * Appending @cur_blk to the pending chunk made
				 * it reach the maximum length, so end it.
				 */
				add_chunk(fs, s, chunk_start, max_blk_per_chunk);
				chunk_start = -1;
			}
		} else if (chunk_start != -1) {
			/* @cur_blk is not in-use, so end the pending chunk. */
			add_chunk(fs, s, chunk_start, cur_blk - chunk_start);
			chunk_start = -1;
		}
	}
	/* If there's still a pending chunk, end it. */
	if (chunk_start != -1)
		add_chunk(fs, s, chunk_start, cur_blk - chunk_start);

	ext2fs_free(fs);
	return s;
}

static bool same_file(const char *in, const char *out)
{
	struct stat st1, st2;

	if (stat(in, &st1) == -1)
		ext2fs_fatal(errno, "stat %s\n", in);
	if (stat(out, &st2) == -1) {
		if (errno == ENOENT)
			return false;
		ext2fs_fatal(errno, "stat %s\n", out);
	}
	return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
}

int main(int argc, char *argv[])
{
	int opt;
	int out_fd;
	struct sparse_file *s;

	while ((opt = getopt(argc, argv, "czS")) != -1) {
		switch(opt) {
		case 'c':
			params.crc = true;
			break;
		case 'z':
			params.gzip = true;
			break;
		case 'S':
			params.sparse = false;
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (optind + 1 >= argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	params.in_file = strdup(argv[optind++]);
	params.out_file = strdup(argv[optind]);
	params.overwrite_input = same_file(params.in_file, params.out_file);

	s = ext_to_sparse(params.in_file);

	out_fd = open(params.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (out_fd == -1)
		ext2fs_fatal(errno, "opening %s\n", params.out_file);
	if (sparse_file_write(s, out_fd, params.gzip, params.sparse, params.crc) < 0)
		sparse_fatal("writing sparse file");

	sparse_file_destroy(s);

	free(params.in_file);
	free(params.out_file);
	free_chunks();
	close(out_fd);

	return 0;
}
