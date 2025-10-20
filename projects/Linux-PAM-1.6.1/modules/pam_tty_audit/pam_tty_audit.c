/* Copyright © 2007, 2008 Red Hat, Inc. All rights reserved.
   Red Hat author: Miloslav Trmač <mitr@redhat.com>

   Redistribution and use in source and binary forms of Linux-PAM, with
   or without modification, are permitted provided that the following
   conditions are met:

   1. Redistributions of source code must retain any existing copyright
      notice, and this entire permission notice in its entirety,
      including the disclaimer of warranties.

   2. Redistributions in binary form must reproduce all prior and current
      copyright notices, this list of conditions, and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

   3. The name of any author may not be used to endorse or promote
      products derived from this software without their specific prior
      written permission.

   ALTERNATIVELY, this product may be distributed under the terms of the
   GNU General Public License, in which case the provisions of the GNU
   GPL are required INSTEAD OF the above restrictions.  (This clause is
   necessary due to a potential conflict between the GNU GPL and the
   restrictions contained in a BSD-style copyright.)

   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
   TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
   DAMAGE. */

#include "config.h"
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libaudit.h>
#include <linux/netlink.h>

#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>

#include "pam_cc_compat.h"
#include "pam_inline.h"

#define DATANAME "pam_tty_audit_last_state"

/* Open an audit netlink socket */
static int
nl_open (void)
{
  return socket (AF_NETLINK, SOCK_RAW, NETLINK_AUDIT);
}

