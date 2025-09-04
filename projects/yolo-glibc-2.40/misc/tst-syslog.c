/* Basic tests for syslog interfaces.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <fcntl.h>
#include <paths.h>
#include <netinet/in.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/un.h>

static const int facilities[] =
  {
    LOG_KERN,
    LOG_USER,
    LOG_MAIL,
    LOG_DAEMON,
    LOG_AUTH,
    LOG_SYSLOG,
    LOG_LPR,
    LOG_NEWS,
    LOG_UUCP,
    LOG_CRON,
    LOG_AUTHPRIV,
    LOG_FTP,
    LOG_LOCAL0,
    LOG_LOCAL1,
    LOG_LOCAL2,
    LOG_LOCAL3,
    LOG_LOCAL4,
    LOG_LOCAL5,
    LOG_LOCAL6,
    LOG_LOCAL7,
  };

static const int priorities[] =
  {
    LOG_EMERG,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
  };

#define IDENT_LENGTH 64
#define MSG_LENGTH   1024

#define SYSLOG_MSG_BASE "syslog_message"
#define OPENLOG_IDENT   "openlog_ident"
static char large_message[MSG_LENGTH];

struct msg_t
  {
    int priority;
    int facility;
    char ident[IDENT_LENGTH];
    char msg[MSG_LENGTH];
    pid_t pid;
  };

static void
call_vsyslog (int priority, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vsyslog (priority, format, ap);
  va_end (ap);
}

static void
send_vsyslog (int options)
{
  for (size_t i = 0; i < array_length (facilities); i++)
    {
      for (size_t j = 0; j < array_length (priorities); j++)
        {
          int facility = facilities[i];
          int priority = priorities[j];
          call_vsyslog (facility | priority, "%s %d %d", SYSLOG_MSG_BASE,
                        facility, priority);
        }
    }
}

static void
send_syslog (int options)
{
  for (size_t i = 0; i < array_length (facilities); i++)
    {
      for (size_t j = 0; j < array_length (priorities); j++)
        {
          int facility = facilities[i];
          int priority = priorities[j];
          syslog (facility | priority, "%s %d %d", SYSLOG_MSG_BASE, facility,
                  priority);
        }
    }
}

static bool
check_syslog_message (const struct msg_t *msg, int msgnum, int options,
                      pid_t pid)
{
  if (msgnum == array_length (facilities) * array_length (priorities) - 1)
    return false;

  int i = msgnum / array_length (priorities);
  int j = msgnum % array_length (priorities);

  int expected_facility = facilities[i];
  /* With no preceding openlog, syslog default to LOG_USER.  */
  if (expected_facility == LOG_KERN)
      expected_facility = LOG_USER;
  int expected_priority = priorities[j];

  TEST_COMPARE (msg->facility, expected_facility);
  TEST_COMPARE (msg->priority, expected_priority);

  return true;
}

static void
send_syslog_large (int options)
{
  int facility = LOG_USER;
  int priority = LOG_INFO;

  syslog (facility | priority, "%s %d %d", large_message, facility,
	  priority);
}

static void
send_vsyslog_large (int options)
{
  int facility = LOG_USER;
  int priority = LOG_INFO;

  call_vsyslog (facility | priority, "%s %d %d", large_message, facility,
		priority);
}

static bool
check_syslog_message_large (const struct msg_t *msg, int msgnum, int options,
			    pid_t pid)
{
  TEST_COMPARE (msg->facility, LOG_USER);
  TEST_COMPARE (msg->priority, LOG_INFO);
  TEST_COMPARE_STRING (msg->msg, large_message);

  return false;
}

