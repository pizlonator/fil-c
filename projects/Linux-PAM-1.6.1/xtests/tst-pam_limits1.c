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
  test case:

  Check the following line in limits.conf:
  *               soft    nice     19
  *               hard    nice     -20

  getrlimit should return soft=1 and hard=40.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <security/pam_appl.h>

/* A conversation function which uses an internally-stored value for
   the responses. */
static int
fake_conv (int num_msg, const struct pam_message **msgm UNUSED,
	   struct pam_response **response, void *appdata_ptr UNUSED)
{
  struct pam_response *reply;
  int count;

  /* Sanity test. */
  if (num_msg <= 0)
    return PAM_CONV_ERR;

  /* Allocate memory for the responses. */
  reply = calloc (num_msg, sizeof (struct pam_response));
  if (reply == NULL)
    return PAM_CONV_ERR;

  /* Each prompt elicits the same response. */
  for (count = 0; count < num_msg; ++count)
    {
      reply[count].resp_retcode = 0;
      reply[count].resp = strdup ("!!");
    }

  /* Set the pointers in the response structure and return. */
  *response = reply;
  return PAM_SUCCESS;
}

static struct pam_conv conv = {
    fake_conv,
    NULL
};

int
main(int argc, char *argv[])
{
  pam_handle_t *pamh = NULL;
  const char *user="tstpamlimits";
  int retval;
  int debug = 0;

  if (argc > 1 && strcmp (argv[1], "-d") == 0)
    debug = 1;

#ifdef RLIMIT_NICE
  retval = pam_start("tst-pam_limits1", user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_limits1: pam_start returned %d\n", retval);
      return 1;
    }

  retval = pam_set_item (pamh, PAM_TTY, "/dev/tty1");
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr,
		 "pam_limits1: pam_set_item(PAM_TTY) returned %d\n",
		 retval);
      return 1;
    }

  retval = pam_open_session (pamh, 0);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_limits1: pam_open_session returned %d\n",
		 retval);
      return 1;
    }

  struct rlimit rlim;

  getrlimit (RLIMIT_NICE, &rlim);

  if (rlim.rlim_cur != 1 && rlim.rlim_max != 40)
    {
      if (debug)
	fprintf (stderr, "pam_limits1: getrlimit failed, soft=%u, hard=%u\n",
		 (unsigned int) rlim.rlim_cur, (unsigned int) rlim.rlim_max);
      return 1;
    }

  retval = pam_end (pamh,retval);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_limits1: pam_end returned %d\n", retval);
      return 1;
    }
  return 0;
#else
  if (debug)
    fprintf (stderr, "pam_limits1: RLIMIT_NICE does not exist)\n");

  return 77;
#endif
}
