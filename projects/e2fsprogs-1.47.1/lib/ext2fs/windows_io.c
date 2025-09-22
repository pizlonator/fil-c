/*
 * windows_io.c --- This is the Windows implementation of the I/O manager.
 *
 * Implements a one-block write-through cache.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *	2002 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <windows.h>
#include <winioctl.h>
#include <io.h>

#if !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#define _XOPEN_SOURCE 600
#define _DARWIN_C_SOURCE
#define _FILE_OFFSET_BITS 64
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "config.h"
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#define PR_GET_DUMPABLE 3
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>

#undef ALIGN_DEBUG

//#define DEBUG
#ifdef DEBUG
#define TRACE(...) {\
		    char __log[256];\
		    snprintf(__log, sizeof(__log), __VA_ARGS__);\
		    __log[sizeof(__log)-1] = 0;\
		    OutputDebugString(__log);\
		}
#else
#define TRACE(...) do { } while (0);
#endif

#include "ext2_fs.h"
#include "ext2fs.h"
#include "ext2fsP.h"

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)
#define EXT2_CHECK_MAGIC_RETURN(struct, code, ret) \
	  if ((struct)->magic != (code)) return (ret)

#define EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL  0x10ed

struct windows_cache {
	char			*buf;
	unsigned long long	block;
	int			access_time;
	unsigned		dirty:1;
	unsigned		in_use:1;
};

#define CACHE_SIZE 8
#define WRITE_DIRECT_SIZE 4	/* Must be smaller than CACHE_SIZE */
#define READ_DIRECT_SIZE 4	/* Should be smaller than CACHE_SIZE */

struct windows_private_data {
	int	magic;
	char	name[MAX_PATH];
	HANDLE	handle;
	char	dos_device[MAX_PATH];
	char	cf_device[MAX_PATH];
	int	dev;
	int	flags;
	int	align;
	int	access_time;
	ext2_loff_t offset;
	struct windows_cache cache[CACHE_SIZE];
	void	*bounce;
	struct struct_io_stats io_stats;
};

#define IS_ALIGNED(n, align) ((((uintptr_t) n) & \
			       ((uintptr_t) ((align)-1))) == 0)

static int fake_dos_name_for_device(struct windows_private_data *data)
{
	if (strncmp(data->name, "\\\\", 2) == 0) {
		data->dos_device[0] = 0;
		strcpy(data->cf_device, data->name);
		return 0;
	}

	_snprintf(data->dos_device, MAX_PATH, "fakedevice%lu", GetCurrentProcessId());

	if (!DefineDosDevice(DDD_RAW_TARGET_PATH, data->dos_device, data->name))
		return 1;

	_snprintf(data->cf_device, MAX_PATH, "\\\\.\\%s", data->dos_device);
	TRACE("e2fsprogs::fake_dos_name_for_device::DefineDosDevice(\"%s\")", data->dos_device);

	return 0;
}

static void remove_fake_dos_name(struct windows_private_data *data)
{
	if (*data->dos_device) {
		TRACE("e2fsprogs::remove_fake_dos_name::DefineDosDevice(\"%s\")", data->dos_device);
		DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE | DDD_REMOVE_DEFINITION, data->dos_device, data->name);
	}
}

static errcode_t windows_get_stats(io_channel channel, io_stats *stats)
{
	errcode_t	retval = 0;

	struct windows_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

	if (stats)
		*stats = &data->io_stats;

	return retval;
}

static LARGE_INTEGER make_large_integer(LONGLONG value)
{
	LARGE_INTEGER	li;

	li.QuadPart = value;
	return li;
}

/*
 * Here are the raw I/O functions
 */