static void
send_openlog (int options)
{
  /* Define a non-default IDENT and a not default facility.  */
  openlog (OPENLOG_IDENT, options, LOG_LOCAL0);
  for (size_t j = 0; j < array_length (priorities); j++)
    {
      int priority = priorities[j];
      syslog (priority, "%s %d %d", SYSLOG_MSG_BASE, LOG_LOCAL0, priority);
    }
  closelog ();

  /* Back to the default IDENT with a non default facility.  */
  openlog (NULL, 0, LOG_LOCAL6);
  for (size_t j = 0; j < array_length (priorities); j++)
    {
      int priority = priorities[j];
      syslog (LOG_LOCAL7 | priority, "%s %d %d", SYSLOG_MSG_BASE, LOG_LOCAL7,
        priority);
    }
  closelog ();

  /* LOG_KERN does not change the internal default facility.  */
  openlog (NULL, 0, LOG_KERN);
  for (size_t j = 0; j < array_length (priorities); j++)
    {
      int priority = priorities[j];
      syslog (priority, "%s %d %d", SYSLOG_MSG_BASE, LOG_KERN, priority);
    }
  closelog ();
}

static void
send_openlog_large (int options)
{
  /* Define a non-default IDENT and a not default facility.  */
  openlog (OPENLOG_IDENT, options, LOG_LOCAL0);

  syslog (LOG_INFO, "%s %d %d", large_message, LOG_LOCAL0, LOG_INFO);

  closelog ();
}

static bool
check_openlog_message (const struct msg_t *msg, int msgnum,
                       int options, pid_t pid)
{
  if (msgnum == 3 * array_length (priorities) - 1)
    return false;

  int expected_priority = priorities[msgnum % array_length (priorities)];
  TEST_COMPARE (msg->priority, expected_priority);

  char expected_ident[IDENT_LENGTH];
  snprintf (expected_ident, sizeof (expected_ident), "%s%s%.0d%s:",
            OPENLOG_IDENT,
            options & LOG_PID ? "[" : "",
            options & LOG_PID ? pid : 0,
            options & LOG_PID ? "]" : "");

  if (msgnum < array_length (priorities))
    {
      if (options & LOG_PID)
        TEST_COMPARE (msg->pid, pid);
      TEST_COMPARE_STRING (msg->ident, expected_ident);
      TEST_COMPARE (msg->facility, LOG_LOCAL0);
    }
  else if (msgnum < 2 * array_length (priorities))
    TEST_COMPARE (msg->facility, LOG_LOCAL7);
  else if (msgnum < 3 * array_length (priorities))
    TEST_COMPARE (msg->facility, LOG_KERN);

  return true;
}

static bool
check_openlog_message_large (const struct msg_t *msg, int msgnum,
			     int options, pid_t pid)
{
  char expected_ident[IDENT_LENGTH];
  snprintf (expected_ident, sizeof (expected_ident), "%s%s%.0d%s:",
            OPENLOG_IDENT,
            options & LOG_PID ? "[" : "",
            options & LOG_PID ? pid : 0,
            options & LOG_PID ? "]" : "");

  TEST_COMPARE_STRING (msg->ident, expected_ident);
  TEST_COMPARE_STRING (msg->msg, large_message);
  TEST_COMPARE (msg->priority, LOG_INFO);
  TEST_COMPARE (msg->facility, LOG_LOCAL0);

  return false;
}

static struct msg_t
parse_syslog_msg (const char *msg)
{
  struct msg_t r = { .pid = -1 };
  int number;
  int wsb, wsa;

#define STRINPUT(size)  XSTRINPUT(size)
#define XSTRINPUT(size) "%" # size "s"

  /* The message in the form:
     <179>Apr  8 14:51:19 tst-syslog: message 176 3  */
  int n = sscanf (msg, "<%3d>%*s %*d %*d:%*d:%*d%n %n" STRINPUT(IDENT_LENGTH)
		       " " STRINPUT(MSG_LENGTH) " %*d %*d",
                  &number, &wsb, &wsa, r.ident, r.msg);
  TEST_COMPARE (n, 3);
  /* It should only one space between timestamp and message.  */
  TEST_COMPARE (wsa - wsb, 1);

  r.facility = number & LOG_FACMASK;
  r.priority = number & LOG_PRIMASK;

  char *pid_start = strchr (r.ident, '[');
  if (pid_start != NULL)
    {
       char *pid_end = strchr (r.ident, ']');
       if (pid_end != NULL)
         r.pid = strtoul (pid_start + 1, NULL, 10);
    }

  return r;
}

