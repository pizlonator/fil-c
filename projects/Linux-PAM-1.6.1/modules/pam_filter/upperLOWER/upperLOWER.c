/*
 * This is a sample filter program, for use with pam_filter (a module
 * provided with Linux-PAM). This filter simply transposes upper and
 * lower case letters, it is intended for demonstration purposes and
 * it serves no purpose other than to annoy the user...
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "pam_filter.h"
#include <security/pam_modutil.h>

/* ---------------------------------------------------------------- */

static void do_transpose(char *buffer,int len)
{
     int i;
     for (i=0; i<len; ++i) {
	  if (islower((unsigned char)buffer[i])) {
	       buffer[i] = toupper((unsigned char)buffer[i]);
	  } else {
	       buffer[i] = tolower((unsigned char)buffer[i]);
	  }
     }
}

extern char **environ;

int main(int argc, char **argv UNUSED)
{
     char buffer[BUFSIZ];
     fd_set readers;
     void (*before_user)(char *,int);
     void (*before_app)(char *,int);

     openlog("upperLOWER", LOG_CONS|LOG_PID, LOG_AUTHPRIV);

#ifdef DEBUG
     {
	  int i;

	  fprintf(stderr,"environment :[\r\n");
	  for (i=0; environ[i]; ++i) {
	       fprintf(stderr,"-> %s\r\n",environ[i]);
	  }
	  fprintf(stderr,"]: end\r\n");
     }
#endif

     if (argc != 1) {
#ifdef DEBUG
	  fprintf(stderr,"filter invoked as conventional executable\n");
#else
	  syslog(LOG_ERR, "filter invoked as conventional executable");
#endif
	  exit(1);
     }

     before_user = before_app = do_transpose;   /* assign filter functions */

     /* enter a loop that deals with the input and output of the
        user.. passing it to and from the application */

     FD_ZERO(&readers);                    /* initialize reading mask */

     for (;;) {

	  FD_SET(APPOUT_FILENO, &readers);              /* wake for output */
	  FD_SET(APPERR_FILENO, &readers);               /* wake for error */
	  FD_SET(STDIN_FILENO, &readers);                /* wake for input */

	  if ( select(APPTOP_FILE,&readers,NULL,NULL,NULL) < 0 ) {
#ifdef DEBUG
	       fprintf(stderr,"select failed\n");
#else
	       syslog(LOG_WARNING,"select failed");
#endif
	       break;
	  }

	  /* application errors */

	  if ( FD_ISSET(APPERR_FILENO,&readers) ) {
	       int got = read(APPERR_FILENO, buffer, BUFSIZ);
	       if (got <= 0) {
		    break;
	       } else {
		    /* translate to give to real terminal */
		    if (before_user != NULL)
			 before_user(buffer, got);
		    if (pam_modutil_write(STDERR_FILENO, buffer, got) != got ) {
			 syslog(LOG_WARNING,"couldn't write %d bytes?!",got);
			 break;
		    }
	       }
	  } else if ( FD_ISSET(APPOUT_FILENO,&readers) ) {    /* app output */
	       int got = read(APPOUT_FILENO, buffer, BUFSIZ);
	       if (got <= 0) {
		    break;
	       } else {
		    /* translate to give to real terminal */
		    if (before_user != NULL)
			 before_user(buffer, got);
		    if (pam_modutil_write(STDOUT_FILENO, buffer, got) != got ) {
			 syslog(LOG_WARNING,"couldn't write %d bytes!?",got);
			 break;
		    }
	       }
	  }

	  if ( FD_ISSET(STDIN_FILENO, &readers) ) {  /* user input */
	       int got = read(STDIN_FILENO, buffer, BUFSIZ);
	       if (got < 0) {
		    syslog(LOG_WARNING,"user input junked");
		    break;
	       } else if (got) {
		    /* translate to give to application */
		    if (before_app != NULL)
			 before_app(buffer, got);
		    if (pam_modutil_write(APPIN_FILENO, buffer, got) != got ) {
			 syslog(LOG_WARNING,"couldn't pass %d bytes!?",got);
			 break;
		    }
	       } else {
		    /* nothing received -- an error? */
		    syslog(LOG_WARNING,"user input null?");
		    break;
	       }
	  }
     }

     exit(0);
}
