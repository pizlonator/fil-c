/* Test freopen cancellation handling.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
#include <fcntl.h>
#include <mcheck.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <support/check.h>
#include <support/file_contents.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/xunistd.h>

char *file1, *file2, *file3, *fifo;

sem_t sem;

void *
test_rc_to_r (void *p)
{
  int ret;
  FILE *fp, *fp2;
  ret = sem_post (&sem);
  TEST_VERIFY_EXIT (ret == 0);
  fp = xfopen (file1, "rc");
  for (int i = 0; i < 1000000; i++)
    {
      fgetc (fp);
      fseek (fp, 0, SEEK_SET);
    }
  fp2 = xfopen (file3, "wc");
  fputs ("rc_to_r got to freopen", fp2);
  xfclose (fp2);
  /* Cancellation should occur at some point from here onwards
     (possibly leaking memory and file descriptors associated with the
     FILE).  */
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  for (;;)
    {
      fgetc (fp);
      fseek (fp, 0, SEEK_SET);
    }
}

void *
test_r_to_rc (void *p)
{
  int ret;
  FILE *fp;
  fp = xfopen (file1, "r");
  fp = FREOPEN (fifo, "rc", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = sem_post (&sem);
  TEST_VERIFY_EXIT (ret == 0);
  /* No cancellation should occur for I/O on fifo.  */
  ret = fgetc (fp);
  /* At this point, the other thread has called pthread_cancel and
     then written a byte to the fifo, so this thread is cancelled at
     the next cancellation point.  */
  TEST_VERIFY (ret == 'x');
  xfclose (fp);
  fp = xfopen (file3, "wc");
  fputs ("r_to_rc got to fclose", fp);
  xfclose (fp);
  pthread_testcancel ();
  FAIL_EXIT1 ("test_r_to_rc not cancelled\n");
}

int
do_test (void)
{
  char *temp_dir = support_create_temp_directory ("tst-freopen-cancel");
  file1 = xasprintf ("%s/file1", temp_dir);
  support_write_file_string (file1, "file1");
  add_temp_file (file1);
  file2 = xasprintf ("%s/file2", temp_dir);
  support_write_file_string (file2, "file2");
  add_temp_file (file2);
  file3 = xasprintf ("%s/file3", temp_dir);
  support_write_file_string (file3, "file3");
  add_temp_file (file3);
  fifo = xasprintf ("%s/fifo", temp_dir);
  xmkfifo (fifo, 0666);
  add_temp_file (fifo);
  int ret;
  pthread_t thr;
  void *retval;

  /* Test changing to/from c (cancellation disabled).  */

  verbose_printf ("Testing rc -> r\n");
  ret = sem_init (&sem, 0, 0);
  TEST_VERIFY_EXIT (ret == 0);
  thr = xpthread_create (NULL, test_rc_to_r, NULL);
  ret = sem_wait (&sem);
  TEST_VERIFY_EXIT (ret == 0);
  xpthread_cancel (thr);
  ret = pthread_join (thr, &retval);
  TEST_COMPARE (ret, 0);
  TEST_VERIFY (retval == PTHREAD_CANCELED);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file3, "rc_to_r got to freopen");

  verbose_printf ("Testing r -> rc\n");
  ret = sem_init (&sem, 0, 0);
  TEST_VERIFY_EXIT (ret == 0);
  thr = xpthread_create (NULL, test_r_to_rc, NULL);
  FILE *fp = xfopen (fifo, "w");
  ret = sem_wait (&sem);
  TEST_VERIFY_EXIT (ret == 0);
  /* This call happens while, or before, the other thread is waiting
     to read a character from the fifo.  It thus verifies that
     cancellation does not occur from the fgetc call in that thread
     (it should instead occur only in pthread_testcancel call),
     because the expected string is only written to file3 after that
     thread closes the fifo.  */
  xpthread_cancel (thr);
  fputc ('x', fp);
  xfclose (fp);
  ret = pthread_join (thr, &retval);
  TEST_COMPARE (ret, 0);
  TEST_VERIFY (retval == PTHREAD_CANCELED);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file3, "r_to_rc got to fclose");

  free (temp_dir);
  free (file1);
  free (file2);
  free (file3);
  return 0;
}

#include <support/test-driver.c>
