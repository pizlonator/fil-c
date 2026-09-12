/* Test setvbuf under various conditions.
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

/* This file is used twice, once as the test itself (where do_test
   is defined) and once as a subprocess we spawn to test stdin et all
   (where main is defined).  INDEPENDENT_PART is defined for the
   latter.

   Note also that the purpose of this test is to test setvbuf, not the
   underlying buffering code.  */

#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <libio.h>
#include <termios.h>

#include <support/support.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>
#include <support/xthread.h>
#include <support/tty.h>

/* Dear future developer: If you are reading this, you are likely
   trying to change or understand this test.  In that case, these
   debug/dump macros will be helpful.  */
#if 0
# define debug printf ("\033[3%dm%s:%d\033[0m\n", \
		       (__LINE__ % 6) + 1, __FUNCTION__, __LINE__);

static void
dumpfp (FILE *fp)
{
  char f[10], *p=f;

  if (fp->_flags & _IO_UNBUFFERED)
    *p++ = 'N';
  if (fp->_flags & _IO_LINE_BUF)
    *p++ = 'L';
  if (p == f)
    *p++ = 'B';
  *p = 0;

  printf ("FILE %p flags %s"
	  " read %p \033[%dm%+ld \033[%dm%+ld\033[0m"
	  " write %p \033[%dm%+ld \033[%dm%+ld\033[0m %ld"
	  " buf %p \033[%dm%+ld\033[0m  sz %ld pend %ld\n",
	  fp, f,

	  fp->_IO_read_base,
	  fp->_IO_read_ptr == fp->_IO_read_base ? 33 : 32,
	  fp->_IO_read_ptr - fp->_IO_read_base,
	  fp->_IO_read_end == fp->_IO_read_base ? 33 : 36,
	  fp->_IO_read_end - fp->_IO_read_base,

	  fp->_IO_write_base,
	  fp->_IO_write_ptr == fp->_IO_write_base ? 33 : 32,
	  fp->_IO_write_ptr - fp->_IO_write_base,
	  fp->_IO_write_end == fp->_IO_write_base ? 33 : 36,
	  fp->_IO_write_end - fp->_IO_write_base,
	  fp->_IO_write_end - fp->_IO_write_base,

	  fp->_IO_buf_base,
	  fp->_IO_buf_end == fp->_IO_buf_base ? 33 : 35,
	  fp->_IO_buf_end - fp->_IO_buf_base,
	  __fbufsize (fp), __fpending (fp)
	  );
}
#else
# define debug
# define dumpfp(FP)
#endif

#ifndef INDEPENDENT_PART
/* st_blksize value for that file, or BUFSIZ if out of range.  */
static int blksize = BUFSIZ;
#endif

/* Our test buffer.  */
#define TEST_BUFSIZE 42
static int bufsize = TEST_BUFSIZE < BUFSIZ ? TEST_BUFSIZE : BUFSIZ;
static char *buffer;

/* Test data, both written to that file and used as an in-memory
   stream.  */
char test_data[2 * BUFSIZ];

#define TEST_STRING "abcdef\n"

enum test_source_case
  {
    test_source_file,
    test_source_pipe,
    test_source_fifo,
    test_source_pseudo_terminal,
    test_source_dev_null,
    test_source_count,
  };

static const char *const test_source_name[test_source_count] =
  {
    "regular file",
    "pipe",
    "fifo",
    "pseudo_terminal",
    "dev_null"
  };

enum test_stream_case
  {
    test_stream_stdin,
    test_stream_stdout,
    test_stream_stderr,
    test_stream_fopen_r,
    test_stream_fdopen_r,
    test_stream_fopen_w,
    test_stream_fdopen_w,
    test_stream_count
  };

static bool test_stream_reads[test_stream_count] =
  {
    true,
    false,
    false,
    true,
    true,
    false,
    false
  };

static const char *const test_stream_name[test_stream_count] =
  {
    "stdin",
    "stdout",
    "stderr",
    "fopen (read)",
    "fdopen (read)",
    "fopen (write)",
    "fdopen (write)"
  };

enum test_config_case
  {
    test_config_none,
    test_config_unbuffered,
    test_config_line,
    test_config_fully,
    test_config_count
  };

static const char *const test_config_name[test_config_count] =
  {
    "no change",
    "unbuffered",
    "line buffered",
    "fully buffered"
  };

