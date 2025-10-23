/* Andrew Morgan (morgan@kernel.org) -- an example application
 * that invokes a shell, based on blank.c */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

/* ------ some local (static) functions ------- */

static void bail_out(pam_handle_t *pamh,int really, int code, const char *fn)
{
     fprintf(stderr,"==> called %s()\n  got: `%s'\n", fn,
	     pam_strerror(pamh,code));
     if (really && code)
	  exit (1);
}

/* ------ some static data objects ------- */

static struct pam_conv conv = {
     misc_conv,
     NULL
};

/* ------- the application itself -------- */

int main(int argc, char **argv)
{
     pam_handle_t *pamh=NULL;
     const void *username=NULL;
     const char *service="xsh";
     int retcode;

     /* did the user call with a username as an argument ?
      * did they also */

     if (argc > 3) {
	  fprintf(stderr,"usage: %s [username [service-name]]\n",argv[0]);
     }
     if ((argc >= 2) && (argv[1][0] != '-')) {
	  username = argv[1];
     }
     if (argc == 3) {
	 service = argv[2];
     }

     /* initialize the Linux-PAM library */
     retcode = pam_start(service, username, &conv, &pamh);
     bail_out(pamh,1,retcode,"pam_start");

     /* fill in the RUSER and RHOST etc. fields */
     {
	 char buffer[100];
	 struct passwd *pw;
	 const char *tty;

	 pw = getpwuid(getuid());
	 if (pw != NULL) {
	     retcode = pam_set_item(pamh, PAM_RUSER, pw->pw_name);
	     bail_out(pamh,1,retcode,"pam_set_item(PAM_RUSER)");
	 }

	 retcode = gethostname(buffer, sizeof(buffer)-1);
	 if (retcode) {
	     perror("failed to look up hostname");
	     retcode = pam_end(pamh, PAM_ABORT);
	     bail_out(pamh,1,retcode,"pam_end");
	 }
	 retcode = pam_set_item(pamh, PAM_RHOST, buffer);
	 bail_out(pamh,1,retcode,"pam_set_item(PAM_RHOST)");

	 tty = ttyname(fileno(stdin));
	 if (tty) {
	     retcode = pam_set_item(pamh, PAM_TTY, tty);
	     bail_out(pamh,1,retcode,"pam_set_item(PAM_TTY)");
	 }
     }

     /* to avoid using goto we abuse a loop here */
     for (;;) {
	  /* authenticate the user --- `0' here, could have been PAM_SILENT
	   *	| PAM_DISALLOW_NULL_AUTHTOK */

	  retcode = pam_authenticate(pamh, 0);
	  bail_out(pamh,0,retcode,"pam_authenticate");

	  /* has the user proved themself valid? */
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: invalid request\n",argv[0]);
	       break;
	  }

	  /* the user is valid, but should they have access at this
	     time? */

	  retcode = pam_acct_mgmt(pamh, 0); /* `0' could be as above */
	  bail_out(pamh,0,retcode,"pam_acct_mgmt");

	  if (retcode == PAM_NEW_AUTHTOK_REQD) {
	       fprintf(stderr,"Application must request new password...\n");
	       retcode = pam_chauthtok(pamh,PAM_CHANGE_EXPIRED_AUTHTOK);
	       bail_out(pamh,0,retcode,"pam_chauthtok");
	  }

	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: invalid request\n",argv[0]);
	       break;
	  }

	  /* `0' could be as above */
	  retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	  bail_out(pamh,0,retcode,"pam_setcred");

	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem setting user credentials\n"
		       ,argv[0]);
	       break;
	  }

	  /* open a session for the user --- `0' could be PAM_SILENT */
	  retcode = pam_open_session(pamh,0);
	  bail_out(pamh,0,retcode,"pam_open_session");
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem opening a session\n",argv[0]);
	       break;
	  }

	  pam_get_item(pamh, PAM_USER, &username);
	  fprintf(stderr,
		  "The user [%s] has been authenticated and `logged in'\n",
		  (const char *)username);

	  /* this is always a really bad thing for security! */
	  retcode = system("/bin/sh");

	  /* close a session for the user --- `0' could be PAM_SILENT
	   * it is possible that this pam_close_call is in another program..
	   */

	  retcode = pam_close_session(pamh,0);
	  bail_out(pamh,0,retcode,"pam_close_session");
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem closing a session\n",argv[0]);
	       break;
	  }

	  /* `0' could be as above */
	  retcode = pam_setcred(pamh, PAM_DELETE_CRED);
	  bail_out(pamh,0,retcode,"pam_setcred");
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem deleting user credentials\n"
		       ,argv[0]);
	       break;
	  }

	  break;                      /* don't go on for ever! */
     }

     /* close the Linux-PAM library */
     retcode = pam_end(pamh, PAM_SUCCESS);
     pamh = NULL;
     bail_out(pamh,1,retcode,"pam_end");

     return (0);
}
