/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)syslog.c	8.4 (Berkeley) 3/18/94";
#endif /* LIBC_SCCS and not lint */

#include <libio/libioP.h>
#include <paths.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <syslog.h>
#include <limits.h>

static int LogType = SOCK_DGRAM;	/* type of socket connection */
static int LogFile = -1;		/* fd for log */
static bool connected;			/* have done connect */
static int LogStat;			/* status bits, set by openlog() */
static const char *LogTag;		/* string to tag the entry with */
static int LogFacility = LOG_USER;	/* default facility code */
static int LogMask = 0xff;		/* mask of priorities to be logged */
extern char *__progname;		/* Program name, from crt0. */

/* Define the lock.  */
__libc_lock_define_initialized (static, syslog_lock)
static void openlog_internal (const char *, int, int);
static void closelog_internal (void);

struct cleanup_arg
{
  void *buf;
  struct sigaction *oldaction;
};

static void
cancel_handler (void *ptr)
{
  /* Restore the old signal handler.  */
  struct cleanup_arg *clarg = (struct cleanup_arg *) ptr;

  if (clarg != NULL)
    /* Free the memstream buffer,  */
    free (clarg->buf);

  /* Free the lock.  */
  __libc_lock_unlock (syslog_lock);
}


/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
void
__syslog (int pri, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  __vsyslog_internal (pri, fmt, ap, 0);
  va_end (ap);
}
ldbl_hidden_def (__syslog, syslog)
ldbl_strong_alias (__syslog, syslog)

void
__vsyslog (int pri, const char *fmt, va_list ap)
{
  __vsyslog_internal (pri, fmt, ap, 0);
}
ldbl_weak_alias (__vsyslog, vsyslog)

void
___syslog_chk (int pri, int flag, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  __vsyslog_internal (pri, fmt, ap, (flag > 0) ? PRINTF_FORTIFY : 0);
  va_end (ap);
}
ldbl_hidden_def (___syslog_chk, __syslog_chk)
ldbl_strong_alias (___syslog_chk, __syslog_chk)

void
__vsyslog_chk (int pri, int flag, const char *fmt, va_list ap)
{
  __vsyslog_internal (pri, fmt, ap, (flag > 0) ? PRINTF_FORTIFY : 0);
}

void
__vsyslog_internal (int pri, const char *fmt, va_list ap,
		    unsigned int mode_flags)
{
  /* Try to use a static buffer as an optimization.  */
  char bufs[1024];
  char *buf = bufs;
  size_t bufsize;

  int msgoff;
  int saved_errno = errno;

#define	INTERNALLOG LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID
  /* Check for invalid bits. */
  if (pri & ~(LOG_PRIMASK | LOG_FACMASK))
    {
      syslog (INTERNALLOG, "syslog: unknown facility/priority: %x", pri);
      pri &= LOG_PRIMASK | LOG_FACMASK;
    }

  /* Prepare for multiple users.  We have to take care: most syscalls we are
     using are cancellation points.  */
  struct cleanup_arg clarg = { NULL, NULL };
  __libc_cleanup_push (cancel_handler, &clarg);
  __libc_lock_lock (syslog_lock);

  /* Check priority against setlogmask values. */
  if ((LOG_MASK (LOG_PRI (pri)) & LogMask) == 0)
    goto out;

  /* Set default facility if none specified. */
  if ((pri & LOG_FACMASK) == 0)
    pri |= LogFacility;

  pid_t pid = LogStat & LOG_PID ? __getpid () : 0;

  /* "%b %e %H:%M:%S "  */
  char timestamp[sizeof "MMM DD hh:mm:ss "];
  __time64_t now = time64_now ();
  struct tm now_tm;
  struct tm *now_tmp = __localtime64_r (&now, &now_tm);
  bool has_ts = now_tmp != NULL;

  /* In the unlikely case of localtime_r failure (tm_year out of int range)
     skip the hostname so the message is handled as valid PRI but without
     TIMESTAMP or invalid TIMESTAMP (which should force the relay to add the
     timestamp itself).  */
  if (has_ts)
    __strftime_l (timestamp, sizeof timestamp, "%h %e %T ", now_tmp,
		  _nl_C_locobj_ptr);

#define SYSLOG_HEADER(__pri, __timestamp, __msgoff, pid) \
  "<%d>%s%n%s%s%.0d%s: ",                                \
  __pri, __timestamp, __msgoff,                          \
  LogTag == NULL ? __progname : LogTag,                  \
  "[" + (pid == 0), pid, "]" + (pid == 0)

#define SYSLOG_HEADER_WITHOUT_TS(__pri, __msgoff)        \
  "<%d>: %n", __pri, __msgoff

  int l, vl;
  if (has_ts)
    l = __snprintf (bufs, sizeof bufs,
		    SYSLOG_HEADER (pri, timestamp, &msgoff, pid));
  else
    l = __snprintf (bufs, sizeof bufs,
		    SYSLOG_HEADER_WITHOUT_TS (pri, &msgoff));
  if (l < 0)
    goto out;

  char *pos;
  size_t len;

  if (l < sizeof bufs)
    {
      /* At this point, there is still a chance that we can print the
         remaining part of the log into bufs and use that.  */
      pos = bufs + l;
      len = sizeof (bufs) - l;
    }
  else
    {
      buf = NULL;
      /* We already know that bufs is too small to use for this log message.
         The next vsnprintf into bufs is used only to calculate the total
         required buffer length.  We will discard bufs contents and allocate
         an appropriately sized buffer later instead.  */
      pos = bufs;
      len = sizeof (bufs);
    }

  {
    va_list apc;
    va_copy (apc, ap);

    /* Restore errno for %m format.  */
    __set_errno (saved_errno);

    vl = __vsnprintf_internal (pos, len, fmt, apc, mode_flags);
    va_end (apc);

    if (vl < 0 || vl >= INT_MAX - l)
      goto out;

    if (vl >= len)
      buf = NULL;

    bufsize = l + vl;
  }

  if (buf == NULL)
    {
      buf = malloc ((bufsize + 1) * sizeof (char));
      if (buf != NULL)
	{
	  /* Tell the cancellation handler to free this buffer.  */
	  clarg.buf = buf;

	  int cl;
	  if (has_ts)
	    cl = __snprintf (buf, l + 1,
			     SYSLOG_HEADER (pri, timestamp, &msgoff, pid));
	  else
	    cl = __snprintf (buf, l + 1,
			     SYSLOG_HEADER_WITHOUT_TS (pri, &msgoff));
	  if (cl != l)
	    goto out;

	  va_list apc;
	  va_copy (apc, ap);
	  cl = __vsnprintf_internal (buf + l, bufsize - l + 1, fmt, apc,
				     mode_flags);
	  va_end (apc);

	  if (cl != vl)
	    goto out;
	}
      else
        {
          int bl;
	  /* Nothing much to do but emit an error message.  */
          bl = __snprintf (bufs, sizeof bufs,
                           "out of memory[%d]", __getpid ());
          if (bl < 0 || bl >= sizeof bufs)
            goto out;

          bufsize = bl;
          buf = bufs;
          msgoff = 0;
        }
    }

  /* Output to stderr if requested. */
  if (LogStat & LOG_PERROR)
    __dprintf (STDERR_FILENO, "%s%s", buf + msgoff,
	       "\n" + (buf[bufsize - 1] == '\n'));

  /* Get connected, output the message to the local logger.  */
  if (!connected)
    openlog_internal (NULL, LogStat | LOG_NDELAY, LogFacility);

  /* If we have a SOCK_STREAM connection, also send ASCII NUL as a record
     terminator.  */
  if (LogType == SOCK_STREAM)
    ++bufsize;

  if (!connected || __send (LogFile, buf, bufsize, MSG_NOSIGNAL) < 0)
    {
      if (connected)
	{
	  /* Try to reopen the syslog connection.  Maybe it went down.  */
	  closelog_internal ();
	  openlog_internal (NULL, LogStat | LOG_NDELAY, LogFacility);
	}

      if (!connected || __send (LogFile, buf, bufsize, MSG_NOSIGNAL) < 0)
	{
	  closelog_internal ();	/* attempt re-open next time */
	  /*
	   * Output the message to the console; don't worry
	   * about blocking, if console blocks everything will.
	   * Make sure the error reported is the one from the
	   * syslogd failure.
	   */
	  int fd;
	  if (LogStat & LOG_CONS &&
	      (fd = __open (_PATH_CONSOLE, O_WRONLY | O_NOCTTY
			    | O_CLOEXEC, 0)) >= 0)
	    {
	      __dprintf (fd, "%s\r\n", buf + msgoff);
	      __close (fd);
	    }
	}
    }

out:
  /* End of critical section.  */
  __libc_cleanup_pop (0);
  __libc_lock_unlock (syslog_lock);

  if (buf != bufs)
    free (buf);
}