FILE *test_stream;

char *test_file_name = NULL;
int pty_fd;
char *test_pipe_name = NULL;
int test_pipe[2];

/* This is either -1 or represents a pre-opened file descriptor for
   the test as returned by prepare_test_file.  */
int test_fd;

/*------------------------------------------------------------*/

/* Note that throughout this test we reopen, remove, and change
   to/from a fifo, the test file.  This would normally cause a race
   condition, except that we're in a test container.  No other process
   can run in the test container simultaneously.  */

void
prepare_test_data (void)
{
  buffer = (char *) xmalloc (bufsize);

#ifndef INDEPENDENT_PART
  /* Both file and pipe need this.  */
  if (test_file_name == NULL)
    {
      debug;
      int fd = create_temp_file ("tst-setvbuf2", &test_file_name);
      TEST_VERIFY_EXIT (fd != -1);
      struct stat64 st;
      xfstat64 (fd, &st);
      if (st.st_blksize > 0 && st.st_blksize < BUFSIZ)
	blksize = st.st_blksize;
      xclose (fd);
    }
#endif

  for (size_t i = 0; i < 2 * BUFSIZ; i++)
    {
      unsigned char c = TEST_STRING[i % strlen (TEST_STRING)];
      test_data[i] = c;
    }
}

#ifndef INDEPENDENT_PART

/* These functions provide a source/sink for the "other" side of any
   pipe-style descriptor we're using for test.  */

static pthread_t writer_thread_tid = 0;
static pthread_t reader_thread_tid = 0;

typedef struct {
  int fd;
  const char *fname;
} ThreadData;
/* It's OK if this is static, we only run one at a time.  */
ThreadData thread_data;

static void
end_thread (pthread_t *ptid)
{
  if (*ptid)
    {
      pthread_cancel (*ptid);
      xpthread_join (*ptid);
      /* The descriptor was passed in, or the helper thread made
	 sufficient progress and opened the file.  */
      if (thread_data.fd >= 0)
	xclose (thread_data.fd);
      *ptid = 0;
    }
}

static void *
writer_thread_proc (void *closure)
{
  ThreadData *td = (ThreadData *) closure;
  int fd;
  int i;
  ssize_t wn;
  debug;

  if (td->fname)
    td->fd = xopen (td->fname, O_WRONLY, 0666);
  fd = td->fd;

  while (1)
    {
      i = 0;
      while (i < BUFSIZ)
	{
	  wn = write (fd, test_data + i, BUFSIZ - i);
	  if (wn <= 0)
	    break;
	  i += wn;
	}
    }
  return NULL;
}

static void *
reader_thread_proc (void *closure)
{
  ThreadData *td = (ThreadData *) closure;
  int fd;
  ssize_t rn;
  int n = 0;
  debug;

  if (td->fname)
    td->fd = xopen (td->fname, O_RDONLY, 0666);
  fd = td->fd;

  while (1)
    {
      char buf[BUFSIZ];
      rn = read (fd, buf, BUFSIZ);
      if (rn <= 0)
	break;
      TEST_COMPARE_BLOB (buf, rn, test_data+n, rn);
      n += rn;
    }
  return NULL;
}

static void
start_writer_thread (int fd)
{
  debug;
  thread_data.fd = fd;
  thread_data.fname = NULL;
  writer_thread_tid = xpthread_create (NULL, writer_thread_proc,
				       (void *)&thread_data);
}

static void
start_writer_thread_n (const char *fname)
{
  debug;
  thread_data.fd = -1;
  thread_data.fname = fname;
  writer_thread_tid = xpthread_create (NULL, writer_thread_proc,
				       (void *)&thread_data);
}

static void
end_writer_thread (void)
{
  debug;
  end_thread (&writer_thread_tid);
}

static void
start_reader_thread (int fd)
{
  debug;
  thread_data.fd = fd;
  thread_data.fname = NULL;
  reader_thread_tid = xpthread_create (NULL, reader_thread_proc,
				       (void *)&thread_data);
}

static void
start_reader_thread_n (const char *fname)
{
  debug;
  thread_data.fd = -1;
  thread_data.fname = fname;
  reader_thread_tid = xpthread_create (NULL, reader_thread_proc,
				       (void *)&thread_data);
}

static void
end_reader_thread (void)
{
  debug;
  end_thread (&reader_thread_tid);
}