static errcode_t raw_read_blk(io_channel channel,
			      struct windows_private_data *data,
			      unsigned long long block,
			      int count, void *bufv)
{
	errcode_t	retval;
	ssize_t	size;
	ext2_loff_t	location;
	DWORD		actual = 0;
	unsigned char	*buf = bufv;
	ssize_t		really_read = 0;

	size = (count < 0) ? -count : count * channel->block_size;
	data->io_stats.bytes_read += size;
	location = ((ext2_loff_t) block * channel->block_size) + data->offset;

	if (data->flags & IO_FLAG_FORCE_BOUNCE) {
		if (!SetFilePointerEx(data->handle, make_large_integer(location), NULL, FILE_BEGIN)) {
			retval = GetLastError();
			goto error_out;
		}
		goto bounce_read;
	}

	if (!SetFilePointerEx(data->handle, make_large_integer(location), NULL, FILE_BEGIN)) {
		retval = GetLastError();
		goto error_out;
	}
	if ((channel->align == 0) || (IS_ALIGNED(buf, channel->align) && IS_ALIGNED(size, channel->align))) {
		if (!ReadFile(data->handle, buf, size, &actual, NULL)) {
			retval = GetLastError();
			goto error_out;
		}
		if (actual != size) {
	short_read:
		if (actual < 0) {
			retval = GetLastError();
			actual = 0;
		} else
			retval = EXT2_ET_SHORT_READ;
		goto error_out;
		}
	return 0;
	}

	/*
	 * The buffer or size which we're trying to read isn't aligned
	 * to the O_DIRECT rules, so we need to do this the hard way...
	 */
bounce_read:
	while (size > 0) {
		if (!ReadFile(data->handle, data->bounce, channel->block_size, &actual, NULL)) {
			retval = GetLastError();
			goto error_out;
		}
		if (actual != channel->block_size) {
			actual = really_read;
			buf -= really_read;
			size += really_read;
			goto short_read;
		}
		actual = size;
		if (size > channel->block_size)
			actual = channel->block_size;
		memcpy(buf, data->bounce, actual);
		really_read += actual;
		size -= actual;
		buf += actual;
	}
	return 0;

error_out:
	if (actual >= 0 && actual < size)
		memset((char *) buf+actual, 0, size-actual);
	if (channel->read_error)
		retval = (channel->read_error)(channel, block, count, buf,
					       size, actual, retval);
	return retval;
}

static errcode_t raw_write_blk(io_channel channel,
			       struct windows_private_data *data,
			       unsigned long long block,
			       int count, const void *bufv)
{
	ssize_t		size;
	ext2_loff_t	location;
	DWORD		actual = 0;
	errcode_t	retval;
	const unsigned char *buf = bufv;

	if (count == 1)
		size = channel->block_size;
	else {
		if (count < 0)
			size = -count;
		else
			size = count * channel->block_size;
	}
	data->io_stats.bytes_written += size;

	location = ((ext2_loff_t) block * channel->block_size) + data->offset;

	if (data->flags & IO_FLAG_FORCE_BOUNCE) {
		if (!SetFilePointerEx(data->handle, make_large_integer(location), NULL, FILE_BEGIN)) {
			retval = GetLastError();
			goto error_out;
		}
	goto bounce_write;
	}

		if (!SetFilePointerEx(data->handle, make_large_integer(location), NULL, FILE_BEGIN)) {
			retval = GetLastError();
		goto error_out;
	}

	SetLastError(0);

	if ((channel->align == 0) || (IS_ALIGNED(buf, channel->align) && IS_ALIGNED(size, channel->align))) {
		if (!WriteFile(data->handle, buf, size, &actual, NULL)) {
			retval = GetLastError();
			goto error_out;
		}

		if (actual != size) {
		short_write:
			retval = EXT2_ET_SHORT_WRITE;
			goto error_out;
		}
		return 0;
	}

	/*
	 * The buffer or size which we're trying to write isn't aligned
	 * to the O_DIRECT rules, so we need to do this the hard way...
	 */
bounce_write:
	while (size > 0) {
		if (size < channel->block_size) {
			if (!ReadFile(data->handle, data->bounce, channel->block_size, &actual, NULL)) {
				retval = GetLastError();
				goto error_out;
			}
			if (actual != channel->block_size) {
				if (actual < 0) {
					retval = GetLastError();
					goto error_out;
				}
				memset((char *) data->bounce + actual, 0,
				       channel->block_size - actual);
			}
		}
		actual = size;
		if (size > channel->block_size)
			actual = channel->block_size;
		memcpy(data->bounce, buf, actual);
		if (!SetFilePointerEx(data->handle, make_large_integer(location), NULL, FILE_BEGIN)) {
			retval = GetLastError();
			goto error_out;
		}
		if (!WriteFile(data->handle, data->bounce, channel->block_size, &actual, NULL)) {
			retval = GetLastError();
			goto error_out;
		}

		if (actual != channel->block_size)
			goto short_write;
		size -= actual;
		buf += actual;
		location += actual;
	}
	return 0;

error_out:
	if (channel->write_error)
		retval = (channel->write_error)(channel, block, count, buf,
						size, actual, retval);
	return retval;
}


/*
 * Here we implement the cache functions
 */

