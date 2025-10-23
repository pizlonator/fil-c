/*
 * pam_stress module
 *
 * created by Andrew Morgan <morgan@linux.kernel.org> 1996/3/12
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <syslog.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

/* ---------- */

/* an internal function to turn all possible test arguments into bits
   of a ctrl number */

/* generic options */

#define PAM_ST_DEBUG         01
#define PAM_ST_NO_WARN       02
#define PAM_ST_USE_PASS1     04
#define PAM_ST_TRY_PASS1    010
#define PAM_ST_ROOTOK       020

/* simulation options */

#define PAM_ST_EXPIRED       040
#define PAM_ST_FAIL_1       0100
#define PAM_ST_FAIL_2       0200
#define PAM_ST_PRELIM       0400
#define PAM_ST_REQUIRE_PWD 01000

/* some syslogging */

static void
_pam_report (const pam_handle_t *pamh, int ctrl, const char *name,
	     int flags, int argc, const char **argv)
{
     if (ctrl & PAM_ST_DEBUG) {
	  pam_syslog(pamh, LOG_DEBUG, "CALLED: %s", name);
	  pam_syslog(pamh, LOG_DEBUG, "FLAGS : 0%o%s",
		     flags, (flags & PAM_SILENT) ? " (silent)":"");
	  pam_syslog(pamh, LOG_DEBUG, "CTRL  = 0%o", ctrl);
	  pam_syslog(pamh, LOG_DEBUG, "ARGV  :");
	  while (argc--) {
	       pam_syslog(pamh, LOG_DEBUG, " \"%s\"", *argv++);
	  }
     }
}

static int
_pam_parse (const pam_handle_t *pamh, int argc, const char **argv)
{
     int ctrl=0;

     /* step through arguments */
     for (ctrl=0; argc-- > 0; ++argv) {

	  /* generic options */

	  if (!strcmp(*argv,"debug"))
	       ctrl |= PAM_ST_DEBUG;
	  else if (!strcmp(*argv,"no_warn"))
	       ctrl |= PAM_ST_NO_WARN;
	  else if (!strcmp(*argv,"use_first_pass"))
	       ctrl |= PAM_ST_USE_PASS1;
	  else if (!strcmp(*argv,"try_first_pass"))
	       ctrl |= PAM_ST_TRY_PASS1;
	  else if (!strcmp(*argv,"rootok"))
	       ctrl |= PAM_ST_ROOTOK;

	  /* simulation options */

	  else if (!strcmp(*argv,"expired"))   /* signal password needs
						  renewal */
	       ctrl |= PAM_ST_EXPIRED;
	  else if (!strcmp(*argv,"fail_1"))    /* instruct fn 1 to fail */
	       ctrl |= PAM_ST_FAIL_1;
	  else if (!strcmp(*argv,"fail_2"))    /* instruct fn 2 to fail */
	       ctrl |= PAM_ST_FAIL_2;
	  else if (!strcmp(*argv,"prelim"))    /* instruct pam_sm_setcred
						  to fail on first call */
	       ctrl |= PAM_ST_PRELIM;
	  else if (!strcmp(*argv,"required"))  /* module is fussy about the
						  user being authenticated */
	       ctrl |= PAM_ST_REQUIRE_PWD;

	  else {
	       pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	  }
     }

     return ctrl;
}

static int converse(pam_handle_t *pamh, int nargs
		    , const struct pam_message **message
		    , struct pam_response **response)
{
     int retval;
     const void *void_conv;
     const struct pam_conv *conv;

     retval = pam_get_item(pamh,PAM_CONV,&void_conv);
     conv = void_conv;
     if (retval == PAM_SUCCESS && conv) {
	  retval = conv->conv(nargs, message, response, conv->appdata_ptr);
	  if (retval != PAM_SUCCESS) {
	       pam_syslog(pamh, LOG_ERR, "converse returned %d: %s",
			retval, pam_strerror(pamh, retval));
	  }
     } else {
	  pam_syslog(pamh, LOG_ERR, "converse failed to get pam_conv");
         if (retval == PAM_SUCCESS)
             retval = PAM_BAD_ITEM; /* conv was null */
     }

     return retval;
}

/* authentication management functions */