/* AF_UNIX address of local logger  */
static const struct sockaddr_un SyslogAddr =
  {
    .sun_family = AF_UNIX,
    .sun_path = _PATH_LOG
  };

static void
openlog_internal (const char *ident, int logstat, int logfac)
{
  if (ident != NULL)
    LogTag = ident;
  LogStat = logstat;
  if ((logfac & ~LOG_FACMASK) == 0)
    LogFacility = logfac;

  int retry = 0;
  while (retry < 2)
    {
      if (LogFile == -1)
	{
	  if (LogStat & LOG_NDELAY)
	    {
	      LogFile = __socket (AF_UNIX, LogType | SOCK_CLOEXEC, 0);
	      if (LogFile == -1)
		return;
	    }
	}
      if (LogFile != -1 && !connected)
	{
	  int old_errno = errno;
	  if (__connect (LogFile, &SyslogAddr, sizeof (SyslogAddr)) == -1)
	    {
	      int saved_errno = errno;
	      int fd = LogFile;
	      LogFile = -1;
	      __close (fd);
	      __set_errno (old_errno);
	      if (saved_errno == EPROTOTYPE)
		{
		  /* retry with the other type: */
		  LogType = LogType == SOCK_DGRAM ? SOCK_STREAM : SOCK_DGRAM;
		  ++retry;
		  continue;
		}
	    }
	  else
	    connected = true;
	}
      break;
    }
}

void
openlog (const char *ident, int logstat, int logfac)
{
  /* Protect against multiple users and cancellation.  */
  __libc_cleanup_push (cancel_handler, NULL);
  __libc_lock_lock (syslog_lock);

  openlog_internal (ident, logstat, logfac);

  __libc_cleanup_pop (1);
}

static void
closelog_internal (void)
{
  if (!connected)
    return;

  __close (LogFile);
  LogFile = -1;
  connected = false;
}

void
closelog (void)
{
  /* Protect against multiple users and cancellation.  */
  __libc_cleanup_push (cancel_handler, NULL);
  __libc_lock_lock (syslog_lock);

  closelog_internal ();
  LogTag = NULL;
  LogType = SOCK_DGRAM; /* this is the default */

  /* Free the lock.  */
  __libc_cleanup_pop (1);
}

/* setlogmask -- set the log mask level */
int
setlogmask (int pmask)
{
  int omask;

  /* Protect against multiple users.  */
  __libc_lock_lock (syslog_lock);

  omask = LogMask;
  if (pmask != 0)
    LogMask = pmask;

  __libc_lock_unlock (syslog_lock);

  return (omask);
}