/* Allocate the cache buffers */
static errcode_t alloc_cache(io_channel channel,
			     struct windows_private_data *data)
{
	errcode_t		retval;
	struct windows_cache	*cache;
	int			i;

	data->access_time = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		cache->block = 0;
		cache->access_time = 0;
		cache->dirty = 0;
		cache->in_use = 0;
		if (cache->buf)
			ext2fs_free_mem(&cache->buf);
		retval = io_channel_alloc_buf(channel, 0, &cache->buf);
		if (retval)
			return retval;
	}
	if (channel->align || data->flags & IO_FLAG_FORCE_BOUNCE) {
		if (data->bounce)
			ext2fs_free_mem(&data->bounce);
		retval = io_channel_alloc_buf(channel, 0, &data->bounce);
	}
	return retval;
}

/* Free the cache buffers */
static void free_cache(struct windows_private_data *data)
{
	struct windows_cache    *cache;
	int			i;

	data->access_time = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		cache->block = 0;
		cache->access_time = 0;
		cache->dirty = 0;
		cache->in_use = 0;
		if (cache->buf)
			ext2fs_free_mem(&cache->buf);
	}
	if (data->bounce)
		ext2fs_free_mem(&data->bounce);
}

#ifndef NO_IO_CACHE
/*
 * Try to find a block in the cache.  If the block is not found, and
 * eldest is a non-zero pointer, then fill in eldest with the cache
 * entry to that should be reused.
 */
static struct windows_cache *find_cached_block(struct windows_private_data *data,
					    unsigned long long block,
					    struct windows_cache **eldest)
{
	struct windows_cache	*cache, *unused_cache, *oldest_cache;
	int			i;

	unused_cache = oldest_cache = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		if (!cache->in_use) {
			if (!unused_cache)
				unused_cache = cache;
			continue;
		}
		if (cache->block == block) {
			cache->access_time = ++data->access_time;
			return cache;
		}
		if (!oldest_cache ||
		   (cache->access_time < oldest_cache->access_time))
			oldest_cache = cache;
	}
	if (eldest)
		*eldest = (unused_cache) ? unused_cache : oldest_cache;
	return 0;
}

/*
 * Reuse a particular cache entry for another block.
 */
static void reuse_cache(io_channel channel, struct windows_private_data *data,
		 struct windows_cache *cache, unsigned long long block)
{
	if (cache->dirty && cache->in_use)
		raw_write_blk(channel, data, cache->block, 1, cache->buf);

	cache->in_use = 1;
	cache->dirty = 0;
	cache->block = block;
	cache->access_time = ++data->access_time;
}

/*
 * Flush all of the blocks in the cache
 */
static errcode_t flush_cached_blocks(io_channel channel,
				     struct windows_private_data *data,
				     int invalidate)
{
	struct windows_cache	*cache;
	errcode_t	retval, retval2;
	int			i;

	retval2 = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		if (!cache->in_use)
			continue;

		if (invalidate)
			cache->in_use = 0;

		if (!cache->dirty)
			continue;

		retval = raw_write_blk(channel, data,
				       cache->block, 1, cache->buf);
		if (retval)
			retval2 = retval;
		else
			cache->dirty = 0;
	}
	return retval2;
}
#endif /* NO_IO_CACHE */

