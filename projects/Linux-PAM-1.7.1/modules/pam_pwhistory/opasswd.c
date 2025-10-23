/*
 * Copyright (c) 2008 Thorsten Kukuk <kukuk@suse.de>
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <pwd.h>
#include <shadow.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#ifdef HELPER_COMPILE
#include <stdarg.h>
#endif
#include <sys/stat.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HELPER_COMPILE
#define pam_modutil_getpwnam(h,n) getpwnam(n)
#define pam_modutil_getspnam(h,n) getspnam(n)
#define pam_syslog(h,a,...) helper_log_err(a,__VA_ARGS__)
#else
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#endif
#include <security/pam_modules.h>
#include "pam_inline.h"

#include "opasswd.h"

#ifndef RANDOM_DEVICE
#define RANDOM_DEVICE "/dev/urandom"
#endif

#define DEFAULT_OLD_PASSWORDS_FILE SCONFIG_DIR "/opasswd"

typedef struct {
  char *user;
  char *uid;
  int count;
  char *old_passwords;
} opwd;

#ifdef HELPER_COMPILE
void
helper_log_err(int err, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  openlog(HELPER_COMPILE, LOG_CONS | LOG_PID, LOG_AUTHPRIV);
  vsyslog(err, format, args);
  va_end(args);
  closelog();
}
#endif

static int
parse_entry (char *line, opwd *data)
{
  const char delimiters[] = ":";
  char *endptr;
  char *count;

  data->user = strsep (&line, delimiters);
  data->uid = strsep (&line, delimiters);
  count = strsep (&line, delimiters);
  if (count == NULL)
      return 1;

  data->count = strtol (count, &endptr, 10);
  if (endptr != NULL && *endptr != '\0')
      return 1;

  data->old_passwords = strsep (&line, delimiters);

  return 0;
}

/* Return 1 if the passwords are equal, 0 if they are not, and -1 on error. */
static int
compare_password(const char *newpass, const char *oldpass)
{
  char *outval;
  int retval;
#ifdef HAVE_CRYPT_R
  struct crypt_data *cdata;

  cdata = calloc(1, sizeof(*cdata));
  if (!cdata)
    return -1;

  outval = crypt_r (newpass, oldpass, cdata);
#else
  outval = crypt (newpass, oldpass);
#endif

  retval = outval != NULL && strcmp(outval, oldpass) == 0;

#ifdef HAVE_CRYPT_R
  pam_overwrite_object(cdata);
  free(cdata);
#else
  pam_overwrite_string(outval);
#endif
  return retval;
}

/* Check, if the new password is already in the opasswd file.  */
PAMH_ARG_DECL(int
check_old_pass, const char *user, const char *newpass, const char *filename, int debug)
{
  int retval = PAM_SUCCESS;
  FILE *oldpf;
  char *buf = NULL;
  size_t buflen = 0;
  opwd entry;
  int found = 0;

#ifndef HELPER_COMPILE
  if (SELINUX_ENABLED)
    return PAM_PWHISTORY_RUN_HELPER;
#endif

  const char *opasswd_file =
	  (filename != NULL ? filename : DEFAULT_OLD_PASSWORDS_FILE);

  if ((oldpf = fopen (opasswd_file, "r")) == NULL)
    {
      if (errno != ENOENT)
	pam_syslog (pamh, LOG_ERR, "Cannot open %s: %m", opasswd_file);
      return PAM_SUCCESS;
    }

  while (!feof (oldpf))
    {
      ssize_t n = getline (&buf, &buflen, oldpf);

      if (n < 1)
        break;

      buf[strcspn(buf, "\n")] = '\0';
      if (buf[0] == '\0')        /* ignore empty lines */
        continue;

      if (strncmp (buf, user, strlen (user)) == 0 &&
          buf[strlen (user)] == ':')
        {
          /* We found the line we needed */
	  if (parse_entry (buf, &entry) == 0)
	    {
	      found = 1;
	      break;
	    }
	}
    }

  fclose (oldpf);

  if (found && entry.old_passwords)
    {
      const char delimiters[] = ",";
      char *running;
      char *oldpass;

      running = entry.old_passwords;

      do {
	oldpass = strsep (&running, delimiters);
	if (oldpass && strlen (oldpass) > 0) {
	    int rc;

	    rc = compare_password(newpass, oldpass);
	    if (rc) {
	      if (rc < 0)
	        pam_syslog (pamh, LOG_ERR, "Cannot allocate crypt data");
	      else if (debug)
	        pam_syslog (pamh, LOG_DEBUG, "New password already used");

	      retval = PAM_AUTHTOK_ERR;
	      break;
	    }
	  }
      } while (oldpass != NULL);
    }

  pam_overwrite_n(buf, buflen);
  free (buf);

  return retval;
}