static int stress_get_password(pam_handle_t *pamh, int flags
			       , int ctrl, char **password)
{
     const void *pam_pass;
     char *pass;

     if ( (ctrl & (PAM_ST_TRY_PASS1|PAM_ST_USE_PASS1))
	 && (pam_get_item(pamh,PAM_AUTHTOK,&pam_pass)
	     == PAM_SUCCESS)
	 && (pam_pass != NULL) ) {
	  if ((pass = strdup(pam_pass)) == NULL)
	       return PAM_BUF_ERR;
     } else if ((ctrl & PAM_ST_USE_PASS1)) {
	  pam_syslog(pamh, LOG_WARNING, "no forwarded password");
	  return PAM_PERM_DENIED;
     } else {                                /* we will have to get one */
	  struct pam_message msg[1];
	  const struct pam_message *pmsg[1];
	  struct pam_response *resp;
	  int retval;

	  /* set up conversation call */

	  pmsg[0] = &msg[0];
	  msg[0].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[0].msg = "STRESS Password: ";
	  resp = NULL;

	  if ((retval = converse(pamh,1,pmsg,&resp)) != PAM_SUCCESS) {
	       return retval;
	  }

	  if (resp) {
	       if ((resp[0].resp == NULL) && (ctrl & PAM_ST_DEBUG)) {
		    pam_syslog(pamh, LOG_DEBUG,
			       "pam_sm_authenticate: NULL authtok given");
	       }
	       if ((flags & PAM_DISALLOW_NULL_AUTHTOK)
		   && resp[0].resp == NULL) {
		    free(resp);
		    return PAM_AUTH_ERR;
	       }

	       pass = resp[0].resp;          /* remember this! */

	       resp[0].resp = NULL;
	  } else {
               if (ctrl & PAM_ST_DEBUG) {
	          pam_syslog(pamh, LOG_DEBUG,
			     "pam_sm_authenticate: no error reported");
	          pam_syslog(pamh, LOG_DEBUG,
			     "getting password, but NULL returned!?");
               }
	       return PAM_CONV_ERR;
	  }
	  free(resp);
     }

     *password = pass;             /* this *MUST* be free()'d by this module */

     return PAM_SUCCESS;
}

/* function to clean up data items */

static void
wipe_up (pam_handle_t *pamh UNUSED, void *data, int error UNUSED)
{
     free(data);
}

int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
     const char *username;
     int retval=PAM_SUCCESS;
     char *pass;
     int ctrl;

     D(("called."));

     ctrl = _pam_parse(pamh, argc, argv);
     _pam_report(pamh, ctrl, "pam_sm_authenticate", flags, argc, argv);

     /* try to get the username */

     retval = pam_get_user(pamh, &username, "username: ");
     if (retval != PAM_SUCCESS) {
	  pam_syslog(pamh, LOG_NOTICE,
		     "pam_sm_authenticate: cannot determine user name: %s",
		     pam_strerror(pamh, retval));
	  return retval;
     }
     else if (ctrl & PAM_ST_DEBUG) {
	  pam_syslog(pamh, LOG_DEBUG,
		     "pam_sm_authenticate: username = %s", username);
     }

     /* now get the password */

     retval = stress_get_password(pamh,flags,ctrl,&pass);
     if (retval != PAM_SUCCESS) {
	  pam_syslog(pamh, LOG_WARNING,
		     "pam_sm_authenticate: failed to get a password");
	  return retval;
     }

     /* try to set password item */

     retval = pam_set_item(pamh,PAM_AUTHTOK,pass);
     pam_overwrite_string(pass); /* clean up local copy of password */
     free(pass);
     pass = NULL;
     if (retval != PAM_SUCCESS) {
	  pam_syslog(pamh, LOG_WARNING,
		     "pam_sm_authenticate: failed to store new password");
	  return retval;
     }

     /* if we are debugging then we print the password */

     if (ctrl & PAM_ST_DEBUG) {
          const void *pam_pass;
	  (void) pam_get_item(pamh,PAM_AUTHTOK,&pam_pass);
	  pam_syslog(pamh, LOG_DEBUG,
		     "pam_st_authenticate: password entered is: [%s]",
		     (const char *)pam_pass);
     }

     /* if we signal a fail for this function then fail */

     if ((ctrl & PAM_ST_FAIL_1) && retval == PAM_SUCCESS)
	  return PAM_PERM_DENIED;

     return retval;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
     int ctrl = _pam_parse(pamh, argc, argv);

     D(("called. [post parsing]"));

     _pam_report(pamh, ctrl, "pam_sm_setcred", flags, argc, argv);

     if (ctrl & PAM_ST_FAIL_2)
	  return PAM_CRED_ERR;

     return PAM_SUCCESS;
}