static errcode_t windows_open_channel(struct windows_private_data *data,
				   int flags, io_channel *channel,
				   io_manager io_mgr)
{
	io_channel	io = NULL;
	errcode_t	retval;
	ext2fs_struct_stat st;

	retval = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (retval)
		goto cleanup;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;

	io->manager = io_mgr;
	retval = ext2fs_get_mem(strlen(data->name)+1, &io->name);
	if (retval)
		goto cleanup;

	strcpy(io->name, data->name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

#if defined(O_DIRECT)
	if (flags & IO_FLAG_DIRECT_IO)
		io->align = ext2fs_get_dio_alignment(data->dev);
#endif

	/*
	 * If the device is really a block device, then set the
	 * appropriate flag, otherwise we can set DISCARD_ZEROES flag
	 * because we are going to use punch hole instead of discard
	 * and if it succeed, subsequent read from sparse area returns
	 * zero.
	 */
	if (ext2fs_fstat(data->dev, &st) == 0) {
		if (ext2fsP_is_disk_device(st.st_mode))
			io->flags |= CHANNEL_FLAGS_BLOCK_DEVICE;
		else
			io->flags |= CHANNEL_FLAGS_DISCARD_ZEROES;
	}

	if ((retval = alloc_cache(io, data)))
		goto cleanup;

#ifdef BLKROGET
	if (flags & IO_FLAG_RW) {
		int error;
		int readonly = 0;

		/* Is the block device actually writable? */
		error = ioctl(data->dev, BLKROGET, &readonly);
		if (!error && readonly) {
			retval = EPERM;
			goto cleanup;
		}
	}
#endif

	*channel = io;
	return 0;

cleanup:
	if (data) {
		if (data->dev >= 0)
			close(data->dev);
		free_cache(data);
		ext2fs_free_mem(&data);
	}
	if (io) {
		if (io->name) {
			ext2fs_free_mem(&io->name);
		}
		ext2fs_free_mem(&io);
	}
	return retval;
}

static DWORD windows_open_device(struct windows_private_data *data, int open_flags)
{
	DWORD ret = 0;

	if (*data->name != '\\')
		strcpy(data->cf_device, data->name);
	else if (fake_dos_name_for_device(data))
		return -1;

	DWORD desired_access = GENERIC_READ | ((open_flags & O_RDWR) ? GENERIC_WRITE : 0);
	DWORD share_mode = (open_flags & O_EXCL) ? 0 : FILE_SHARE_READ | ((open_flags & O_RDWR) ? FILE_SHARE_WRITE : 0);
	DWORD flags_and_attributes =
#if defined(O_DIRECT)
	(open_flags & O_DIRECT) ? (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH) : FILE_ATTRIBUTE_NORMAL;
#else
	FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
#endif

	data->handle = CreateFile(data->cf_device, desired_access, share_mode, NULL, OPEN_EXISTING,
				  flags_and_attributes, NULL);

	if (data->handle == INVALID_HANDLE_VALUE) {
		ret = GetLastError();
		goto invalid_handle;
	}

	TRACE("e2fsprogs::windows_open_device::CreateFile(\"%s\") = %p", data->cf_device, data->handle);

	data->dev = _open_osfhandle((intptr_t)data->handle, 0);
	if (data->dev < 0) {
		ret = GetLastError() ? GetLastError() : 9999;
		goto osfhandle_error;
	}

	TRACE("e2fsprogs::windows_open_device::_open_osfhandle(%p) = %d", data->handle, data->dev);

	return 0;

osfhandle_error:
	TRACE("e2fsprogs::windows_open_device::CloseHandle(%p)", data->handle);
	CloseHandle(data->handle);
invalid_handle:
	remove_fake_dos_name(data);
	TRACE("e2fsprogs::windows_open_device() = %lu, errno = %d", ret, errno);
	return ret;
}

static struct windows_private_data *init_private_data(const char *name, int flags)
{
	struct windows_private_data *data = NULL;

	if (ext2fs_get_mem(sizeof(struct windows_private_data), &data))
		return NULL;

	memset(data, 0, sizeof(struct windows_private_data));
	strncpy(data->name, name, sizeof(data->name) - 1);
	data->magic = EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL;
	data->io_stats.num_fields = 2;
	data->flags = flags;
	data->handle = INVALID_HANDLE_VALUE;

	return data;
}

static errcode_t windows_open(const char *name, int flags, io_channel *channel)
{
	int open_flags;
	struct windows_private_data *data;

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;

	data = init_private_data(name, flags);
	if (!data)
		return EXT2_ET_NO_MEMORY;

	open_flags = (flags & IO_FLAG_RW) ? O_RDWR : O_RDONLY;
	if (flags & IO_FLAG_EXCLUSIVE)
		open_flags |= O_EXCL;
#if defined(O_DIRECT)
	if (flags & IO_FLAG_DIRECT_IO)
		open_flags |= O_DIRECT;
#endif

	if (windows_open_device(data, open_flags)) {
		ext2fs_free_mem(&data);
		return EXT2_ET_BAD_DEVICE_NAME;
	}

	return windows_open_channel(data, flags, channel, windows_io_manager);
}

static errcode_t windows_close(io_channel channel)
{
	struct windows_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;

#ifndef NO_IO_CACHE
	retval = flush_cached_blocks(channel, data, 0);
#endif

	remove_fake_dos_name(data);

	if (_close(data->dev) != 0)
		retval = errno;

	TRACE("e2fsprogs::windows_close::_close(%d)", data->dev);

	free_cache(data);

	ext2fs_free_mem(&channel->private_data);
	if (channel->name)
		ext2fs_free_mem(&channel->name);
	ext2fs_free_mem(&channel);
	return retval;
}

static errcode_t windows_set_blksize(io_channel channel, int blksize)
{
	struct windows_private_data *data;
	errcode_t		retval;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

	if (channel->block_size != blksize) {
#ifndef NO_IO_CACHE
		if ((retval = flush_cached_blocks(channel, data, 0)))
			return retval;
#endif

		channel->block_size = blksize;
		free_cache(data);
		if ((retval = alloc_cache(channel, data)))
			return retval;
	}
	return 0;
}

static errcode_t windows_read_blk64(io_channel channel, unsigned long long block, int count, void *buf)
{
	struct windows_private_data *data;
	struct windows_cache *cache, *reuse[READ_DIRECT_SIZE];
	errcode_t	retval;
	char		*cp;
	int		i, j;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

#ifdef NO_IO_CACHE
	return raw_read_blk(channel, data, block, count, buf);
#else
	/*
	 * If we're doing an odd-sized read or a very large read,
	 * flush out the cache and then do a direct read.
	 */
	if (count < 0 || count > WRITE_DIRECT_SIZE) {
		if ((retval = flush_cached_blocks(channel, data, 0)))
			return retval;
		return raw_read_blk(channel, data, block, count, buf);
	}

	cp = buf;
	while (count > 0) {
		/* If it's in the cache, use it! */
		if ((cache = find_cached_block(data, block, &reuse[0]))) {
#ifdef DEBUG
			printf("Using cached block %lu\n", block);
#endif
			memcpy(cp, cache->buf, channel->block_size);
			count--;
			block++;
			cp += channel->block_size;
			continue;
		}
		if (count == 1) {
			/*
			 * Special case where we read directly into the
			 * cache buffer; important in the O_DIRECT case
			 */
			cache = reuse[0];
			reuse_cache(channel, data, cache, block);
			if ((retval = raw_read_blk(channel, data, block, 1,
						   cache->buf))) {
				cache->in_use = 0;
				return retval;
			}
			memcpy(cp, cache->buf, channel->block_size);
			return 0;
		}

		/*
		 * Find the number of uncached blocks so we can do a
		 * single read request
		 */
		for (i=1; i < count; i++)
			if (find_cached_block(data, block+i, &reuse[i]))
				break;
#ifdef DEBUG
		printf("Reading %d blocks starting at %lu\n", i, block);
#endif
		if ((retval = raw_read_blk(channel, data, block, i, cp)))
			return retval;

		/* Save the results in the cache */
		for (j=0; j < i; j++) {
			count--;
			cache = reuse[j];
			reuse_cache(channel, data, cache, block++);
			memcpy(cache->buf, cp, channel->block_size);
			cp += channel->block_size;
		}
	}
	return 0;
#endif /* NO_IO_CACHE */
}

static errcode_t windows_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	return windows_read_blk64(channel, block, count, buf);
}

