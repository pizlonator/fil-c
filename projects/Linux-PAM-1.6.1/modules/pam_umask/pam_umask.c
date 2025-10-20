/*
 * pam_umask module
 *
 * Copyright (c) 2005, 2006, 2007, 2010, 2013 Thorsten Kukuk <kukuk@thkukuk.de>
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
 * the GNU Public License V2, in which case the provisions of the GPL
 * are required INSTEAD OF the above restrictions.  (This clause is
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

#include "config.h"

#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

#define LOGIN_DEFS "/etc/login.defs"
#define LOGIN_CONF "/etc/default/login"

struct options_t {
  int debug;
  int usergroups;
  int silent;
  const char *umask;
  char *login_umask;
};
typedef struct options_t options_t;

static void
parse_option (const pam_handle_t *pamh, const char *argv, options_t *options)
{
  const char *str;

  if (argv == NULL || argv[0] == '\0')
    return;

  if (strcasecmp (argv, "debug") == 0)
    options->debug = 1;
  else if ((str = pam_str_skip_icase_prefix (argv, "umask=")) != NULL)
    options->umask = str;
  else if (strcasecmp (argv, "usergroups") == 0)
    options->usergroups = 1;
  else if (strcasecmp (argv, "nousergroups") == 0)
    options->usergroups = 0;
  else if (strcasecmp (argv, "silent") == 0)
    options->silent = 1;
  else
    pam_syslog (pamh, LOG_ERR, "Unknown option: `%s'", argv);
}

static int
get_options (pam_handle_t *pamh, options_t *options,
	     int argc, const char **argv)
{
  memset (options, 0, sizeof (options_t));

  options->usergroups = DEFAULT_USERGROUPS_SETTING;

  /* Parse parameters for module */
  for ( ; argc-- > 0; argv++)
    parse_option (pamh, *argv, options);

  if (options->umask == NULL) {
    options->login_umask = pam_modutil_search_key (pamh, LOGIN_DEFS, "UMASK");
    if (options->login_umask == NULL)
      options->login_umask = pam_modutil_search_key (pamh, LOGIN_CONF, "UMASK");
    options->umask = options->login_umask;
  }

  return 0;
}

static void
set_umask (const char *value)
{
  const char *value_orig = value;
  mode_t mask;
  char *endptr;

  mask = strtoul (value, &endptr, 8) & 0777;
  if (((mask == 0) && (value_orig == endptr)) ||
      ((mask == UINT_MAX) && (errno == ERANGE)))
    return;
  umask (mask);
}

/* Set the process nice, ulimit, and umask from the
   password file entry.  */
static void
setup_limits_from_gecos (pam_handle_t *pamh, options_t *options,
			 struct passwd *pw)
{
  char *cp;

  if (options->usergroups)
    {
      /* if not root and username is the same as primary group name,
         set umask group bits to be the same as owner bits
	 (examples: 022 -> 002, 077 -> 007).  */
      if (pw->pw_uid != 0)
	{
	  struct group *grp = pam_modutil_getgrgid (pamh, pw->pw_gid);
	  if (grp && (strcmp (pw->pw_name, grp->gr_name) == 0))
	    {
	      mode_t oldmask = umask (0777);
	      umask ((oldmask & ~070) | ((oldmask >> 3) & 070));
	    }
        }
    }

  /* See if the GECOS field contains values for NICE, UMASK or ULIMIT.  */
  for (cp = pw->pw_gecos; cp != NULL; cp = strchr (cp, ','))
    {
      const char *str;

      if (*cp == ',')
	cp++;

      if ((str = pam_str_skip_icase_prefix (cp, "umask=")) != NULL)
	umask (strtol (str, NULL, 8) & 0777);
      else if ((str = pam_str_skip_icase_prefix (cp, "pri=")) != NULL)
	{
	  errno = 0;
	  if (nice (strtol (str, NULL, 10)) == -1 && errno != 0)
	    {
	      if (!options->silent || options->debug)
		pam_error (pamh, "nice failed: %m\n");
	      pam_syslog (pamh, LOG_ERR, "nice failed: %m");
	    }
	}
      else if ((str = pam_str_skip_icase_prefix (cp, "ulimit=")) != NULL)
	{
	  struct rlimit rlimit_fsize;
	  rlimit_fsize.rlim_cur = 512L * strtol (str, NULL, 10);
	  rlimit_fsize.rlim_max = rlimit_fsize.rlim_cur;
	  if (setrlimit (RLIMIT_FSIZE, &rlimit_fsize) == -1)
	    {
	      if (!options->silent || options->debug)
		pam_error (pamh, "setrlimit failed: %m\n");
	      pam_syslog (pamh, LOG_ERR, "setrlimit failed: %m");
	    }
        }
    }
}


int
pam_sm_open_session (pam_handle_t *pamh, int flags UNUSED,
                     int argc, const char **argv)
{
  struct passwd *pw;
  options_t options;
  const char *name;
  int retval = PAM_SUCCESS;

  get_options (pamh, &options, argc, argv);
  if (flags & PAM_SILENT)
    options.silent = 1;

  /* get the user name. */
  if ((retval = pam_get_user (pamh, &name, NULL)) != PAM_SUCCESS)
    {
      pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
		 pam_strerror(pamh, retval));
      return (retval == PAM_CONV_AGAIN ? PAM_INCOMPLETE:retval);
    }

  pw = pam_modutil_getpwnam (pamh, name);
  if (pw == NULL)
    {
      pam_syslog (pamh, LOG_NOTICE, "account for %s not found", name);
      return PAM_USER_UNKNOWN;
    }

  if (options.umask != NULL)
    {
      set_umask (options.umask);
      free (options.login_umask);
      options.umask = options.login_umask = NULL;
    }

  setup_limits_from_gecos (pamh, &options, pw);

  return retval;
}

int
pam_sm_close_session (pam_handle_t *pamh UNUSED, int flags UNUSED,
		      int argc UNUSED, const char **argv UNUSED)
{
  return PAM_SUCCESS;
}

/* end of module definition */
