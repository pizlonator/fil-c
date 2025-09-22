/* Decompression support for libdwfl: zlib (gzip) and/or bzlib (bzip2).
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwflP.h"
#include "system.h"

#ifdef LZMA
# define USE_INFLATE	1
# include <lzma.h>
# define unzip		__libdw_unlzma
# define DWFL_E_ZLIB	DWFL_E_LZMA
# define MAGIC		"\xFD" "7zXZ\0" /* XZ file format.  */
# define MAGIC2		"\x5d\0"	/* Raw LZMA format.  */
# define Z(what)	LZMA_##what
# define LZMA_ERRNO	LZMA_PROG_ERROR
# define z_stream	lzma_stream
# define inflateInit(z)	lzma_auto_decoder (z, 1 << 30, 0)
# define do_inflate(z)	lzma_code (z, LZMA_RUN)
# define inflateEnd(z)	lzma_end (z)
#elif defined ZSTD
# define USE_INFLATE	1
# include <zstd.h>
# define unzip		__libdw_unzstd
# define DWFL_E_ZLIB	DWFL_E_ZSTD
# define MAGIC		"\x28\xb5\x2f\xfd"
#elif defined BZLIB
# define USE_INFLATE	1
# include <bzlib.h>
# define unzip		__libdw_bunzip2
# define DWFL_E_ZLIB	DWFL_E_BZLIB
# define MAGIC		"BZh"
# define Z(what)	BZ_##what
# define BZ_ERRNO	BZ_IO_ERROR
# define z_stream	bz_stream
# define inflateInit(z)	BZ2_bzDecompressInit (z, 0, 0)
# define do_inflate(z)	BZ2_bzDecompress (z)
# define inflateEnd(z)	BZ2_bzDecompressEnd (z)
#else
# define USE_INFLATE	0
# define crc32		loser_crc32
# include <zlib.h>
# define unzip		__libdw_gunzip
# define MAGIC		"\037\213"
# define Z(what)	Z_##what
#endif

#define READ_SIZE		(1 << 20)

struct unzip_state {
#if !USE_INFLATE
  gzFile zf;
#endif
  size_t mapped_size;
  void **whole;
  void *buffer;
  size_t size;
  void *input_buffer;
  off_t input_pos;
};

static inline bool
bigger_buffer (struct unzip_state *state, size_t start)
{
  size_t more = state->size ? state->size * 2 : start;
  char *b = realloc (state->buffer, more);
  while (unlikely (b == NULL) && more >= state->size + 1024)
    b = realloc (state->buffer, more -= 1024);
  if (unlikely (b == NULL))
    return false;
  state->buffer = b;
  state->size = more;
  return true;
}

static inline void
smaller_buffer (struct unzip_state *state, size_t end)
{
  state->buffer =
      realloc (state->buffer, end) ?: end == 0 ? NULL : state->buffer;
  state->size = end;
}

static inline Dwfl_Error
fail (struct unzip_state *state, Dwfl_Error failure)
{
  if (state->input_pos == (off_t) state->mapped_size)
    *state->whole = state->input_buffer;
  else
    {
      free (state->input_buffer);
      *state->whole = NULL;
    }
  free (state->buffer);
  return failure;
}

#ifndef ZSTD
static inline Dwfl_Error
zlib_fail (struct unzip_state *state, int result)
{
  switch (result)
    {
    case Z (MEM_ERROR):
      return fail (state, DWFL_E_NOMEM);
    case Z (ERRNO):
      return fail (state, DWFL_E_ERRNO);
    default:
      return fail (state, DWFL_E_ZLIB);
    }
}
#endif

#if !USE_INFLATE
static Dwfl_Error
open_stream (int fd, off_t start_offset, struct unzip_state *state)
{
    int d = dup (fd);
    if (unlikely (d < 0))
      return DWFL_E_ERRNO;
    if (start_offset != 0)
      {
	off_t off = lseek (d, start_offset, SEEK_SET);
	if (off != start_offset)
	  {
	    close (d);
	    return DWFL_E_ERRNO;
	  }
      }
    state->zf = gzdopen (d, "r");
    if (unlikely (state->zf == NULL))
      {
	close (d);
	return DWFL_E_NOMEM;
      }

    /* From here on, zlib will close D.  */

    return DWFL_E_NOERROR;
}
#endif

/* If this is not a compressed image, return DWFL_E_BADELF.
   If we uncompressed it into *WHOLE, *WHOLE_SIZE, return DWFL_E_NOERROR.
   Otherwise return an error for bad compressed data or I/O failure.
   If we return an error after reading the first part of the file,
   leave that portion malloc'd in *WHOLE, *WHOLE_SIZE.  If *WHOLE
   is not null on entry, we'll use it in lieu of repeating a read.  */