static struct msg_t
parse_syslog_console (const char *msg)
{
  int priority;
  int facility;
  struct msg_t r;

  /* The message in the form:
     openlog_ident: syslog_message 128 0  */
  int n = sscanf (msg, STRINPUT(IDENT_LENGTH) " " STRINPUT(MSG_LENGTH) " %d %d",
      r.ident, r.msg, &facility, &priority);
  TEST_COMPARE (n, 4);

  r.facility = facility;
  r.priority = priority;

  return r;
}

static void
check_syslog_udp (void (*syslog_send)(int), int options,
                  bool (*syslog_check)(const struct msg_t *, int, int,
                                       pid_t))
{
  struct sockaddr_un addr =
    {
      .sun_family = AF_UNIX,
      .sun_path = _PATH_LOG
    };

  socklen_t addrlen = sizeof (addr);
  int server_udp = xsocket (AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  xbind (server_udp, (struct sockaddr *) &addr, addrlen);

  pid_t sender_pid = xfork ();
  if (sender_pid == 0)
    {
      syslog_send (options);
      _exit (0);
    }

  int msgnum = 0;
  while (1)
    {
      char buf[2048];
      size_t l = xrecvfrom (server_udp, buf, sizeof (buf), 0,
                            (struct sockaddr *) &addr, &addrlen);
      buf[l] = '\0';

      struct msg_t msg = parse_syslog_msg (buf);
      if (!syslog_check (&msg, msgnum++, options, sender_pid))
        break;
     }

  xclose (server_udp);

  int status;
  xwaitpid (sender_pid, &status, 0);
  TEST_COMPARE (status, 0);

  unlink (_PATH_LOG);
}

static void
check_syslog_tcp (void (*syslog_send)(int), int options,
                  bool (*syslog_check)(const struct msg_t *, int, int,
                                       pid_t))
{
  struct sockaddr_un addr =
    {
      .sun_family = AF_UNIX,
      .sun_path = _PATH_LOG
    };
  socklen_t addrlen = sizeof (addr);

  int server_tcp = xsocket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  xbind (server_tcp, (struct sockaddr *) &addr, addrlen);
  xlisten (server_tcp, 5);

  pid_t sender_pid = xfork ();
  if (sender_pid == 0)
    {
      syslog_send (options);
      _exit (0);
    }

  int client_tcp = xaccept (server_tcp, NULL, NULL);

  char buf[2048], *rb = buf;
  size_t rbl = sizeof (buf);
  size_t prl = 0;  /* Track the size of the partial record.  */
  int msgnum = 0;

  while (1)
    {
      size_t rl = xrecvfrom (client_tcp, rb, rbl - prl, 0, NULL, NULL);
      if (rl == 0)
        break;

      /* Iterate over the buffer to find and check the record.  */
      size_t l = rl + prl;
      char *b = buf;
      while (1)
	{
          /* With TCP each record ends with a '\0'.  */
          char *e = memchr (b, '\0', l);
          if (e != NULL)
            {
              struct msg_t msg = parse_syslog_msg (b);
              if (!syslog_check (&msg, msgnum++, options, sender_pid))
                break;

	      /* Advance to the next record.  */
	      ptrdiff_t diff = e + 1 - b;
	      b += diff;
	      l -= diff;
	    }
	  else
	    {
              /* Move the partial record to the start of the buffer.  */
	      memmove (buf, b, l);
	      rb = buf + l;
	      prl = l;
	      break;
            }
        }
    }

  xclose (client_tcp);
  xclose (server_tcp);

  int status;
  xwaitpid (sender_pid, &status, 0);
  TEST_COMPARE (status, 0);

  unlink (_PATH_LOG);
}

static void
check_syslog_console_read (FILE *fp)
{
  char buf[512];
  int msgnum = 0;
  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      struct msg_t msg = parse_syslog_console (buf);
      TEST_COMPARE_STRING (msg.ident, OPENLOG_IDENT ":");
      TEST_COMPARE (msg.priority, priorities[msgnum]);
      TEST_COMPARE (msg.facility, LOG_LOCAL0);

      if (++msgnum == array_length (priorities))
        break;
    }
}