static errcode_t windows_write_blk64(io_channel channel,
				unsigned long long block,
				int count, const void *buf)
{
	struct windows_private_data *data;
	struct windows_cache *cache, *reuse;
	errcode_t	retval = 0;
	const char	*cp;
	int		writethrough;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

#ifdef NO_IO_CACHE
	return raw_write_blk(channel, data, block, count, buf);
#else
	/*
	 * If we're doing an odd-sized write or a very large write,
	 * flush out the cache completely and then do a direct write.
	 */
	if (count < 0 || count > WRITE_DIRECT_SIZE) {
		if ((retval = flush_cached_blocks(channel, data, 1)))
			return retval;
		return raw_write_blk(channel, data, block, count, buf);
	}

	/*
	 * For a moderate-sized multi-block write, first force a write
	 * if we're in write-through cache mode, and then fill the
	 * cache with the blocks.
	 */
	writethrough = channel->flags & CHANNEL_FLAGS_WRITETHROUGH;
	if (writethrough)
		retval = raw_write_blk(channel, data, block, count, buf);

	cp = buf;
	while (count > 0) {
		cache = find_cached_block(data, block, &reuse);
		if (!cache) {
			cache = reuse;
			reuse_cache(channel, data, cache, block);
		}
		if (cache->buf != cp)
			memcpy(cache->buf, cp, channel->block_size);
		cache->dirty = !writethrough;
		count--;
		block++;
		cp += channel->block_size;
	}
	return retval;
#endif /* NO_IO_CACHE */
}

static errcode_t windows_cache_readahead(io_channel channel,
				      unsigned long long block,
				      unsigned long long count)
{
	return EXT2_ET_OP_NOT_SUPPORTED;
}

