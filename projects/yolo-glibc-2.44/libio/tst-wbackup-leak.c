/* Test _IO_wdefault_finish frees wide backup buffer [BZ #33999].  */

#include <malloc.h>
#include <stdio.h>
#include <wchar.h>
#include <support/check.h>

static void
one_round (void)
{
  wchar_t *buf = NULL;
  size_t size = 0;

  FILE *fp = open_wmemstream (&buf, &size);
  TEST_VERIFY_EXIT (fp != NULL);
  fputwc (L'A', fp);
  fflush (fp);
  /* Push back without prior read. read_ptr == read_base, so
     _IO_wdefault_pbackfail skips the buggy narrow read_ptr access
     (BZ #33998) and goes straight to allocating a wide backup
     buffer at fp->_wide_data->_IO_save_base.

     Note: this testcase relies on the fact that open_wmemstream
     does not set _IO_NO_READS on the stream. If that implementation
     is changed, this test would need a different stream type to verify
     the leak.  */
  rewind (fp);
  ungetwc (L'Z', fp);
  fclose (fp);
  free (buf);
}

static int
do_test (void)
{
  /* Warm up to stabilize allocator state.  */
  one_round ();

  struct mallinfo2 before = mallinfo2 ();
  for (int i = 0; i < 1000; i++)
    one_round ();
  struct mallinfo2 after = mallinfo2 ();

  /* Each leak is 128 * sizeof(wchar_t) = 512 bytes.
   * 1000 iterations would leak ~512 KB. Allow 4 KB noise. */
  TEST_VERIFY (after.uordblks - before.uordblks < 4096);

  return 0;
}

#include <support/test-driver.c>
