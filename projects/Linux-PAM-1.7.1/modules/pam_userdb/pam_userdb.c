/*
 * pam_userdb module
 *
 * Written by Cristian Gafton <gafton@redhat.com> 1996/09/10
 * See the end of the file for Copyright Information
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "pam_userdb.h"

#ifdef HAVE_NDBM_H
# include <ndbm.h>
#elif defined(HAVE_GDBM_H)
# include <gdbm.h>
#else
# ifdef HAVE_DB_H
#  define DB_DBM_HSEARCH    1 /* use the dbm interface */
#  define HAVE_DBM	      /* for BerkDB 5.0 and later */
#  include <db.h>
# else
#  error "failed to find a libdb or equivalent"
# endif
#endif

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/_pam_macros.h>
#include "pam_inline.h"
#include "pam_i18n.h"

#ifndef HAVE_GDBM_H
# define COND_UNUSED UNUSED
#else
# define COND_UNUSED
#endif /* HAVE_GDBM_H */

/*
 * Conversation function to obtain the user's password
 */
static int
obtain_authtok(pam_handle_t *pamh)
{
    char *resp;
    const void *item;
    int retval;

    retval = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &resp, _("Password: "));

    if (retval != PAM_SUCCESS)
	return retval;

    if (resp == NULL)
	return PAM_CONV_ERR;

    /* set the auth token */
    retval = pam_set_item(pamh, PAM_AUTHTOK, resp);

    /* clean it up */
    pam_overwrite_string(resp);
    _pam_drop(resp);

    if ( (retval != PAM_SUCCESS) ||
	 (retval = pam_get_item(pamh, PAM_AUTHTOK, &item))
	 != PAM_SUCCESS ) {
	return retval;
    }

    return retval;
}