static int
nl_send (int fd, unsigned type, unsigned flags, const void *data, size_t size)
{
  struct sockaddr_nl addr;
  struct msghdr msg;
  struct nlmsghdr nlm;
  struct iovec iov[2];
  ssize_t res;

  nlm.nlmsg_len = NLMSG_LENGTH (size);
  nlm.nlmsg_type = type;
  nlm.nlmsg_flags = NLM_F_REQUEST | flags;
  nlm.nlmsg_seq = 0;
  nlm.nlmsg_pid = 0;
  iov[0].iov_base = &nlm;
  iov[0].iov_len = sizeof (nlm);
  DIAG_PUSH_IGNORE_CAST_QUAL;
  iov[1].iov_base = (void *)data;
  DIAG_POP_IGNORE_CAST_QUAL;
  iov[1].iov_len = size;
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = 0;
  addr.nl_groups = 0;
  msg.msg_name = &addr;
  msg.msg_namelen = sizeof (addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  res = sendmsg (fd, &msg, 0);
  if (res == -1)
    return -1;
  if ((size_t)res != nlm.nlmsg_len)
    {
      errno = EIO;
      return -1;
    }
  return 0;
}

static int
nl_recv (int fd, unsigned type, void *buf, size_t size)
{
  struct sockaddr_nl addr;
  struct msghdr msg;
  struct nlmsghdr nlm;
  struct iovec iov[2];
  ssize_t res, resdiff;

 again:
  iov[0].iov_base = &nlm;
  iov[0].iov_len = sizeof (nlm);
  msg.msg_name = &addr;
  msg.msg_namelen = sizeof (addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  if (type != NLMSG_ERROR)
    {
      res = recvmsg (fd, &msg, MSG_PEEK);
      if (res == -1)
	return -1;
      if (res != NLMSG_LENGTH (0))
	{
	  errno = EIO;
	  return -1;
	}
      if (nlm.nlmsg_type == NLMSG_ERROR)
	{
	  struct nlmsgerr err;

	  iov[1].iov_base = &err;
	  iov[1].iov_len = sizeof (err);
	  msg.msg_iovlen = 2;
	  res = recvmsg (fd, &msg, 0);
	  if (res == -1)
	    return -1;
	  if ((size_t)res != NLMSG_LENGTH (sizeof (err))
	      || nlm.nlmsg_type != NLMSG_ERROR)
	    {
	      errno = EIO;
	      return -1;
	    }
	  if (err.error == 0)
	    goto again;
	  errno = -err.error;
	  return -1;
	}
    }
  if (size != 0)
    {
      iov[1].iov_base = buf;
      iov[1].iov_len = size;
      msg.msg_iovlen = 2;
    }
  res = recvmsg (fd, &msg, 0);
  if (res == -1)
    return -1;
  resdiff = NLMSG_LENGTH(size) - (size_t)res;
  if (resdiff < 0
      || nlm.nlmsg_type != type)
    {
      errno = EIO;
      return -1;
    }
  else if (resdiff > 0)
    {
      memset((char *)buf + size - resdiff, 0, resdiff);
    }
  return 0;
}

static int
nl_recv_ack (int fd)
{
  struct nlmsgerr err;

  if (nl_recv (fd, NLMSG_ERROR, &err, sizeof (err)) != 0)
    return -1;
  if (err.error != 0)
    {
      errno = -err.error;
      return -1;
    }
  return 0;
}

static void
cleanup_old_status (pam_handle_t *pamh, void *data, int error_status)
{
  (void)pamh;
  (void)error_status;
  free (data);
}

enum uid_range { UID_RANGE_NONE, UID_RANGE_MM, UID_RANGE_MIN,
    UID_RANGE_ONE, UID_RANGE_ERR };

static enum uid_range
parse_uid_range(pam_handle_t *pamh, const char *s,
                uid_t *min_uid, uid_t *max_uid)
{
    const char *range = s;
    const char *pmax;
    char *endptr;
    enum uid_range rv = UID_RANGE_MM;

    if ((pmax=strchr(range, ':')) == NULL)
        return UID_RANGE_NONE;
    ++pmax;

    if (range[0] == ':')
        rv = UID_RANGE_ONE;
    else {
            errno = 0;
            *min_uid = strtoul (range, &endptr, 10);
            if (errno != 0 || (range == endptr) || *endptr != ':') {
                pam_syslog(pamh, LOG_DEBUG,
                           "wrong min_uid value in '%s'", s);
                return UID_RANGE_ERR;
            }
    }

    if (*pmax == '\0') {
        if (rv == UID_RANGE_ONE)
            return UID_RANGE_ERR;

        return UID_RANGE_MIN;
    }

    errno = 0;
    *max_uid = strtoul (pmax, &endptr, 10);
    if (errno != 0 || (pmax == endptr) || *endptr != '\0') {
        pam_syslog(pamh, LOG_DEBUG,
                   "wrong max_uid value in '%s'", s);
        return UID_RANGE_ERR;
    }

    if (rv == UID_RANGE_ONE)
        *min_uid = *max_uid;
    return rv;
}

int
pam_sm_open_session (pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  enum command { CMD_NONE, CMD_ENABLE, CMD_DISABLE };

  enum command command;
  struct audit_tty_status *old_status, new_status;
  const char *user;
  int i, fd, open_only;
  struct passwd *pwd;
#ifdef HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD
  int log_passwd;
#endif /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */

  (void)flags;

  if (pam_get_user (pamh, &user, NULL) != PAM_SUCCESS)
    {
      pam_syslog(pamh, LOG_NOTICE, "cannot determine user name");
      return PAM_SESSION_ERR;
    }

  pwd = pam_modutil_getpwnam(pamh, user);
  if (pwd == NULL)
    {
      pam_syslog(pamh, LOG_NOTICE,
                 "open_session unknown user '%s'", user);
      return PAM_SESSION_ERR;
    }

  command = CMD_NONE;
  open_only = 0;
#ifdef HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD
  log_passwd = 0;
#endif /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */
  for (i = 0; i < argc; i++)
    {
      const char *str;

      if ((str = pam_str_skip_prefix(argv[i], "enable=")) != NULL
	  || (str = pam_str_skip_prefix(argv[i], "disable=")) != NULL)
	{
	  enum command this_command;
	  char *copy, *tok_data, *tok;

	  this_command = *argv[i] == 'e' ? CMD_ENABLE : CMD_DISABLE;
	  copy = strdup (str);
	  if (copy == NULL)
	    return PAM_SESSION_ERR;
	  for (tok = strtok_r (copy, ",", &tok_data);
	       tok != NULL && command != this_command;
	       tok = strtok_r (NULL, ",", &tok_data))
	    {
	      uid_t min_uid = 0, max_uid = 0;
	      switch (parse_uid_range(pamh, tok, &min_uid, &max_uid))
		{
		case UID_RANGE_NONE:
		    if (fnmatch (tok, user, 0) == 0)
			command = this_command;
		    break;
		case UID_RANGE_MM:
		    if (pwd->pw_uid >= min_uid && pwd->pw_uid <= max_uid)
			command = this_command;
		    break;
		case UID_RANGE_MIN:
		    if (pwd->pw_uid >= min_uid)
			command = this_command;
		    break;
		case UID_RANGE_ONE:
		    if (pwd->pw_uid == max_uid)
			command = this_command;
		    break;
		case UID_RANGE_ERR:
		    break;
		}
	    }
	  free (copy);
	}
      else if (strcmp (argv[i], "open_only") == 0)
	open_only = 1;
      else if (strcmp (argv[i], "log_passwd") == 0)
#ifdef HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD
        log_passwd = 1;
#else /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */
        pam_syslog (pamh, LOG_WARNING,
                    "The log_passwd option was not available at compile time.");
#warning "pam_tty_audit: The log_passwd option is not available.  Please upgrade your headers/kernel."
#endif /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */
      else
	{
	  pam_syslog (pamh, LOG_ERR, "unknown option `%s'", argv[i]);
	}
    }
  if (command == CMD_NONE)
    return PAM_SUCCESS;

  old_status = malloc (sizeof (*old_status));
  if (old_status == NULL)
    return PAM_SESSION_ERR;

  fd = nl_open ();
  if (fd == -1
      && errno == EPROTONOSUPPORT)
    {
      pam_syslog (pamh, LOG_WARNING, "unable to open audit socket, audit not "
                  "supported; tty_audit skipped");
      free (old_status);
      return PAM_IGNORE;
    }
  else if (fd == -1
      || nl_send (fd, AUDIT_TTY_GET, 0, NULL, 0) != 0
      || nl_recv (fd, AUDIT_TTY_GET, old_status, sizeof (*old_status)) != 0)
    {
      pam_syslog (pamh, LOG_ERR, "error reading current audit status: %m");
      if (fd != -1)
	close (fd);
      free (old_status);
      return PAM_SESSION_ERR;
    }

  memcpy(&new_status, old_status, sizeof(new_status));

  new_status.enabled = (command == CMD_ENABLE ? 1 : 0);
#ifdef HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD
  new_status.log_passwd = log_passwd;
#endif /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */
  if (old_status->enabled == new_status.enabled
#ifdef HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD
      && old_status->log_passwd == new_status.log_passwd
#endif /* HAVE_STRUCT_AUDIT_TTY_STATUS_LOG_PASSWD */
     )
    {
      open_only = 1; /* to clean up old_status */
      goto ok_fd;
    }

  if (open_only == 0
      && pam_set_data (pamh, DATANAME, old_status, cleanup_old_status)
      != PAM_SUCCESS)
    {
      pam_syslog (pamh, LOG_ERR, "error saving old audit status");
      close (fd);
      free (old_status);
      return PAM_SESSION_ERR;
    }

  if (nl_send (fd, AUDIT_TTY_SET, NLM_F_ACK, &new_status,
	       sizeof (new_status)) != 0
      || nl_recv_ack (fd) != 0)
    {
      pam_syslog (pamh, LOG_ERR, "error setting current audit status: %m");
      close (fd);
      if (open_only != 0)
	free (old_status);
      return PAM_SESSION_ERR;
    }
  /* Fall through */
 ok_fd:
  close (fd);
  pam_syslog (pamh, LOG_DEBUG, "changed status from %d to %d",
	      old_status->enabled, new_status.enabled);
  if (open_only != 0)
    free (old_status);
  return PAM_SUCCESS;
}

int
pam_sm_close_session (pam_handle_t *pamh, int flags, int argc,
		      const char **argv)
{
  const void *status_;

  (void)flags;
  (void)argc;
  (void)argv;
  if (pam_get_data (pamh, DATANAME, &status_) == PAM_SUCCESS)
    {
      const struct audit_tty_status *status;
      int fd;

      status = status_;

      fd = nl_open ();
      if (fd == -1
	  || nl_send (fd, AUDIT_TTY_SET, NLM_F_ACK, status,
		      sizeof (*status)) != 0
	  || nl_recv_ack (fd) != 0)
	{
	  pam_syslog (pamh, LOG_ERR, "error restoring audit status: %m");
	  if (fd != -1)
	    close (fd);
	  return PAM_SESSION_ERR;
	}
      close (fd);
      pam_syslog (pamh, LOG_DEBUG, "restored status to %d", status->enabled);
    }
  return PAM_SUCCESS;
}
