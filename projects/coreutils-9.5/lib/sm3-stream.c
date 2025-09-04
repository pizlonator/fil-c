/* sm3.c - Functions to compute SM3 message digest of files or memory blocks
   according to the specification GM/T 004-2012 Cryptographic Hash Algorithm
   SM3, published by State Encryption Management Bureau, China.

   SM3 cryptographic hash algorithm.
   <http://www.sca.gov.cn/sca/xwdt/2010-12/17/content_1002389.shtml>

   Copyright (C) 2017-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jia Zhang <qianyue.zj@alibaba-inc.com>, 2017,
   considerably copypasting from David Madore's sha256.c */

#include <config.h>

/* Specification.  */
#if HAVE_OPENSSL_SM3
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sm3.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

/* Compute SM3 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 32 bytes
   beginning at RESBLOCK.  */
int
sm3_stream (FILE *stream, void *resblock)
{
  struct sm3_ctx ctx;
  size_t sum;

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  /* Initialize the computation context.  */
  sm3_init_ctx (&ctx);

  /* Iterate over full file contents.  */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
         computation function processes the whole buffer so that with the
         next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */
      while (1)
        {
          n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);

          sum += n;

          if (sum == BLOCKSIZE)
            break;

          if (n == 0)
            {
              /* Check for the error flag IFF N == 0, so that we don't
                 exit the loop after a partial read due to e.g., EAGAIN
                 or EWOULDBLOCK.  */
              if (ferror (stream))
                {
                  free (buffer);
                  return 1;
                }
              goto process_partial_block;
            }

          /* We've read at least one byte, so ignore errors.  But always
             check for EOF, since feof may be true even though N > 0.
             Otherwise, we could end up calling fread after EOF.  */
          if (feof (stream))
            goto process_partial_block;
        }

      /* Process buffer with BLOCKSIZE bytes.  Note that
                        BLOCKSIZE % 64 == 0
       */
      sm3_process_block (buffer, BLOCKSIZE, &ctx);
    }

 process_partial_block:;

  /* Process any remaining bytes.  */
  if (sum > 0)
    sm3_process_bytes (buffer, sum, &ctx);

  /* Construct result in desired memory.  */
  sm3_finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

/*
 * Hey Emacs!
 * Local Variables:
 * coding: utf-8
 * End:
 */
