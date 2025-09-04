/* Bug 28213: test for NULL pointer dereference in mq_notify.
   Copyright The GNU Toolchain Authors.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>

static mqd_t m = -1;
static const char msg[] = "hello";

static void
check_bz28213_cb (union sigval sv)
{
  char buf[sizeof (msg)];

  (void) sv;

  TEST_VERIFY_EXIT ((size_t) mq_receive (m, buf, sizeof (buf), NULL)
		    == sizeof (buf));
  TEST_VERIFY_EXIT (memcmp (buf, msg, sizeof (buf)) == 0);

  exit (0);
}

static void
check_bz28213 (void)
{
  struct sigevent sev;

  memset (&sev, '\0', sizeof (sev));
  sev.sigev_notify = SIGEV_THREAD;
  sev.sigev_notify_function = check_bz28213_cb;

  /* Step 1: Register & unregister notifier.
     Helper thread should receive NOTIFY_REMOVED notification.
     In a vulnerable version of glibc, NULL pointer dereference follows. */
  TEST_VERIFY_EXIT (mq_notify (m, &sev) == 0);
  TEST_VERIFY_EXIT (mq_notify (m, NULL) == 0);

  /* Step 2: Once again, register notification.
     Try to send one message.
     Test is considered successful, if the callback does exit (0). */
  TEST_VERIFY_EXIT (mq_notify (m, &sev) == 0);
  TEST_VERIFY_EXIT (mq_send (m, msg, sizeof (msg), 1) == 0);

  /* Wait... */
  pause ();
}

static int
do_test (void)
{
  static const char m_name[] = "/bz28213_queue";
  struct mq_attr m_attr;

  memset (&m_attr, '\0', sizeof (m_attr));
  m_attr.mq_maxmsg = 1;
  m_attr.mq_msgsize = sizeof (msg);

  m = mq_open (m_name,
               O_RDWR | O_CREAT | O_EXCL,
               0600,
               &m_attr);

  if (m < 0)
    {
      if (errno == ENOSYS)
        FAIL_UNSUPPORTED ("POSIX message queues are not implemented\n");
      FAIL_EXIT1 ("Failed to create POSIX message queue: %m\n");
    }

  TEST_VERIFY_EXIT (mq_unlink (m_name) == 0);

  check_bz28213 ();

  return 0;
}

#include <support/test-driver.c>