/*------------------------------------------------------------*/

/* These two functions are reponsible for choosing a file to be tested
   against, typically by returning a filename but in a few cases also
   providing a file descriptor (i.e. for fdopen).  */

static const char *
prepare_test_file (enum test_source_case f, enum test_stream_case s)
{
  debug;

  test_fd = -1;

  switch (f)
    {
    case test_source_file:
      {
	if (test_stream_reads[f])
	  {
	    debug;
	    FILE *fp = xfopen (test_file_name, "w");
	    TEST_VERIFY_EXIT (fwrite (test_data, 1, 2 * BUFSIZ, fp)
			      == 2 * BUFSIZ);
	    xfclose (fp);
	  }
	debug;
	return test_file_name;
      }

    case test_source_pipe:
      {
	debug;
	xpipe (test_pipe);
	if (test_stream_reads[s])
	  {
	    start_writer_thread (test_pipe[1]);
	    test_fd = test_pipe[0];
	  }
	else
	  {
	    start_reader_thread (test_pipe[0]);
	    test_fd = test_pipe[1];
	  }
	test_pipe_name = xasprintf ("/proc/self/fd/%d", test_fd);
	debug;
	return test_pipe_name;
      }

    case test_source_fifo:
      {
	/* We do not want to fail/exit if the file doesn't exist.  */
	unlink (test_file_name);
	xmkfifo (test_file_name, 0600);
	debug;
	if (test_stream_reads[s])
	  start_writer_thread_n (test_file_name);
	else
	  start_reader_thread_n (test_file_name);
	debug;
	return test_file_name;
      }

    case test_source_pseudo_terminal:
      {
	support_openpty (&pty_fd, &test_fd, &test_pipe_name, NULL, NULL);

	debug;
	if (test_stream_reads[s])
	  start_writer_thread (pty_fd);
	else
	  start_reader_thread (pty_fd);

	debug;
	return test_pipe_name;
      }

    case test_source_dev_null:
	debug;
      return "/dev/null";

    default:
      abort ();
    }
}

static void
unprepare_test_file (FILE *fp,
		     enum test_source_case f,
		     enum test_stream_case s)
{
  debug;
  switch (f)
    {
    case test_source_file:
      break;

    case test_source_pipe:
      free (test_pipe_name);
      if (test_stream_reads[s])
	end_writer_thread ();
      else
	end_reader_thread ();
      break;

    case test_source_fifo:
      if (test_stream_reads[s])
	end_writer_thread ();
      else
	end_reader_thread ();
      unlink (test_file_name);
      break;

    case test_source_pseudo_terminal:
      free (test_pipe_name);
      if (test_stream_reads[s])
	end_writer_thread ();
      else
	end_reader_thread ();
      break;

    case test_source_dev_null:
      break;

    default:
      abort ();
    }
  debug;
}

/*------------------------------------------------------------*/

/* This function takes a filename and returns a file descriptor,
   opened according to the method requested.  */