/* account management functions */

int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
     int ctrl = _pam_parse(pamh, argc, argv);

     D(("called. [post parsing]"));

     _pam_report(pamh, ctrl,"pam_sm_acct_mgmt", flags, argc, argv);

     if (ctrl & PAM_ST_FAIL_1)
	  return PAM_PERM_DENIED;
     else if (ctrl & PAM_ST_EXPIRED) {
	  int retval;
	  void *text = strdup("yes");
	  if (!text)
	        return PAM_BUF_ERR;
	  retval = pam_set_data(pamh,"stress_new_pwd",text,wipe_up);
	  if (retval != PAM_SUCCESS) {
	        pam_syslog(pamh, LOG_DEBUG,
			   "pam_sm_acct_mgmt: failed setting stress_new_pwd");
	        free(text);
	        return retval;
	  }

	  if (ctrl & PAM_ST_DEBUG) {
	       pam_syslog(pamh, LOG_DEBUG,
			  "pam_sm_acct_mgmt: need a new password");
	  }
	  return PAM_NEW_AUTHTOK_REQD;
     }

     return PAM_SUCCESS;
}

int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
     const void *username, *service;
     int ctrl = _pam_parse(pamh, argc, argv);

     D(("called. [post parsing]"));

     _pam_report(pamh, ctrl,"pam_sm_open_session", flags, argc, argv);

     if ((pam_get_item(pamh, PAM_USER, &username)
	  != PAM_SUCCESS || !username)
	 || (pam_get_item(pamh, PAM_SERVICE, &service)
	     != PAM_SUCCESS || !service)) {
	  pam_syslog(pamh, LOG_WARNING, "pam_sm_open_session: for whom?");
	  return PAM_SESSION_ERR;
     }

     pam_syslog(pamh, LOG_NOTICE, "opened [%s] session for user [%s]",
	        (const char *)service, (const char *)username);

     if (ctrl & PAM_ST_FAIL_1)
	  return PAM_SESSION_ERR;

     return PAM_SUCCESS;
}

int pam_sm_close_session(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
     const void *username, *service;
     int ctrl = _pam_parse(pamh, argc, argv);

     D(("called. [post parsing]"));

     _pam_report(pamh, ctrl,"pam_sm_close_session", flags, argc, argv);

     if ((pam_get_item(pamh, PAM_USER, &username)
	  != PAM_SUCCESS || !username)
	 || (pam_get_item(pamh, PAM_SERVICE, &service)
	     != PAM_SUCCESS || !service)) {
	  pam_syslog(pamh, LOG_WARNING, "pam_sm_close_session: for whom?");
	  return PAM_SESSION_ERR;
     }

     pam_syslog(pamh, LOG_NOTICE, "closed [%s] session for user [%s]",
	      (const char *)service, (const char *)username);

     if (ctrl & PAM_ST_FAIL_2)
	  return PAM_SESSION_ERR;

     return PAM_SUCCESS;
}