static errcode_t windows_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	return windows_write_blk64(channel, block, count, buf);
}

static errcode_t windows_write_byte(io_channel channel, unsigned long offset,
				 int size, const void *buf)
{
	return EXT2_ET_UNIMPLEMENTED;
}

/*
 * Flush data buffers to disk.
 */
static errcode_t windows_flush(io_channel channel)
{
	struct windows_private_data *data;
	errcode_t retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

#ifndef NO_IO_CACHE
	retval = flush_cached_blocks(channel, data, 0);
#endif

	return retval;
}

static errcode_t windows_set_option(io_channel channel, const char *option,
				 const char *arg)
{
	struct windows_private_data *data;
	unsigned long long tmp;
	char *end;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

	if (!strcmp(option, "offset")) {
		if (!arg)
			return EXT2_ET_INVALID_ARGUMENT;

		tmp = strtoull(arg, &end, 0);
		if (*end)
			return EXT2_ET_INVALID_ARGUMENT;
		data->offset = tmp;
		if (data->offset < 0)
			return EXT2_ET_INVALID_ARGUMENT;
		return 0;
	}
	return EXT2_ET_INVALID_ARGUMENT;
}

static errcode_t windows_discard(io_channel channel, unsigned long long block,
				 unsigned long long count)
{
	TRACE("e2fsprogs::windows_discard::EXT2_ET_UNIMPLEMENTED");
	return EXT2_ET_UNIMPLEMENTED;
}

/* parameters might not be used if OS doesn't support zeroout */
#if __GNUC_PREREQ (4, 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
static errcode_t windows_zeroout(io_channel channel, unsigned long long block,
			      unsigned long long count)
{
	struct windows_private_data *data;
	int		ret;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct windows_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_WINDOWS_IO_CHANNEL);

	if (channel->flags & CHANNEL_FLAGS_BLOCK_DEVICE) {
		/* Not implemented until the BLKZEROOUT mess is fixed */
		goto unimplemented;
	} else {
		/* Regular file, try to use truncate/punch/zero. */
		struct stat statbuf;

		if (count == 0)
			return 0;
		/*
		 * If we're trying to zero a range past the end of the file,
		 * extend the file size, then truncate everything.
		 */
		ret = fstat(data->dev, &statbuf);
		if (ret)
			goto err;
		if ((unsigned long long) statbuf.st_size <
			(block + count) * channel->block_size + data->offset) {
			ret = ftruncate(data->dev,
					(block + count) * channel->block_size + data->offset);
			if (ret)
				goto err;
		}
		goto unimplemented;
	}
err:
	if (ret < 0) {
		if (errno == EOPNOTSUPP)
			goto unimplemented;
		return errno;
	}
	return 0;
unimplemented:
	return EXT2_ET_UNIMPLEMENTED;
}

int ext2fs_open_file(const char *pathname, int flags, mode_t mode)
{
	flags |= O_BINARY;

	if (mode)
#if defined(HAVE_OPEN64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
		return open64(pathname, flags, mode);
	else
		return open64(pathname, flags);
#else
		return open(pathname, flags, mode);
	else
		return open(pathname, flags);
#endif
}

int ext2fs_stat(const char *path, ext2fs_struct_stat *buf)
{
#if defined(HAVE_FSTAT64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
	return stat64(path, buf);
#else
	return stat(path, buf);
#endif
}

int ext2fs_fstat(int fd, ext2fs_struct_stat *buf)
{
#if defined(HAVE_FSTAT64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
	return fstat64(fd, buf);
#else
	return fstat(fd, buf);
#endif
}

#if __GNUC_PREREQ (4, 6)
#pragma GCC diagnostic pop
#endif

static struct struct_io_manager struct_windows_manager = {
	.magic		= EXT2_ET_MAGIC_IO_MANAGER,
	.name		= "Windows I/O Manager",
	.open		= windows_open,
	.close		= windows_close,
	.set_blksize	= windows_set_blksize,
	.read_blk	= windows_read_blk,
	.write_blk	= windows_write_blk,
	.flush		= windows_flush,
	.write_byte	= windows_write_byte,
	.set_option	= windows_set_option,
	.get_stats	= windows_get_stats,
	.read_blk64	= windows_read_blk64,
	.write_blk64	= windows_write_blk64,
	.discard	= windows_discard,
	.cache_readahead	= windows_cache_readahead,
	.zeroout	= windows_zeroout,
};

io_manager windows_io_manager = &struct_windows_manager;