static FILE *
open_test_stream (enum test_source_case f, enum test_stream_case s)
{
  int fd;
  FILE *fp;
  const char *fname;

  debug;
  fname = prepare_test_file (f, s);
  if (fname == NULL)
    return NULL;

  switch (s)
    {
    case test_stream_stdin:
      fp = xfopen (fname, "r");
      break;

    case test_stream_stdout:
      fp = xfopen (fname, "w");
      break;

    case test_stream_stderr:
      fp = xfopen (fname, "w");
      break;

    case test_stream_fopen_r:
      fp = xfopen (fname, "r");
      break;

    case test_stream_fdopen_r:
      if (test_fd == -1)
	fd = xopen (fname, O_RDONLY, 0);
      else
	fd = test_fd;
      fp = fdopen (fd, "r");
      break;

    case test_stream_fopen_w:
      fp = xfopen (fname, "w");
      break;

    case test_stream_fdopen_w:
      fd = xopen (fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
      fp = fdopen (fd, "w");
      break;

    default:
      abort ();
    }
  TEST_VERIFY_EXIT (fp != NULL);

  if (f == test_source_pseudo_terminal)
    {
      struct termios t;
      /* We disable the NL to CR-LF conversion so that we can compare
	 data without having to remove the extra CRs.  */
      if (tcgetattr (fileno (fp), &t) < 0)
	FAIL_EXIT1 ("tcgetattr failed: %m");
      t.c_oflag &= ~ONLCR;
      if (tcsetattr (fileno (fp), TCSANOW, &t) < 0)
	FAIL_EXIT1 ("tcsetattr failed: %m");
    }

  debug;
  printf ("source %s stream %s file %s fd %d\n",
	  test_source_name[f],
	  test_stream_name[s], fname, fileno (fp));
  return fp;
}

#endif

/*------------------------------------------------------------*/

/* These functions do the actual testing - setting various buffering
   options and verifying that they buffer as expected.  */

static void
test_put_string (FILE *fp, const char *s, int count)
{
  while (*s && count--)
    {
      fputc (*s++, fp);
      TEST_VERIFY_EXIT (!ferror (fp));
    }
}

int
verify_fully_buffered (FILE *fp,
		       enum test_source_case f,
		       enum test_stream_case s,
		       enum test_config_case c)
{
  debug;
  if (test_stream_reads[s])
    {
      char buf[10];
      dumpfp (fp);
      size_t fc = fread (buf, 1, 10 - 1, fp);
      dumpfp (fp);

      ssize_t count = fp->_IO_read_ptr - fp->_IO_read_base;

      TEST_VERIFY (fp->_IO_read_base != NULL);
      if (f == test_source_dev_null)
	{
	  TEST_VERIFY (fc == 0);
	  TEST_VERIFY (count == 0);
	}
      else if (f == test_source_pseudo_terminal)
	{
	  TEST_VERIFY (fc == 9);
	  TEST_VERIFY (count == 3 || count == 10);
	}
      else
	{
	  TEST_VERIFY (fc == 9);
	  TEST_VERIFY (count == 10);
	}

      /* We already checked for the first character being 'a'.  */
      if (count > 1)
	{
	  TEST_COMPARE_BLOB (buf, count - 1, test_data + 1, count - 1);
	  TEST_COMPARE_BLOB (fp->_IO_read_base, count, test_data, count);
	}
    }
  else
    {
      dumpfp (fp);
      test_put_string (fp, test_data + 1, 10 - 1);
      dumpfp (fp);
      TEST_COMPARE (fp->_IO_write_ptr - fp->_IO_write_base, 10);
      TEST_COMPARE_BLOB (fp->_IO_write_base, 10, test_data, 10);
    }

  TEST_COMPARE ((fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF)), 0);
  if (c != test_config_none)
    TEST_COMPARE (__fbufsize (fp), bufsize);
  return 0;
}

int
verify_line_buffered (FILE *fp,
		      enum test_source_case f,
		      enum test_stream_case s,
		      enum test_config_case c)
{
  debug;
  /* "line buffered" for inputs is not really defined; what you really
     want here is to control the device providing input.  For GLIBC a
     line-buffered input is treated as fully buffered.  */
  if (test_stream_reads[s])
    {
      char buf[10];
      dumpfp (fp);
      size_t fc = fread (buf, 1, 10 - 1, fp);
      dumpfp (fp);

      ssize_t count = fp->_IO_read_ptr - fp->_IO_read_base;

      TEST_VERIFY (fp->_IO_read_base != NULL);
      if (f == test_source_dev_null)
	{
	  TEST_VERIFY (fc == 0);
	  TEST_VERIFY (count == 0);
	}
      else if (f == test_source_pseudo_terminal)
	{
	  TEST_VERIFY (fc == 9);
	  TEST_VERIFY (count == 3 || count == 10);
	}
      else
	{
	  TEST_VERIFY (fc == 9);
	  TEST_VERIFY (count == 10);
	}

      /* We already checked for the first character being 'a'.  */
      if (count > 1)
	{
	  TEST_COMPARE_BLOB (buf, count - 1, test_data + 1, count - 1);
	  TEST_COMPARE_BLOB (fp->_IO_read_base, count, test_data, count);
	}
    }
  else
    {
      dumpfp (fp);
      test_put_string (fp, test_data + 1, 10 - 1);
      dumpfp (fp);
      TEST_COMPARE (fp->_IO_write_ptr - fp->_IO_write_base, 3);
      /* The first "abcdef\n" got flushed, leaving "abc".  */
      TEST_COMPARE_BLOB (fp->_IO_write_base, 3, test_data + 7, 3);
    }