static void
check_syslog_console_read_large (FILE *fp)
{
  char buf[2048];
  TEST_VERIFY (fgets (buf, sizeof (buf), fp) != NULL);
  struct msg_t msg = parse_syslog_console (buf);

  TEST_COMPARE_STRING (msg.ident, OPENLOG_IDENT ":");
  TEST_COMPARE_STRING (msg.msg, large_message);
  TEST_COMPARE (msg.priority, LOG_INFO);
  TEST_COMPARE (msg.facility, LOG_LOCAL0);
}

static void
check_syslog_console (void (*syslog_send)(int),
		      void (*syslog_check)(FILE *fp))
{
  xmkfifo (_PATH_CONSOLE, 0666);

  pid_t sender_pid = xfork ();
  if (sender_pid == 0)
    {
      syslog_send (LOG_CONS);
      _exit (0);
    }

  {
    FILE *fp = xfopen (_PATH_CONSOLE, "r+");
    syslog_check (fp);
    xfclose (fp);
  }

  int status;
  xwaitpid (sender_pid, &status, 0);
  TEST_COMPARE (status, 0);

  unlink (_PATH_CONSOLE);
}

static void
send_openlog_callback (void *clousure)
{
  int options = *(int *) clousure;
  send_openlog (options);
}

static void
send_openlog_callback_large (void *clousure)
{
  int options = *(int *) clousure;
  send_openlog_large (options);
}

static void
check_syslog_perror (bool large)
{
  struct support_capture_subprocess result;
  result = support_capture_subprocess (large
				       ? send_openlog_callback_large
				       : send_openlog_callback,
                                       &(int){LOG_PERROR});

  FILE *mfp = fmemopen (result.err.buffer, result.err.length, "r");
  if (mfp == NULL)
    FAIL_EXIT1 ("fmemopen: %m");
  if (large)
    check_syslog_console_read_large (mfp);
  else
    check_syslog_console_read (mfp);
  xfclose (mfp);

  support_capture_subprocess_check (&result, "tst-openlog-child", 0,
                                    sc_allow_stderr);
  support_capture_subprocess_free (&result);
}

static int
do_test (void)
{
  /* Send every combination of facility/priority over UDP and TCP.  */
  check_syslog_udp (send_syslog, 0, check_syslog_message);
  check_syslog_tcp (send_syslog, 0, check_syslog_message);

  /* Also check vsyslog.  */
  check_syslog_udp (send_vsyslog, 0, check_syslog_message);
  check_syslog_tcp (send_vsyslog, 0, check_syslog_message);

  /* Run some openlog/syslog/closelog combinations.  */
  check_syslog_udp (send_openlog, 0, check_openlog_message);
  check_syslog_tcp (send_openlog, 0, check_openlog_message);

  /* Check the LOG_PID option.  */
  check_syslog_udp (send_openlog, LOG_PID, check_openlog_message);
  check_syslog_tcp (send_openlog, LOG_PID, check_openlog_message);

  /* Check the LOG_CONS option.  */
  check_syslog_console (send_openlog, check_syslog_console_read);

  /* Check the LOG_PERROR option.  */
  check_syslog_perror (false);

  /* Similar tests as before, but with a large message to trigger the
     syslog path that uses dynamically allocated memory.  */
  memset (large_message, 'a', sizeof large_message - 1);
  large_message[sizeof large_message - 1] = '\0';

  check_syslog_udp (send_syslog_large, 0, check_syslog_message_large);
  check_syslog_tcp (send_syslog_large, 0, check_syslog_message_large);

  check_syslog_udp (send_vsyslog_large, 0, check_syslog_message_large);
  check_syslog_tcp (send_vsyslog_large, 0, check_syslog_message_large);

  check_syslog_udp (send_openlog_large, 0, check_openlog_message_large);
  check_syslog_tcp (send_openlog_large, 0, check_openlog_message_large);

  check_syslog_udp (send_openlog_large, LOG_PID, check_openlog_message_large);
  check_syslog_tcp (send_openlog_large, LOG_PID, check_openlog_message_large);

  check_syslog_console (send_openlog_large, check_syslog_console_read_large);

  check_syslog_perror (true);

  return 0;
}

#include <support/test-driver.c>