PAMH_ARG_DECL(int
save_old_pass, const char *user, int howmany, const char *filename, int debug UNUSED)
{
  struct stat opasswd_stat;
  FILE *oldpf, *newpf;
  int newpf_fd;
  int do_create = 0;
  int retval = PAM_SUCCESS;
  char *buf = NULL;
  size_t buflen = 0;
  int found = 0;
  struct passwd *pwd;
  const char *oldpass;

  /* Define opasswd file and temp file for opasswd */
  const char *opasswd_file =
	  (filename != NULL ? filename : DEFAULT_OLD_PASSWORDS_FILE);
  char *opasswd_tmp = pam_asprintf("%s.tmpXXXXXX", opasswd_file);

  if (opasswd_tmp == NULL)
    return PAM_BUF_ERR;

  pwd = pam_modutil_getpwnam (pamh, user);
  if (pwd == NULL)
    {
      free (opasswd_tmp);
      return PAM_USER_UNKNOWN;
    }

  if (howmany <= 0)
    {
      free (opasswd_tmp);
      return PAM_SUCCESS;
    }

#ifndef HELPER_COMPILE
  if (SELINUX_ENABLED)
    {
      free (opasswd_tmp);
      return PAM_PWHISTORY_RUN_HELPER;
    }
#endif

  if ((strcmp(pwd->pw_passwd, "x") == 0)  ||
      ((pwd->pw_passwd[0] == '#') &&
       (pwd->pw_passwd[1] == '#') &&
       (strcmp(pwd->pw_name, pwd->pw_passwd + 2) == 0)))
    {
      struct spwd *spw = pam_modutil_getspnam (pamh, user);

      if (spw == NULL)
	{
	  free (opasswd_tmp);
	  return PAM_USER_UNKNOWN;
	}
      oldpass = spw->sp_pwdp;
    }
  else
      oldpass = pwd->pw_passwd;

  if (oldpass == NULL || *oldpass == '\0')
    {
      free (opasswd_tmp);
      return PAM_SUCCESS;
    }

  if ((oldpf = fopen (opasswd_file, "r")) == NULL)
    {
      if (errno == ENOENT)
	{
	  pam_syslog (pamh, LOG_NOTICE, "Creating %s", opasswd_file);
	  do_create = 1;
	}
      else
	{
	  pam_syslog (pamh, LOG_ERR, "Cannot open %s: %m", opasswd_file);
	  free (opasswd_tmp);
	  return PAM_AUTHTOK_ERR;
	}
    }
  else if (fstat (fileno (oldpf), &opasswd_stat) < 0)
    {
      pam_syslog (pamh, LOG_ERR, "Cannot stat %s: %m", opasswd_file);
      fclose (oldpf);
      free (opasswd_tmp);
      return PAM_AUTHTOK_ERR;
    }

  /* Open a temp passwd file */
  newpf_fd = mkstemp (opasswd_tmp);
  if (newpf_fd == -1)
    {
      pam_syslog (pamh, LOG_ERR, "Cannot create %s temp file: %m",
		  opasswd_file);
      if (oldpf)
	fclose (oldpf);
      free (opasswd_tmp);
      return PAM_AUTHTOK_ERR;
    }
  if (do_create)
    {
      if (fchmod (newpf_fd, S_IRUSR|S_IWUSR) != 0)
	pam_syslog (pamh, LOG_ERR,
		    "Cannot set permissions of %s temp file: %m", opasswd_file);
      if (fchown (newpf_fd, 0, 0) != 0)
	pam_syslog (pamh, LOG_ERR,
		    "Cannot set owner/group of %s temp file: %m", opasswd_file);
    }
  else
    {
      if (fchmod (newpf_fd, opasswd_stat.st_mode) != 0)
	pam_syslog (pamh, LOG_ERR,
		    "Cannot set permissions of %s temp file: %m", opasswd_file);
      if (fchown (newpf_fd, opasswd_stat.st_uid, opasswd_stat.st_gid) != 0)
	pam_syslog (pamh, LOG_ERR,
		    "Cannot set owner/group of %s temp file: %m", opasswd_file);
    }
  newpf = fdopen (newpf_fd, "w+");
  if (newpf == NULL)
    {
      pam_syslog (pamh, LOG_ERR, "Cannot fdopen %s: %m", opasswd_tmp);
      if (oldpf)
	fclose (oldpf);
      close (newpf_fd);
      retval = PAM_AUTHTOK_ERR;
      goto error_opasswd;
    }

  if (!do_create)
    while (!feof (oldpf))
      {
	char *save;
	ssize_t n = getline (&buf, &buflen, oldpf);

	if (n < 1)
	  break;

	save = strdup (buf); /* Copy to write the original data back.  */
	if (save == NULL)
          {
	    fclose (oldpf);
	    fclose (newpf);
	    retval = PAM_BUF_ERR;
	    goto error_opasswd;
          }

	buf[strcspn(buf, "\n")] = '\0';
	if (buf[0] == '\0')        /* ignore empty lines */
	  goto write_old_data;

	if (strncmp (buf, user, strlen (user)) == 0 &&
	    buf[strlen (user)] == ':')
	  {
	    /* We found the line we needed */
	    opwd entry;

	    if (parse_entry (buf, &entry) == 0)
	      {
		char *out;

		found = 1;

		/* Don't save the current password twice */
		if (entry.old_passwords && entry.old_passwords[0] != '\0')
		  {
		    char *cp = entry.old_passwords;
		    char *last = cp;

		    entry.count = 1;  /* Don't believe the count */
		    while ((cp = strchr (cp, ',')) != NULL)
		      {
			entry.count++;
			last = ++cp;
		      }

		    /* compare the last password */
		    if (strcmp (last, oldpass) == 0)
		      goto write_old_data;
		  }
		else
		  entry.count = 0;

		/* increase count.  */
		entry.count++;

		/* check that we don't remember too many passwords.  */
		while (entry.count > howmany && entry.count > 1)
		  {
		    char *p = strpbrk (entry.old_passwords, ",");
		    if (p != NULL)
		      entry.old_passwords = ++p;
		    entry.count--;
		  }

		if (entry.count == 1)
		  out = pam_asprintf("%s:%s:%d:%s\n",
				     entry.user, entry.uid, entry.count, oldpass);
		else
		  out = pam_asprintf("%s:%s:%d:%s,%s\n",
				     entry.user, entry.uid, entry.count,
				     entry.old_passwords, oldpass);
		if (out == NULL)
		  {
		    free (save);
		    retval = PAM_AUTHTOK_ERR;
		    fclose (oldpf);
		    fclose (newpf);
		    goto error_opasswd;
		  }

		if (fputs (out, newpf) < 0)
		  {
		    free (out);
		    free (save);
		    retval = PAM_AUTHTOK_ERR;
		    fclose (oldpf);
		    fclose (newpf);
		    goto error_opasswd;
		  }
		free (out);
	      }
	  }
	else
	  {
	  write_old_data:
	    if (fputs (save, newpf) < 0)
	      {
		free (save);
		retval = PAM_AUTHTOK_ERR;
		fclose (oldpf);
		fclose (newpf);
		goto error_opasswd;
	      }
	  }
	free (save);
      }

  if (!found)
    {
      char *out;

      if ((out = pam_asprintf("%s:%d:1:%s\n", user, pwd->pw_uid, oldpass)) == NULL)
	{
	  retval = PAM_AUTHTOK_ERR;
	  if (oldpf)
	    fclose (oldpf);
	  fclose (newpf);
	  goto error_opasswd;
	}
      if (fputs (out, newpf) < 0)
	{
	  pam_overwrite_string(out);
	  free (out);
	  retval = PAM_AUTHTOK_ERR;
	  if (oldpf)
	    fclose (oldpf);
	  fclose (newpf);
	  goto error_opasswd;
	}
      pam_overwrite_string(out);
      free (out);
    }

  if (oldpf)
    if (fclose (oldpf) != 0)
      {
	pam_syslog (pamh, LOG_ERR, "Error while closing old opasswd file: %m");
	retval = PAM_AUTHTOK_ERR;
	fclose (newpf);
	goto error_opasswd;
      }

  if (fflush (newpf) != 0 || fsync (fileno (newpf)) != 0)
    {
      pam_syslog (pamh, LOG_ERR,
		  "Error while syncing temporary opasswd file: %m");
      retval = PAM_AUTHTOK_ERR;
      fclose (newpf);
      goto error_opasswd;
    }

  if (fclose (newpf) != 0)
    {
      pam_syslog (pamh, LOG_ERR,
		  "Error while closing temporary opasswd file: %m");
      retval = PAM_AUTHTOK_ERR;
      goto error_opasswd;
    }

  char *opasswd_backup = pam_asprintf("%s.old", opasswd_file);
  if (opasswd_backup == NULL)
    {
      retval = PAM_BUF_ERR;
      goto error_opasswd;
    }

  unlink (opasswd_backup);
  if (link (opasswd_file, opasswd_backup) != 0 &&
      errno != ENOENT)
    pam_syslog (pamh, LOG_ERR, "Cannot create backup file of %s: %m",
		opasswd_file);
  rename (opasswd_tmp, opasswd_file);
  free (opasswd_backup);
 error_opasswd:
  unlink (opasswd_tmp);
  free (opasswd_tmp);
  pam_overwrite_n(buf, buflen);
  free (buf);

  return retval;
}