  TEST_COMPARE ((fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF)), _IO_LINE_BUF);
  if (c != test_config_none)
    TEST_COMPARE (__fbufsize (fp), bufsize);
  return 0;
}

int
verify_unbuffered (FILE *fp,
		   enum test_source_case f,
		   enum test_stream_case s,
		   enum test_config_case c)
{
  debug;
  if (test_stream_reads[s])
    {
      /* We've already read one byte.  */
      dumpfp (fp);
      TEST_VERIFY (fp->_IO_read_base != NULL);
      if (f == test_source_dev_null)
	TEST_COMPARE (fp->_IO_read_ptr - fp->_IO_read_base, 0);
      else
	{
	  TEST_COMPARE (fp->_IO_read_ptr - fp->_IO_read_base, 1);
	  TEST_COMPARE (fp->_IO_read_base[0], test_data[0]);
	  TEST_VERIFY (fp->_IO_read_ptr == fp->_IO_read_end);
	}
    }
  else
    {
      dumpfp (fp);
      fputc (test_data[1], fp);
      dumpfp (fp);
      TEST_COMPARE (fp->_IO_write_ptr - fp->_IO_write_base, 0);
      TEST_COMPARE (fp->_IO_write_base[0], test_data[1]);
      TEST_VERIFY (fp->_IO_write_end == fp->_IO_write_base);
    }
  TEST_COMPARE ((fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF)),
	       _IO_UNBUFFERED);
  TEST_COMPARE (__fbufsize (fp), 1);
  return 0;
}

static int
do_setvbuf (FILE *fp, void *buf, int flags, int size,
	    enum test_stream_case s)
{
  if (s != test_stream_stdout)
    printf ("SETVBUF %p %p %s %d\n",
	    fp, buf,
	    flags == _IONBF ? "_IONBF"
	    : flags == _IOLBF ? "_IOLBF"
	    : flags == _IOFBF ? "_IOFBF"
	    : "???", size);
  if (setvbuf (fp, buf, flags, size))
    {
      perror ("setvbuf");
      return 1;
    }
  return 0;
}

int
do_second_part (FILE *fp,
		enum test_source_case f,
		enum test_stream_case s,
		enum test_config_case c)
{
  /* At this point, FP is the stream to test according to the other
     parameters.  */

  int rv = 0;
  int flags_before;
  int flags_after;

  debug;

  flags_before = fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF);

  /* This is where we do the thing we're testing for.  */
  switch (c)
    {
    case test_config_none:
      /* Buffering is unchanged.  */
      break;

    case test_config_unbuffered:
      do_setvbuf (fp, NULL, _IONBF, 0, s);
      break;

    case test_config_line:
      do_setvbuf (fp, buffer, _IOLBF, bufsize, s);
      break;

    case test_config_fully:
      do_setvbuf (fp, buffer, _IOFBF, bufsize, s);
      break;

    default:
      abort ();
    }

  flags_after = fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF);

  /* Check the buffer mode after we touch it, if we touched it.  */
  switch (c)
    {
    case test_config_none:
      /* Buffering is unchanged, but may change on the first read/write.  */
      TEST_COMPARE (flags_after, flags_before);
      break;

    case test_config_unbuffered:
      TEST_COMPARE (flags_after, _IO_UNBUFFERED);
      break;

    case test_config_line:
      TEST_COMPARE (flags_after, _IO_LINE_BUF);
      break;

    case test_config_fully:
      TEST_COMPARE (flags_after, 0);
      break;

    default:
      abort ();
    }

  /* Glibc defers calculating the appropriate buffering mechanism
     until it reads from or writes to the device.  So we read one
     character here, and account for that in the tests.  */
  if (test_stream_reads[s])
    {
      dumpfp (fp);
      int c = fgetc (fp);
      if (c != TEST_STRING[0] && f != test_source_dev_null)
	FAIL ("first char read is %c not %c", c, TEST_STRING[0]);
      dumpfp (fp);
    }
  else
    {
      dumpfp (fp);
      fputc (TEST_STRING[0], fp);
      dumpfp (fp);
    }

  switch (fp->_flags & (_IO_UNBUFFERED | _IO_LINE_BUF))
    {
    case _IO_LINE_BUF:
      rv += verify_line_buffered (fp, f, s, c);
      break;

    case _IO_UNBUFFERED:
      rv += verify_unbuffered (fp, f, s, c);
      break;

    case 0: /* Fully buffered.  */
      rv += verify_fully_buffered (fp, f, s, c);
      break;

    default:
      abort ();
    }


  xfclose (fp);
  return rv;
}

