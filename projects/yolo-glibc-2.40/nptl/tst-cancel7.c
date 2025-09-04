/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>
#include <support/xthread.h>

static const char *command;
static const char *pidfile;
static const char *semfile;
static char *pidfilename;
static char *semfilename;

static sem_t *sem;

static void *
tf (void *arg)
{
  char *cmd = xasprintf ("%s --direct --sem %s --pidfile %s",
			 command, semfilename, pidfilename);
  if (system (cmd))
    FAIL_EXIT1("system call unexpectedly returned");
  /* This call should never return.  */
  return NULL;
}

static void
sl (void)
{
  FILE *f = xfopen (pidfile, "w");

  fprintf (f, "%lld\n", (long long) getpid ());
  fflush (f);

  if (sem_post (sem) != 0)
    FAIL_EXIT1 ("sem_post: %m");

  struct flock fl =
    {
      .l_type = F_WRLCK,
      .l_start = 0,
      .l_whence = SEEK_SET,
      .l_len = 1
    };
  if (fcntl (fileno (f), F_SETLK, &fl) != 0)
    FAIL_EXIT1 ("fcntl (F_SETFL): %m");

  sigset_t ss;
  sigfillset (&ss);
  sigsuspend (&ss);
  exit (0);
}


static void
do_prepare (int argc, char *argv[])
{
  int semfd;
  if (semfile == NULL)
    semfd = create_temp_file ("tst-cancel7.", &semfilename);
  else
    semfd = open (semfile, O_RDWR);
  TEST_VERIFY_EXIT (semfd != -1);

  sem = xmmap (NULL, sizeof (sem_t), PROT_READ | PROT_WRITE, MAP_SHARED,
	       semfd);
  TEST_VERIFY_EXIT (sem != SEM_FAILED);
  if (semfile == NULL)
    {
      xftruncate (semfd, sizeof (sem_t));
      TEST_VERIFY_EXIT (sem_init (sem, 1, 0) != -1);
    }

  if (command == NULL)
    command = argv[0];

  if (pidfile)
    sl ();

  int fd = create_temp_file ("tst-cancel7-pid-", &pidfilename);
  if (fd == -1)
    FAIL_EXIT1 ("create_temp_file failed: %m");

  xwrite (fd, " ", 1);
  xclose (fd);
}


static int
do_test (void)
{
  pthread_t th = xpthread_create (NULL, tf, NULL);

  /* Wait to cancel until after the pid is written.  */
  if (sem_wait (sem) != 0)
    FAIL_EXIT1 ("sem_wait: %m");

  xpthread_cancel (th);
  void *r = xpthread_join (th);

  FILE *f = xfopen (pidfilename, "r+");

  long long ll;
  if (fscanf (f, "%lld\n", &ll) != 1)
    FAIL_EXIT1 ("fscanf: %m");

  struct flock fl =
    {
      .l_type = F_WRLCK,
      .l_start = 0,
      .l_whence = SEEK_SET,
      .l_len = 1
    };
  if (fcntl (fileno (f), F_GETLK, &fl) != 0)
    FAIL_EXIT1 ("fcntl: %m");

  if (fl.l_type != F_UNLCK)
    {
      printf ("child %lld still running\n", (long long) fl.l_pid);
      if (fl.l_pid == ll)
	kill (fl.l_pid, SIGKILL);

      return 1;
    }

  xfclose (f);

  return r != PTHREAD_CANCELED;
}

static void
do_cleanup (void)
{
  FILE *f = fopen (pidfilename, "r+");
  long long ll;

  if (f != NULL && fscanf (f, "%lld\n", &ll) == 1)
    {
      struct flock fl =
	{
	  .l_type = F_WRLCK,
	  .l_start = 0,
	  .l_whence = SEEK_SET,
	  .l_len = 1
	};
      if (fcntl (fileno (f), F_GETLK, &fl) == 0 && fl.l_type != F_UNLCK
	  && fl.l_pid == ll)
	kill (fl.l_pid, SIGKILL);

      fclose (f);
    }
}

#define OPT_COMMAND	10000
#define OPT_PIDFILE	10001
#define OPT_SEMFILE	10002
#define CMDLINE_OPTIONS \
  { "command", required_argument, NULL, OPT_COMMAND },	\
  { "pidfile", required_argument, NULL, OPT_PIDFILE },  \
  { "sem",     required_argument, NULL, OPT_SEMFILE },
static void
cmdline_process (int c)
{
  switch (c)
    {
    case OPT_COMMAND:
      command = optarg;
      break;
    case OPT_PIDFILE:
      pidfile = optarg;
      break;
    case OPT_SEMFILE:
      semfile = optarg;
      break;
    }
}
#define CMDLINE_PROCESS cmdline_process
#define CLEANUP_HANDLER do_cleanup
#define PREPARE do_prepare
#include <support/test-driver.c>