static int
_pam_parse (pam_handle_t *pamh, int argc, const char **argv,
	    const char **database, const char **cryptmode)
{
  int ctrl;

  *database = NULL;
  *cryptmode = NULL;

  /* step through arguments */
  for (ctrl = 0; argc-- > 0; ++argv)
    {
      const char *str;

      /* generic options */

      if (!strcmp(*argv,"debug"))
	ctrl |= PAM_DEBUG_ARG;
      else if (!strcasecmp(*argv, "icase"))
	ctrl |= PAM_ICASE_ARG;
      else if (!strcasecmp(*argv, "dump"))
	ctrl |= PAM_DUMP_ARG;
      else if (!strcasecmp(*argv, "unknown_ok"))
	ctrl |= PAM_UNKNOWN_OK_ARG;
      else if (!strcasecmp(*argv, "key_only"))
	ctrl |= PAM_KEY_ONLY_ARG;
      else if (!strcasecmp(*argv, "use_first_pass"))
	ctrl |= PAM_USE_FPASS_ARG;
      else if (!strcasecmp(*argv, "try_first_pass"))
	ctrl |= PAM_TRY_FPASS_ARG;
      else if ((str = pam_str_skip_icase_prefix(*argv, "db=")) != NULL)
	{
	  *database = str;
	  if (**database == '\0') {
	    *database = NULL;
	    pam_syslog(pamh, LOG_ERR,
		       "db= specification missing argument - ignored");
	  }
	}
      else if ((str = pam_str_skip_icase_prefix(*argv, "crypt=")) != NULL)
	{
	  *cryptmode = str;
	  if (**cryptmode == '\0')
	    pam_syslog(pamh, LOG_ERR,
		       "crypt= specification missing argument - ignored");
	}
      else
	{
	  pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

  return ctrl;
}

/*
 * Database abstraction functions
 */
static void *
db_open(const char *database, mode_t file_mode)
{
#ifdef HAVE_GDBM_H
    return gdbm_open(database, 4096, GDBM_READER, file_mode, NULL);
#else
    return dbm_open(database, O_RDONLY, file_mode);
#endif /* HAVE_GDBM_H */
}

static datum
db_firstkey(void *dbm)
{
#ifdef HAVE_GDBM_H
    return gdbm_firstkey(dbm);
#else
    return dbm_firstkey(dbm);
#endif /* HAVE_GDBM_H */
}

static datum
db_nextkey(void *dbm, datum key COND_UNUSED)
{
#ifdef HAVE_GDBM_H
    return gdbm_nextkey(dbm, key);
#else
    return dbm_nextkey(dbm);
#endif /* HAVE_GDBM_H */
}

static datum
db_fetch(void *dbm, datum key)
{
#ifdef HAVE_GDBM_H
    return gdbm_fetch(dbm, key);
#else
    return dbm_fetch(dbm, key);
#endif /* HAVE_GDBM_H */
}

static int
db_close(void *dbm)
{
#ifdef HAVE_GDBM_H
# ifdef GDBM_CLOSE_RETURNS_INT
    return gdbm_close(dbm);
# else
    gdbm_close(dbm);
    return 0;
# endif
#else
    dbm_close(dbm);
    return 0;
#endif /* HAVE_GDBM_H */
}


/*
 * Looks up a user name in a database and checks the password
 *
 * return values:
 *	 1  = User not found
 *	 0  = OK
 *	-1  = Password incorrect
 *	-2  = System error
 */
static int
user_lookup (pam_handle_t *pamh, const char *database, const char *cryptmode,
	     const char *user, const char *pass, int ctrl)
{
#ifdef HAVE_GDBM_H
    GDBM_FILE *dbm;
#else
    DBM *dbm;
#endif
    datum key, data;

    /* Open the DB file. */
    dbm = db_open(database, 0644);
    if (dbm == NULL) {
	pam_syslog(pamh, LOG_ERR,
		   "user_lookup: could not open database `%s': %m", database);
	return -2;
    }

    /* dump out the database contents for debugging */
    if (ctrl & PAM_DUMP_ARG) {
	pam_syslog(pamh, LOG_INFO, "Database dump:");
	for (key = db_firstkey(dbm);  key.dptr != NULL;
	     key = db_nextkey(dbm, key)) {
	     data = db_fetch(dbm, key);
	    pam_syslog(pamh, LOG_INFO,
		       "key[len=%d] = `%s', data[len=%d] = `%s'",
		       key.dsize, key.dptr, data.dsize, data.dptr);
	}
    }

    /* do some more init work */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (ctrl & PAM_KEY_ONLY_ARG) {
	if ((key.dptr = pam_asprintf("%s-%s", user, pass)) != NULL)
	    key.dsize = strlen(key.dptr);
    } else {
        key.dptr = strdup(user);
        key.dsize = strlen(user);
    }

    if (key.dptr) {
	data = db_fetch(dbm, key);
	pam_overwrite_n(key.dptr, key.dsize);
	free(key.dptr);
    }

    if (ctrl & PAM_DEBUG_ARG) {
	pam_syslog(pamh, LOG_INFO,
		   "password in database is [%p]`%.*s', len is %d",
		   data.dptr, data.dsize, (char *) data.dptr, data.dsize);
    }

    if (data.dptr != NULL) {
	int compare = -2;

	if (ctrl & PAM_KEY_ONLY_ARG)
	  {
	    db_close (dbm);
	    return 0; /* found it, data contents don't matter */
	}

	if (cryptmode && pam_str_skip_icase_prefix(cryptmode, "crypt") != NULL) {
	  if (data.dsize < 13) {
	    /* hash is too short */
	    pam_syslog(pamh, LOG_INFO, "password hash in database is too short");
	  } else if (ctrl & PAM_ICASE_ARG) {
	    pam_syslog(pamh, LOG_INFO,
	       "case-insensitive comparison only works with plaintext passwords");
	  } else {
	    /* libdb is not guaranteed to produce null terminated strings */
	    char *pwhash = strndup(data.dptr, data.dsize);

	    if (pwhash == NULL) {
	      pam_syslog(pamh, LOG_CRIT, "strndup failed: data.dptr");
	    } else {
	      char *cryptpw = NULL;
#ifdef HAVE_CRYPT_R
	      struct crypt_data *cdata = NULL;
	      cdata = calloc(1, sizeof(*cdata));
	      if (cdata == NULL) {
	        pam_syslog(pamh, LOG_CRIT, "malloc failed: struct crypt_data");
	      } else {
	        cryptpw = crypt_r(pass, pwhash, cdata);
	      }
#else
	      cryptpw = crypt (pass, pwhash);
#endif
	      if (cryptpw && strlen(cryptpw) == (size_t)data.dsize) {
	        compare = memcmp(data.dptr, cryptpw, data.dsize);
	      } else {
	        if (ctrl & PAM_DEBUG_ARG) {
	          if (cryptpw) {
	            pam_syslog(pamh, LOG_INFO, "lengths of computed and stored hashes differ");
	            pam_syslog(pamh, LOG_INFO, "computed hash: %s", cryptpw);
	          } else {
	            pam_syslog(pamh, LOG_ERR, "crypt() returned NULL");
	          }
	        }
	      }
#ifdef HAVE_CRYPT_R
	      pam_overwrite_object(cdata);
	      free(cdata);
#else
	      pam_overwrite_string(cryptpw);
#endif
	    }
	    pam_overwrite_string(pwhash);
	    free(pwhash);
	  }
	} else {

	  /* Unknown password encryption method -
	   * default to plaintext password storage
	   */

	  if (strlen(pass) != (size_t)data.dsize) {
	    compare = 1; /* wrong password len -> wrong password */
	  } else if (ctrl & PAM_ICASE_ARG) {
	    compare = strncasecmp(data.dptr, pass, data.dsize);
	  } else {
	    compare = strncmp(data.dptr, pass, data.dsize);
	  }

	  if (cryptmode && pam_str_skip_icase_prefix(cryptmode, "none") == NULL
		&& (ctrl & PAM_DEBUG_ARG)) {
	    pam_syslog(pamh, LOG_INFO, "invalid value for crypt parameter: %s",
		       cryptmode);
	    pam_syslog(pamh, LOG_INFO, "defaulting to plaintext password mode");
	  }

	}

	db_close(dbm);
	if (compare == 0)
	    return 0; /* match */
	else
	    return -1; /* wrong */
    } else {
        int saw_user = 0;

	if (ctrl & PAM_DEBUG_ARG) {
	    pam_syslog(pamh, LOG_INFO, "error returned by dbm_fetch: %m");
	}

	/* probably we should check dbm_error() here */

        if ((ctrl & PAM_KEY_ONLY_ARG) == 0) {
	    db_close(dbm);
            return 1; /* not key_only, so no entry => no entry for the user */
        }

        /* now handle the key_only case */
        for (key = db_firstkey(dbm);
             key.dptr != NULL;
             key = db_nextkey(dbm, key)) {
            int compare;
            /* first compare the user portion (case sensitive) */
            compare = strncmp(key.dptr, user, strlen(user));
            if (compare == 0) {
                /* assume failure */
                compare = -1;
                /* if we have the divider where we expect it to be... */
                if (key.dptr[strlen(user)] == '-') {
		    saw_user = 1;
		    if ((size_t)key.dsize == strlen(user) + 1 + strlen(pass)) {
		        if (ctrl & PAM_ICASE_ARG) {
			    /* compare the password portion (case insensitive)*/
                            compare = strncasecmp(key.dptr + strlen(user) + 1,
                                                  pass,
                                                  strlen(pass));
		        } else {
                            /* compare the password portion (case sensitive) */
                            compare = strncmp(key.dptr + strlen(user) + 1,
                                              pass,
                                              strlen(pass));
		        }
		    }
                }
                if (compare == 0) {
                    db_close(dbm);
                    return 0; /* match */
                }
            }
        }
        db_close(dbm);
	if (saw_user)
	    return -1; /* saw the user, but password mismatch */
	else
	    return 1; /* not found */
    }

    /* NOT REACHED */
    return -2;
}

/* --- authentication management functions (only) --- */

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
     const char *username;
     const void *password;
     const char *database = NULL;
     const char *cryptmode = NULL;
     int retval = PAM_AUTH_ERR, ctrl;

     /* parse arguments */
     ctrl = _pam_parse(pamh, argc, argv, &database, &cryptmode);
     if (database == NULL) {
        pam_syslog(pamh, LOG_ERR, "can not get the database name");
        return PAM_SERVICE_ERR;
     }

     /* Get the username */
     retval = pam_get_user(pamh, &username, NULL);
     if (retval != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
                   pam_strerror(pamh, retval));
        return PAM_SERVICE_ERR;
     }

     if ((ctrl & PAM_USE_FPASS_ARG) == 0 && (ctrl & PAM_TRY_FPASS_ARG) == 0) {
        /* Converse to obtain a password */
        retval = obtain_authtok(pamh);
        if (retval != PAM_SUCCESS) {
	    pam_syslog(pamh, LOG_ERR, "can not obtain password from user");
	    return retval;
        }
     }

     /* Check if we got a password */
     retval = pam_get_item(pamh, PAM_AUTHTOK, &password);
     if (retval != PAM_SUCCESS || password == NULL) {
        if ((ctrl & PAM_TRY_FPASS_ARG) != 0) {
	    /* Converse to obtain a password */
	    retval = obtain_authtok(pamh);
	    if (retval != PAM_SUCCESS) {
	        pam_syslog(pamh, LOG_ERR, "can not obtain password from user");
		return retval;
	    }
	    retval = pam_get_item(pamh, PAM_AUTHTOK, &password);
	}
	if (retval != PAM_SUCCESS || password == NULL) {
	    pam_syslog(pamh, LOG_ERR, "can not recover user password");
	    return PAM_AUTHTOK_RECOVERY_ERR;
	}
     }

     if (ctrl & PAM_DEBUG_ARG)
	 pam_syslog(pamh, LOG_INFO, "Verify user `%s' with a password",
		    username);

     /* Now use the username to look up password in the database file */
     retval = user_lookup(pamh, database, cryptmode, username, password, ctrl);
     switch (retval) {
	 case -2:
	     /* some sort of system error. The log was already printed */
	     return PAM_SERVICE_ERR;
	 case -1:
	     /* incorrect password */
	     pam_syslog(pamh, LOG_NOTICE,
			"user `%s' denied access (incorrect password)",
			username);
	     return PAM_AUTH_ERR;
	 case 1:
	     /* the user does not exist in the database */
	     if (ctrl & PAM_DEBUG_ARG)
		 pam_syslog(pamh, LOG_NOTICE,
			    "user `%s' not found in the database", username);
	     return PAM_USER_UNKNOWN;
	 case 0:
	     /* Otherwise, the authentication looked good */
	     pam_syslog(pamh, LOG_NOTICE, "user '%s' granted access", username);
	     return PAM_SUCCESS;
	 default:
	     /* we don't know anything about this return value */
	     pam_syslog(pamh, LOG_ERR,
		      "internal module error (retval = %d, user = `%s'",
		      retval, username);
	     return PAM_SERVICE_ERR;
     }

     /* should not be reached */
     return PAM_IGNORE;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
    const char *username;
    const char *database = NULL;
    const char *cryptmode = NULL;
    int retval = PAM_AUTH_ERR, ctrl;

    /* parse arguments */
    ctrl = _pam_parse(pamh, argc, argv, &database, &cryptmode);

    /* Get the username */
    retval = pam_get_user(pamh, &username, NULL);
    if (retval != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
                   pam_strerror(pamh, retval));
        return PAM_SERVICE_ERR;
    }

    /* Now use the username to look up password in the database file */
    retval = user_lookup(pamh, database, cryptmode, username, "", ctrl);
    switch (retval) {
        case -2:
	    /* some sort of system error. The log was already printed */
	    return PAM_SERVICE_ERR;
	case -1:
	    /* incorrect password, but we don't care */
	    /* FALL THROUGH */
	case 0:
	    /* authentication succeeded. dumbest password ever. */
	    return PAM_SUCCESS;
	case 1:
	    /* the user does not exist in the database */
	    return PAM_USER_UNKNOWN;
        default:
	    /* we don't know anything about this return value */
	    pam_syslog(pamh, LOG_ERR,
		       "internal module error (retval = %d, user = `%s'",
		       retval, username);
            return PAM_SERVICE_ERR;
    }

    return PAM_SUCCESS;
}

/*
 * Copyright (c) Cristian Gafton <gafton@redhat.com>, 1999
 *                                              All rights reserved
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
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
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
