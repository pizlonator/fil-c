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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <security/pam_appl.h>


int
main (void)
{
  const char *service = "confdir";
  const char *xservice = "nonexistent-service";
  const char *user = "root";
  const char *confdir;
  const char *xconfdir = "/nonexistent-confdir";
  struct pam_conv conv = { NULL, NULL };
  pam_handle_t *pamh;
  int retval;

  confdir = getenv("srcdir");

  if (confdir == NULL)
    {
      fprintf (stderr, "Error: srcdir not set\n");
      return 1;
    }

  /* 1: check with valid arguments */
  retval = pam_start_confdir (service, user, &conv, confdir, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start_confdir (%s, %s, &conv, %s, &pamh) returned %d\n",
	       service, user, confdir, retval);
      return 1;
    }
  else if (pamh == NULL)
    {
      fprintf (stderr, "pam_start_confdir (%s, %s, &conv, %s, &pamh) returned NULL for pamh\n",
	       service, user, confdir);
      return 1;
    }

  pam_end (pamh, retval);

  /* 2: check with invalid service */
  retval = pam_start_confdir (xservice, user, &conv, confdir, &pamh);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start_confdir (%s, %s, &conv, %s, &pamh) incorrectly succeeded\n",
	       xservice, user, confdir);
      return 1;
    }

  pam_end (pamh, retval);

  /* 3: check with invalid confdir */
  retval = pam_start_confdir (service, user, &conv, xconfdir, &pamh);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start_confdir (%s, %s, &conv, %s, &pamh) incorrectly succeeded\n",
	       service, user, xconfdir);
      return 1;
    }

  pam_end (pamh, retval);

  return 0;
}
