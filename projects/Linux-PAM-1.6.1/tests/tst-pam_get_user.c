/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>

static const char *prompt = "myprompt:";
static const char *user = "itsme";

static int
login_conv (int num_msg, const struct pam_message **mesg,
           struct pam_response **resp, void *appdata_ptr UNUSED)
{
  struct pam_response *reply;
  int count;

  reply = calloc(num_msg, sizeof (struct pam_response));

  if (reply == NULL)
    return PAM_BUF_ERR;

  for (count = 0; count < num_msg; count++)
    {
      reply[count].resp_retcode = 0;
      reply[count].resp = NULL;

      switch (mesg[count]->msg_style)
	{
	case PAM_PROMPT_ECHO_ON:
	  if (strcmp (mesg[count]->msg, prompt) != 0)
	    {
	      fprintf (stderr, "conv function called with wrong prompt: %s\n",
		       mesg[count]->msg);
	      exit (1);
	    }
	  reply[count].resp = strdup (user);
	  break;

	default:
	  fprintf (stderr,
	     "pam_get_user calls conv function with unexpected msg style");
	  exit (1);
        }
    }

  *resp = reply;
  return PAM_SUCCESS;
}

int
main (void)
{
  const char *service = "dummy";
  const char *value;
  struct pam_conv conv = { &login_conv, NULL};
  pam_handle_t *pamh;
  int retval;

  /* 1: Call with NULL for every argument */
  retval = pam_get_user (NULL, NULL, NULL);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr,
               "tst-pam_get_user (NULL, NULL, NULL) returned PAM_SUCCESS\n");
      return 1;
    }

 /* setup pam handle */
  retval = pam_start (service, user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, &pamh) returned %d\n",
               service, user, retval);
      return 1;
    }

  /* 2: Call with valid pamh handle but NULL for user */
  retval = pam_get_user (pamh, NULL, NULL);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr,
               "tst-pam_get_user (pamh, NULL, NULL) returned PAM_SUCCESS\n");
      return 1;
    }

  /* 3: Call with valid pamh handle and valid user ptr */
  retval = pam_get_user (pamh, &value, NULL);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
               "tst-pam_get_user (pamh, &value, NULL) returned %d\n",
	       retval);
      return 1;
    }
  if (strcmp (user, value) != 0)
    {
      fprintf (stderr,
               "tst-pam_get_user (pamh, &value, NULL) mismatch:\n"
	       "expected: %s\n"
	       "got: %s\n", user, value);
      return 1;
    }

  pam_end (pamh, 0);

 /* setup pam handle without user */
  retval = pam_start (service, NULL, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, &pamh) returned %d\n",
               service, user, retval);
      return 1;
    }

  /* 4: Call with valid pamh handle and valid user ptr */
  retval = pam_get_user (pamh, &value, prompt);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
               "tst-pam_get_user (pamh, &value, prompt) returned %d\n",
	       retval);
      return 1;
    }
  if (strcmp (user, value) != 0)
    {
      fprintf (stderr,
               "tst-pam_get_user (pamh, &value, prompt) mismatch:\n"
	       "expected: %s\n"
	       "got: %s\n", user, value);
      return 1;
    }

  pam_end (pamh, 0);

  return 0;
}