/*------------------------------------------------------------*/

#ifdef INDEPENDENT_PART
/* This part is the independent sub-process we call to test stdin et
   al.  */

int
main (int argc, char **argv)
{
  /* This is one of the subprocesses we created to test stdin et
     al.  */
  FILE *fp;

  /* If we're called as a regular test, instead of as a sub-process,
     don't complain.  */
  if (argc == 1)
    return 0;

  if (argc != 4)
    {
      int i;
      for (i = 0; i <= argc; i ++)
	printf ("argv[%d] = `%s'\n", i, argv[i] ?: "(null)");
      FAIL_EXIT1 ("sub-process called wrong");
    }

  prepare_test_data ();

  enum test_source_case f = atoi (argv[1]);
  enum test_stream_case s = atoi (argv[2]);
  enum test_config_case c = atoi (argv[3]);

  if (s != test_stream_stdout)
    printf ("\n\033[41mRunning test %s : %s : %s\033[0m\n",
	    test_source_name[f],
	    test_stream_name[s],
	    test_config_name[c]);

  switch (s)
    {
    case test_stream_stdin:
      fp = stdin;
      break;
    case test_stream_stdout:
      fp = stdout;
      break;
    case test_stream_stderr:
      fp = stderr;
      break;
    default:
      abort ();
    }

  return do_second_part (fp, f, s, c);
}

#else
/* This part is the standard test process.  */

/* Spawn an independent sub-process with std* redirected.  */
int
recurse (FILE *fp,
	 enum test_source_case f,
	 enum test_stream_case s,
	 enum test_config_case c)
{
  /* We need to test stdin, stdout, or stderr, which means creating a
     subprocess with one of those redirected from FP.  */
  debug;

  pid_t pid;
  int status;

  pid = fork ();

  switch (pid)
    {
    case -1: /* error */
      perror ("fork");
      return 1;
      break;

    default: /* parent */
      xfclose (fp);
      xwaitpid (pid, &status, 0);
      if (WIFEXITED (status)
	  && WEXITSTATUS (status) == 0)
	return 0;
      return 1;

    case 0: /* child */
      switch (s)
	{
	case test_stream_stdin:
	  xclose (0);
	  dup2 (fileno (fp), 0);
	  break;
	case test_stream_stdout:
	  xclose (1);
	  dup2 (fileno (fp), 1);
	  break;
	case test_stream_stderr:
	  xclose (2);
	  dup2 (fileno (fp), 2);
	  break;
	default:
	  abort ();
	}
      fclose (fp);

      /* At this point, we have to run a program... which is tricky to
	 properly support for remote targets or crosses, because of
	 glibc versions etc.  Hence we run in a test-container.  */

      char fs[10], ss[10], cs[10];
      sprintf (fs, "%d", f);
      sprintf (ss, "%d", s);
      sprintf (cs, "%d", c);
      execl (IND_PROC, IND_PROC, fs, ss, cs, NULL);
      if (s == test_stream_stdout)
	fprintf (stderr, "execl (%s) failed, ", IND_PROC);
      else
	printf ("execl (%s) failed, ", IND_PROC);
      perror ("The error was");
      exit (1);
      break;
    }

  return 0;
}

int
do_test (void)
{
  int rv = 0;

  signal (SIGPIPE, SIG_IGN);

  prepare_test_data ();

  for (enum test_source_case f = 0; f < test_source_count; ++f)
    for (enum test_stream_case s = 0; s < test_stream_count; ++s)
      for (enum test_config_case c = 0; c < test_config_count; ++c)
	{
	  printf ("\n\033[43mRunning test %s : %s : %s\033[0m\n",
		  test_source_name[f],
		  test_stream_name[s],
		  test_config_name[c]);

	  FILE *fp = open_test_stream (f, s);

	  if (fp)
	    {

	      if (s <= test_stream_stderr)
		rv += recurse (fp, f, s, c);
	      else
		rv += do_second_part (fp, f, s, c);

	      unprepare_test_file (fp, f, s);
	    }
	}

  free (buffer);

  printf ("return %d\n", rv);
  return rv;
}

# include <support/test-driver.c>
#endif
