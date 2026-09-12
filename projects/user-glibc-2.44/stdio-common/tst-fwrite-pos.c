/* Test if fwrite returns consistent values on partial writes.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <errno.h>
/* stdio.h provides BUFSIZ, which is the size of fwrite's internal buffer.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/temp_file.h>

/* Length of the buffer in bytes.  */
#define INBUF_SIZE (BUFSIZ)

/* Amount of bytes written to fwrite's internal cache that trigger a
   flush.  */
#define CACHE_THRESHOLD (BUFSIZ / 2)

#define ITERATIONS 1000

/* Maximum number of bytes written during a partial write.  */
#define PARTIAL_BYTES 4

#define EXPECT_EVENT(opcode, state, expected_state)             \
  {                                                             \
    if (state != expected_state)                                \
      {                                                         \
	char *s = support_fuse_opcode (opcode);                 \
	FAIL ("unexpected event %s at state %d", s, state);     \
	free (s);                                               \
      }                                                         \
  }

/* The goal of this test is to check that file position of a file stream is
   correctly updated when write () returns a partial write.
   The file system simulates pseudorandom partial writes while the test is
   running.
   Meanwhile the main thread calls fwrite () with a large object first and
   small objects later.  The usage of a large enough object ensures that
   fwrite's internal cache is full enough, without triggering a write to file.
   Subsequent calls to fwrite are guaranteed to trigger a write to file.  */

static void
fuse_thread (struct support_fuse *f, void *closure)
{
  struct fuse_in_header *inh;
  int state = 0;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      {
	char *opcode = support_fuse_opcode (inh->opcode);
	printf ("info: (T) event %s(%llu) len=%u nodeid=%llu\n",
		opcode, (unsigned long long int) inh->unique, inh->len,
		(unsigned long long int) inh->nodeid);
	free (opcode);
      }

      /* Handle mountpoint and basic directory operation for the root (1).  */
      if (support_fuse_handle_mountpoint (f)
	  || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
	continue;

      switch (inh->opcode)
	{
	case FUSE_LOOKUP:
	  EXPECT_EVENT (inh->nodeid, state, 0);
	  state++;
	  support_fuse_reply_error (f, ENOENT);
	  break;
	case FUSE_CREATE:
	  EXPECT_EVENT (inh->nodeid, state, 1);
	  state++;
	  struct fuse_entry_out *entry;
	  struct fuse_open_out *open;
	  support_fuse_prepare_create (f, 2, &entry, &open);
	  entry->attr.mode = S_IFREG | 0600;
	  support_fuse_reply_prepared (f);
	  break;
	case FUSE_GETXATTR:
	  /* We don't need to support extended attributes in this test.  */
	  support_fuse_reply_error (f, ENOSYS);
	  break;
	case FUSE_GETATTR:
	  /* Happens after open.  */
	  if (inh->nodeid == 2)
	    {
	      struct fuse_attr_out *out = support_fuse_prepare_attr (f);
	      out->attr.mode = S_IFREG | 0600;
	      out->attr.size = 0;
	      support_fuse_reply_prepared (f);
	    }
	  else
	    support_fuse_reply_error (f, ENOENT);
	  break;
	case FUSE_WRITE:
	  if (inh->nodeid == 2)
	    {
	      struct fuse_write_out out;
	      if (state > 1 && state < ITERATIONS + 2)
		{
		  /* The 2nd and subsequent calls to fwrite () trigger a
		     flush of fwrite's internal cache.  Simulate a partial
		     write of up to PARTIAL_BYTES bytes.  */
		  out.padding = 0;
		  out.size = 1 + rand () % PARTIAL_BYTES,
		  state++;
		  support_fuse_reply (f, &out, sizeof (out));
		}
	      else if (state >= ITERATIONS + 2)
		{
		  /* This request is expected to come from fflush ().  Copy
		     all the data successfully.  This may be executed more
		     than once.  */
		  struct fuse_write_in *p = support_fuse_cast (WRITE, inh);
		  out.padding = 0;
		  out.size = p->size,
		  state++;
		  support_fuse_reply (f, &out, sizeof (out));
		}
	      else
		support_fuse_reply_error (f, EIO);
	    }
	  else
	    support_fuse_reply_error (f, EIO);
	  break;
	case FUSE_FLUSH:
	case FUSE_RELEASE:
	  TEST_COMPARE (inh->nodeid, 2);
	  support_fuse_reply_empty (f);
	  break;
	default:
	  FAIL ("unexpected event %s", support_fuse_opcode (inh->opcode));
	  support_fuse_reply_error (f, EIO);
	}
    }
}

static int
do_test (void)
{
  char *in;
  int i;
  size_t written;

  _Static_assert (CACHE_THRESHOLD <= INBUF_SIZE,
		  "the input buffer must be larger than the cache threshold");
  /* Avoid filling up fwrite's cache.  */
  _Static_assert (CACHE_THRESHOLD - 1 + PARTIAL_BYTES * ITERATIONS <= BUFSIZ,
		  "fwrite's cache must fit all data written");

  support_fuse_init ();
  struct support_fuse *fs = support_fuse_mount (fuse_thread, NULL);

  /* Create and open a temporary file in the fuse mount point.  */
  char *fname = xasprintf ("%s/%sXXXXXX", support_fuse_mountpoint (fs),
                           "tst-fwrite-fuse");
  int fd = mkstemp (fname);
  TEST_VERIFY_EXIT (fd != -1);
  FILE *f = fdopen (fd, "w");
  TEST_VERIFY_EXIT (f != NULL);

  /* Allocate an input array that will be written to the temporary file.  */
  in = xmalloc (INBUF_SIZE);
  for (i = 0; i < INBUF_SIZE; i++)
    in[i] = i % 0xff;

  /* Ensure the file position indicator is at the beginning of the stream.  */
  TEST_COMPARE (ftell (f), 0);

  /* Try to fill as most data to the cache of the file stream as possible
     with a single large object.
     All data is expected to be written to the cache.
     No errors are expected from this.  */
  TEST_COMPARE (fwrite (in, CACHE_THRESHOLD - 1, 1, f), 1);
  TEST_COMPARE (ferror (f), 0);
  written = CACHE_THRESHOLD - 1;

  /* Ensure the file position indicator advanced correctly.  */
  TEST_COMPARE (ftell (f), written);

  for (i = 0; i < ITERATIONS; i++)
    {
      /* Write an extra object of size PARTIAL_BYTES that triggers a write to
	 disk.  Our FS will write at most PARTIAL_BYTES bytes to the file
	 instead of all the data.  By writting PARTIAL_BYTES, we guarantee
	 the amount of data in the cache will never decrease below
	 CACHE_THRESHOLD.
	 No errors are expected.  */
      TEST_COMPARE (fwrite (in, PARTIAL_BYTES, 1, f), 1);
      TEST_COMPARE (ferror (f), 0);
      written += PARTIAL_BYTES;

      /* Ensure the file position indicator advanced correctly.  */
      TEST_COMPARE (ftell (f), written);
    }

  /* Flush the rest of the data.  */
  TEST_COMPARE (fflush (f), 0);
  TEST_COMPARE (ferror (f), 0);

  /* Ensure the file position indicator was not modified.  */
  TEST_COMPARE (ftell (f), written);

  /* In case an unexpected error happened, clear it before exiting.  */
  if (ferror (f))
    clearerr (f);

  xfclose (f);
  free (fname);
  free (in);
  support_fuse_unmount (fs);
  return 0;
}

#include <support/test-driver.c>
