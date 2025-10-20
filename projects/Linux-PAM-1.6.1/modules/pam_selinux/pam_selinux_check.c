/******************************************************************************
 * A module for Linux-PAM that will set the default security context after login
 * via PAM.
 *
 * Copyright (c) 2003 Red Hat, Inc.
 * Written by Dan Walsh <dwalsh@redhat.com>
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
 *
 */

/************************************************************************
 *
 * All PAM code goes in this section.
 *
 ************************************************************************/

#include "config.h"

#include <errno.h>
#include <syslog.h>
#include <unistd.h>               /* for getuid(), exit(), getopt() */
#include <signal.h>
#include <sys/wait.h>		  /* for wait() */

#include <security/pam_appl.h>    /* for PAM functions */
#include <security/pam_misc.h>    /* for misc_conv PAM utility function */

#define SERVICE_NAME "pam_selinux_check"   /* the name of this program for PAM */
				  /* The file containing the context to run
				   * the scripts under.                     */
int authenticate_via_pam( const char *user ,   pam_handle_t **pamh);

/* authenticate_via_pam()
 *
 * in:     user
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           1     pam thinks that the user authenticated themselves properly
 *           0     otherwise
 *
 * this function uses pam to authenticate the user running this
 * program.  this is the only function in this program that makes pam
 * calls.
 *
 */

int authenticate_via_pam( const char *user ,   pam_handle_t **pamh) {

  struct pam_conv *conv;
  int result = 0;    /* our result, set to 0 (not authenticated) by default */

  /* this is a jump table of functions for pam to use when it wants to *
   * communicate with the user.  we'll be using misc_conv(), which is  *
   * provided for us via pam_misc.h.                                   */
  struct pam_conv pam_conversation = {
    misc_conv,
    NULL
  };
  conv = &pam_conversation;


  /* make `p_pam_handle' a valid pam handle so we can use it when *
   * calling pam functions.                                       */
  if( PAM_SUCCESS != pam_start( SERVICE_NAME,
				user,
				conv,
				pamh ) ) {
    fprintf( stderr, _("failed to initialize PAM\n") );
    exit( -1 );
  }

  if( PAM_SUCCESS != pam_set_item(*pamh, PAM_RUSER, user))
  {
      fprintf( stderr, _("failed to pam_set_item()\n") );
      exit( -1 );
  }

  /* Ask PAM to authenticate the user running this program */
  if( PAM_SUCCESS == pam_authenticate(*pamh,0) ) {
    if ( PAM_SUCCESS == pam_open_session(*pamh, 0) )
      result = 1;  /* user authenticated OK! */
  }
  return( result );

} /* authenticate_via_pam() */

int
main (int argc, char **argv)
{
  pam_handle_t *pamh;
  int childPid;

  if (argc < 1)
    exit (-1);

  if (!authenticate_via_pam(argv[1],&pamh))
    exit(-1);

  childPid = fork();
  if (childPid < 0) {
    /* error in fork() */
    fprintf(stderr, _("login: failure forking: %m"));
    pam_close_session(pamh, 0);
    /* We're done with PAM.  Free `pam_handle'. */
    pam_end( pamh, PAM_SUCCESS );
    exit(0);
  }
  if (childPid) {
    close(0); close(1); close(2);
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    while(wait(NULL) == -1 && errno == EINTR) /**/ ;
    openlog("login", LOG_ODELAY, LOG_AUTHPRIV);
    pam_close_session(pamh, 0);
    /* We're done with PAM.  Free `pam_handle'. */
    pam_end( pamh, PAM_SUCCESS );
    exit(0);
  }
  argv[0]=strdup ("/bin/sh");
  argv[1]=NULL;

  /* NOTE: The environment has not been sanitized. LD_PRELOAD and other fun
   * things could be set. */
  execv("/bin/sh",argv);
  fprintf(stderr,"Failure\n");
  return 0;
}
