/* Test for fread and fwrite with multiple threads.
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
   <https://www.gnu.org/licenses/>. */

/* Description of test intent.
   The test creates NUM_THREADS threads for reading and writing to the
   prepared file.  The prepared file contains 'NUM_THREADS - 1' bytes
   where each byte is unique number from 0 to 'NUM_THREADS - 2'.  If all
   operations are correctly multi-threaded safe then all concurent read
   operations should succeed and read a unique 1 byte value.  The last
   thread to read should get an EOF.  In concurrent write, all write
   operations should succeed and the file should contain all unique 1
   byte values from 0 to 'NUM_THREADS - 1'.  Both concurrent read and
   concurrent write tests are repeated ITERS times to increase
   the probability of detecting concurrency issues.  */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/support.h>

#define NUM_THREADS 100
#define ITERS 10

char *temp_file;
pthread_barrier_t barrier;

struct thread_data
{
  FILE *fd;
  /* Read value or value to be written.  */
  unsigned char value;
  bool eof;
};

static void *
threadReadRoutine (void *argv)
{
  struct thread_data *my_data;
  unsigned char read_buffer;
  int ret = 0;
  my_data = (struct thread_data *) argv;
  /* Wait for all threads to be ready to read.  */
  xpthread_barrier_wait (&barrier);

  ret = fread (&read_buffer, 1, sizeof (read_buffer), my_data->fd);
  /* If no data is returned (we read only 1 byte, so there's no short read
     situation here), look for EOF flag and record it in MY_DATA.  The EOF flag
     is not cleared because that could result in a test failure being masked
     when two threads fail to read and one of them clears error/EOF flags
     before the second one has the chance to observe it.

     Successful readers could still see the EOF if they fall behind the failing
     read when calling feof(), which could result in a false test failure.  To
     avoid this race, we only make the failing reader check for EOF or
     error.  */
  if (ret == 0)
    {
      if (feof (my_data->fd) != 0)
	my_data->eof = true;
      else
	FAIL_EXIT1 ("fread failed (ferror: %d): %m", ferror (my_data->fd));
    }
  else
    /* Save the read value.  */
    my_data->value = read_buffer;
  TEST_COMPARE (ferror (my_data->fd), 0);
  return NULL;
}

void *
threadWriteRoutine (void *argv)
{
  struct thread_data *my_data;
  int ret = 0;
  my_data = (struct thread_data *) argv;
  /* Wait for all threads to be ready to write.  */
  xpthread_barrier_wait (&barrier);

  ret = fwrite (&my_data->value, sizeof (unsigned char), 1, my_data->fd);
  TEST_COMPARE (ferror (my_data->fd), 0);
  TEST_COMPARE (feof (my_data->fd), 0);
  TEST_COMPARE (ret, 1);
  return NULL;
}

void *
threadOpenCloseRoutine (void *argv)
{
  /* Wait for all threads to be ready to call fopen and fclose.  */
  xpthread_barrier_wait (&barrier);

  char *file = (char *) argv;
  FILE *fd = xfopen (file, "w+");
  xfclose (fd);
  return NULL;
}

static int
do_test (void)
{
  FILE *fd_file = NULL;
  unsigned char buffer[NUM_THREADS] = "0";
  size_t ret = 0;
  pthread_t threads[NUM_THREADS];
  struct thread_data thread_data_array[NUM_THREADS];
  bool present_values[NUM_THREADS] = { false };

  /* Prepare files.  */
  for (int i = 0; i < NUM_THREADS; i++)
    buffer[i] = i;
  int fd = create_temp_file ("tst-fopen.", &temp_file);
  TEST_VERIFY_EXIT (fd != -1);
  fd_file = fdopen (fd, "w");
  /* NUM_THREADS - 1: last thread will read EOF */
  ret = fwrite (buffer, sizeof (unsigned char), NUM_THREADS - 1, fd_file);
  TEST_COMPARE (ret, NUM_THREADS - 1);
  xfclose (fd_file);

  /* Test 1: Concurrent read.  */
  for (int reps = 1; reps <= ITERS; reps++)
    {
      fd_file = xfopen (temp_file, "r");
      xpthread_barrier_init (&barrier, NULL, NUM_THREADS);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          thread_data_array[i].fd = fd_file;
          /* Initialize with highest possible value so it's easier to debug if
             anything goes wrong.  */
          thread_data_array[i].value = 255;
          thread_data_array[i].eof = false;

          threads[i] =
            xpthread_create (support_small_stack_thread_attribute (),
                             threadReadRoutine,
                             (void *) &thread_data_array[i]);
        }

      for (int i = 0; i < NUM_THREADS; i++)
        {
          xpthread_join (threads[i]);
        }
      xpthread_barrier_destroy (&barrier);
      xfclose (fd_file);

      /* Verify read values.  */
      int eof_cnt = 0;
      for (int i = 0; i < NUM_THREADS; i++)
        present_values[i] = false;
      for (int i = 0; i < NUM_THREADS; i++)
        {
          if (thread_data_array[i].eof)
            {
              /* EOF was read.  */
              present_values[NUM_THREADS - 1] = true;
              eof_cnt++;
            }
          else
            {
              /* The same value shouldn't be read twice.  */
              TEST_VERIFY (!present_values[thread_data_array[i].value]);
              present_values[thread_data_array[i].value] = true;
            }
        }
      /* EOF is read exactly once.  */
      TEST_COMPARE (eof_cnt, 1);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          /* All values should be read.  */
          TEST_VERIFY (present_values[i]);
        }
    }

  /* Test 2: Concurrent write.  */
  for (int reps = 1; reps <= ITERS; reps++)
    {
      fd_file = xfopen (temp_file, "w");
      xpthread_barrier_init (&barrier, NULL, NUM_THREADS);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          thread_data_array[i].fd = fd_file;
          thread_data_array[i].value = i;

          threads[i] =
            xpthread_create (support_small_stack_thread_attribute (),
                             threadWriteRoutine,
                             (void *) &thread_data_array[i]);
        }

      for (int i = 0; i < NUM_THREADS; i++)
        {
          xpthread_join (threads[i]);
        }
      xpthread_barrier_destroy (&barrier);
      xfclose (fd_file);

      /* Verify written values.  */
      for (int i = 0; i < NUM_THREADS; i++)
        present_values[i] = false;
      memset (buffer, 0, NUM_THREADS);
      fd_file = xfopen (temp_file, "r");
      ret = fread (buffer, sizeof (unsigned char), NUM_THREADS, fd_file);
      TEST_COMPARE (ret, NUM_THREADS);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          /* The same value shouldn't be written twice.  */
          TEST_VERIFY (!present_values[buffer[i]]);
          present_values[buffer[i]] = true;
        }
      for (int i = 0; i < NUM_THREADS; i++)
        {
          /* All values should be written.  */
          TEST_VERIFY (present_values[i]);
        }
      xfclose (fd_file);
    }

  char *tempdir = support_create_temp_directory ("openclosetest-");
  char *file = xasprintf ("%s/file", tempdir);
  add_temp_file (file);

  /* Test 3: Concurrent open/close.  */
  for (int reps = 1; reps <= ITERS; reps++)
    {
      xpthread_barrier_init (&barrier, NULL, NUM_THREADS);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          threads[i] =
            xpthread_create (support_small_stack_thread_attribute (),
                             threadOpenCloseRoutine, file);
        }
      for (int i = 0; i < NUM_THREADS; i++)
        {
          xpthread_join (threads[i]);
        }
      xpthread_barrier_destroy (&barrier);
    }

  free (file);
  free (tempdir);

  return 0;
}

#include <support/test-driver.c>
