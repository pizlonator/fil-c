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

/*
 * Check password change minimum days handling.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

/* A conversation function which uses an internally-stored value for
   the responses. */
static int
fake_conv (int num_msg, const struct pam_message **msgm UNUSED,
	   struct pam_response **response, void *appdata_ptr UNUSED)
{
  struct pam_response *reply;
  int count;
  static int respnum = 0;
  static const char *resps[] = { "pamunix01", "TsTPAM01MAP", "TsTPAM01MAP" };

  /* Sanity test. */
  if (num_msg <= 0)
    return PAM_CONV_ERR;

  /* Allocate memory for the responses. */
  reply = calloc (num_msg, sizeof (struct pam_response));
  if (reply == NULL)
    return PAM_CONV_ERR;

  /* Answer with appropriate response from the above array. */
  for (count = 0; count < num_msg; ++count)
    {
      if (msgm[count]->msg_style == PAM_PROMPT_ECHO_OFF)
        {
          reply[count].resp_retcode = 0;
          reply[count].resp = strdup (resps[respnum % 3]);
          ++respnum;
        }
    }

  /* Set the pointers in the response structure and return. */
  *response = reply;
  return PAM_SUCCESS;
}

static struct pam_conv conv = {
    fake_conv,
    NULL
};


/* Check that errors of optional modules are ignored and that
   required modules after a sufficient one are not executed.  */

int
main(int argc, char *argv[])
{
  pam_handle_t *pamh=NULL;
  const char *user="tstpamunix";
  int retval;
  int debug = 0;
  int fail;
  struct passwd *pwd;

  if (argc < 2 || (*argv[1] != 'f' &&
      *argv[1] != 'p'))
    {
      fprintf (stderr, "Need fail or pass argument.\n");
      return 2;
    }

  fail = *argv[1] == 'f';

  if (argc > 2 && strcmp (argv[2], "-d") == 0)
    debug = 1;

  pwd = getpwnam (user);

  if (pwd == NULL)
    {
       if (debug)
         fprintf (stderr, "unix4: Missing tstpamunix user.\n");
       return 2;
    }

  /* we must switch the real (not effective) user so the restrictions
     are enforced */
  setreuid (pwd->pw_uid, -1);

  retval = pam_start("tst-pam_unix4", user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "unix4: pam_start returned %d\n", retval);
      return 1;
    }

  retval = pam_chauthtok (pamh, 0);
  if ((!fail && retval != PAM_SUCCESS) || (fail && retval == PAM_SUCCESS))
    {
      if (debug)
	fprintf (stderr, "unix4-1: pam_chauthtok returned %d\n", retval);
      return 1;
    }

  retval = pam_end (pamh,retval);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "unix4: pam_end returned %d\n", retval);
      return 1;
    }
  return 0;
}