int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
     int retval;
     int ctrl = _pam_parse(pamh, argc, argv);

     D(("called. [post parsing]"));

     _pam_report(pamh, ctrl,"pam_sm_chauthtok", flags, argc, argv);

     /* this function should be called twice by the Linux-PAM library */

     if (flags & PAM_PRELIM_CHECK) {           /* first call */
	  if (ctrl & PAM_ST_DEBUG) {
	       pam_syslog(pamh, LOG_DEBUG, "pam_sm_chauthtok: prelim check");
	  }
	  if (ctrl & PAM_ST_PRELIM)
	       return PAM_TRY_AGAIN;

	  return PAM_SUCCESS;
     } else if (flags & PAM_UPDATE_AUTHTOK) {  /* second call */
	  struct pam_message msg[3];
	  const struct pam_message *pmsg[3];
	  struct pam_response *resp;
	  const void *text;
	  char *txt=NULL;
	  int i;

	  if (ctrl & PAM_ST_DEBUG) {
	       pam_syslog(pamh, LOG_DEBUG, "pam_sm_chauthtok: alter password");
	  }

	  if (ctrl & PAM_ST_FAIL_1)
	       return PAM_AUTHTOK_LOCK_BUSY;

	  if ( !(ctrl & PAM_ST_EXPIRED)
	       && (flags & PAM_CHANGE_EXPIRED_AUTHTOK)
	       && (pam_get_data(pamh,"stress_new_pwd", &text)
		      != PAM_SUCCESS || strcmp(text,"yes"))) {
	       return PAM_SUCCESS;          /* the token has not expired */
	  }

	  /* the password should be changed */

	  if ((ctrl & PAM_ST_REQUIRE_PWD)
	      && !(getuid() == 0 && (ctrl & PAM_ST_ROOTOK))
	       ) {                       /* first get old one? */
	       char *pass;

	       if (ctrl & PAM_ST_DEBUG) {
		    pam_syslog(pamh, LOG_DEBUG,
			       "pam_sm_chauthtok: getting old password");
	       }
	       retval = stress_get_password(pamh,flags,ctrl,&pass);
	       if (retval != PAM_SUCCESS) {
		    pam_syslog(pamh, LOG_DEBUG,
			       "pam_sm_chauthtok: no password obtained");
		    return retval;
	       }
	       retval = pam_set_item(pamh, PAM_OLDAUTHTOK, pass);
	       pam_overwrite_string(pass);
	       free(pass);
	       pass = NULL;
	       if (retval != PAM_SUCCESS) {
		    pam_syslog(pamh, LOG_DEBUG,
			       "pam_sm_chauthtok: could not set OLDAUTHTOK");
		    return retval;
	       }
	  }

	  /* set up for conversation */

	  if (!(flags & PAM_SILENT)) {
	       const void *username;

	       if ( pam_get_item(pamh, PAM_USER, &username)
		    || username == NULL ) {
		    pam_syslog(pamh, LOG_ERR, "no username set");
		    return PAM_USER_UNKNOWN;
	       }
	       pmsg[0] = &msg[0];
	       msg[0].msg_style = PAM_TEXT_INFO;
	       if ((txt = pam_asprintf("Changing STRESS password for %s.",
				       (const char *)username)) == NULL) {
		    pam_syslog(pamh, LOG_CRIT, "out of memory");
		    return PAM_BUF_ERR;
	       }

	       msg[0].msg = txt;
	       i = 1;
	  } else {
	       i = 0;
	  }

	  pmsg[i] = &msg[i];
	  msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[i++].msg = "Enter new STRESS password: ";
	  pmsg[i] = &msg[i];
	  msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[i++].msg = "Retype new STRESS password: ";
	  resp = NULL;

	  retval = converse(pamh,i,pmsg,&resp);
	  free(txt);
	  txt = NULL;               /* clean up */
	  if (retval != PAM_SUCCESS) {
	       return retval;
	  }

	  if (resp == NULL) {
	       pam_syslog(pamh, LOG_ERR,
			  "pam_sm_chauthtok: no response from conv");
	       return PAM_CONV_ERR;
	  }

	  /* store the password */

	  if (resp[i-2].resp && resp[i-1].resp) {
	       if (strcmp(resp[i-2].resp,resp[i-1].resp)) {
		    /* passwords are not the same; forget and return error */

		    pam_drop_response(resp, i);

		    if (!(flags & PAM_SILENT) && !(ctrl & PAM_ST_NO_WARN)) {
			 pmsg[0] = &msg[0];
			 msg[0].msg_style = PAM_ERROR_MSG;
			 msg[0].msg = "Verification mis-typed; "
				      "password unchanged";
			 resp = NULL;
			 (void) converse(pamh,1,pmsg,&resp);
			 if (resp) {
			     pam_drop_response(resp, 1);
			 }
		    }
		    return PAM_AUTHTOK_ERR;
	       }

	       if (pam_get_item(pamh,PAM_AUTHTOK,&text)
		   == PAM_SUCCESS) {
		    (void) pam_set_item(pamh,PAM_OLDAUTHTOK,text);
		    text = NULL;
	       }
	       (void) pam_set_item(pamh,PAM_AUTHTOK,resp[0].resp);
	  } else {
	       pam_syslog(pamh, LOG_DEBUG,
			  "pam_sm_chauthtok: problem with resp");
	       retval = PAM_SYSTEM_ERR;
	  }

	  pam_drop_response(resp, i);      /* clean up the passwords */
     } else {
	  pam_syslog(pamh, LOG_ERR,
		     "pam_sm_chauthtok: this must be a Linux-PAM error");
	  return PAM_SYSTEM_ERR;
     }

     return retval;
}