Dwfl_Error internal_function
unzip (int fd, off_t start_offset,
       void *mapped, size_t _mapped_size,
       void **_whole, size_t *whole_size)
{
  struct unzip_state state =
    {
#if !USE_INFLATE
      .zf = NULL,
#endif
      .mapped_size = _mapped_size,
      .whole = _whole,
      .buffer = NULL,
      .size = 0,
      .input_buffer = NULL,
      .input_pos = 0
    };

  if (mapped == NULL)
    {
      if (*state.whole == NULL)
	{
	  state.input_buffer = malloc (READ_SIZE);
	  if (unlikely (state.input_buffer == NULL))
	    return DWFL_E_NOMEM;

	  ssize_t n = pread_retry (fd, state.input_buffer, READ_SIZE, start_offset);
	  if (unlikely (n < 0))
	    return fail (&state, DWFL_E_ERRNO);

	  state.input_pos = n;
	  mapped = state.input_buffer;
	  state.mapped_size = n;
	}
      else
	{
	  state.input_buffer = *state.whole;
	  state.input_pos = state.mapped_size = *whole_size;
	}
    }

#define NOMAGIC(magic) \
  (state.mapped_size <= sizeof magic || \
   memcmp (mapped, magic, sizeof magic - 1))

  /* First, look at the header.  */
  if (NOMAGIC (MAGIC)
#ifdef MAGIC2
      && NOMAGIC (MAGIC2)
#endif
      )
    /* Not a compressed file.  */
    return fail (&state, DWFL_E_BADELF);

#ifdef ZSTD
  /* special case for libzstd since it is slightly different from the
     API provided by bzlib and liblzma.  */

  void *next_in = mapped;
  size_t avail_in = state.mapped_size;
  void *next_out = NULL;
  size_t avail_out = 0;
  size_t total_out = 0;

  size_t result;
  ZSTD_DCtx *dctx = ZSTD_createDCtx();
  if (dctx == NULL)
    return fail (&state, DWFL_E_NOMEM);

  do
    {
      if (avail_in == 0 && state.input_buffer != NULL)
	{
	  ssize_t n = pread_retry (fd, state.input_buffer, READ_SIZE,
				   start_offset + state.input_pos);
	  if (unlikely (n < 0))
	    {
	      ZSTD_freeDCtx (dctx);
	      return fail (&state, DWFL_E_ERRNO);
	    }
	  next_in = state.input_buffer;
	  avail_in = n;
	  state.input_pos += n;
	}
      if (avail_out == 0)
	{
	  ptrdiff_t pos = (void *) next_out - state.buffer;
	  if (!bigger_buffer (&state, avail_in))
	    {
	      ZSTD_freeDCtx (dctx);
	      return fail (&state, DWFL_E_NOMEM);
	    }
	  next_out = state.buffer + pos;
	  avail_out = state.size - pos;
	}

      ZSTD_inBuffer input = { next_in, avail_in, 0 };
      ZSTD_outBuffer output = { next_out, avail_out, 0 };
      result = ZSTD_decompressStream (dctx, &output, &input);

      if (! ZSTD_isError (result))
	{
	  total_out += output.pos;
	  next_out += output.pos;
	  avail_out -= output.pos;
	  next_in += input.pos;
	  avail_in -= input.pos;
	}

      if (result == 0)
	break;
    }
  while (avail_in > 0 && ! ZSTD_isError (result));

  ZSTD_freeDCtx (dctx);

  if (ZSTD_isError (result))
    return fail (&state, DWFL_E_ZSTD);

  smaller_buffer (&state, total_out);

#elif USE_INFLATE

  /* This style actually only works with bzlib and liblzma.
     The stupid zlib interface has nothing to grok the
     gzip file headers except the slow gzFile interface.  */

  z_stream z = { .next_in = mapped, .avail_in = state.mapped_size };
  int result = inflateInit (&z);
  if (result != Z (OK))
    {
      inflateEnd (&z);
      return zlib_fail (&state, result);
    }

  do
    {
      if (z.avail_in == 0 && state.input_buffer != NULL)
	{
	  ssize_t n = pread_retry (fd, state.input_buffer, READ_SIZE,
				   start_offset + state.input_pos);
	  if (unlikely (n < 0))
	    {
	      inflateEnd (&z);
	      return zlib_fail (&state, Z (ERRNO));
	    }
	  z.next_in = state.input_buffer;
	  z.avail_in = n;
	  state.input_pos += n;
	}
      if (z.avail_out == 0)
	{
	  ptrdiff_t pos = (void *) z.next_out - state.buffer;
	  if (!bigger_buffer (&state, z.avail_in))
	    {
	      result = Z (MEM_ERROR);
	      break;
	    }
	  z.next_out = state.buffer + pos;
	  z.avail_out = state.size - pos;
	}
    }
  while ((result = do_inflate (&z)) == Z (OK));

#ifdef BZLIB
  uint64_t total_out = (((uint64_t) z.total_out_hi32 << 32)
			| z.total_out_lo32);
  smaller_buffer (&state, total_out);
#else
  smaller_buffer (&state, z.total_out);
#endif

  inflateEnd (&z);

  if (result != Z (STREAM_END))
    return zlib_fail (&state, result);

#else  /* gzip only.  */

  /* Let the decompression library read the file directly.  */

  Dwfl_Error result = open_stream (fd, start_offset, &state);

  if (result == DWFL_E_NOERROR && gzdirect (state.zf))
    {
      gzclose (state.zf);
      /* Not a compressed stream after all.  */
      return fail (&state, DWFL_E_BADELF);
    }

  if (result != DWFL_E_NOERROR)
    return fail (&state, result);

  ptrdiff_t pos = 0;
  while (1)
    {
      if (!bigger_buffer (&state, 1024))
	{
	  gzclose (state.zf);
	  return zlib_fail (&state, Z (MEM_ERROR));
	}
      int n = gzread (state.zf, state.buffer + pos, state.size - pos);
      if (n < 0)
	{
	  int code;
	  gzerror (state.zf, &code);
	  gzclose (state.zf);
	  return zlib_fail (&state, code);
	}
      if (n == 0)
	break;
      pos += n;
    }

  gzclose (state.zf);
  smaller_buffer (&state, pos);
#endif

  free (state.input_buffer);

  *state.whole = state.buffer;
  *whole_size = state.size;

  return DWFL_E_NOERROR;
}
