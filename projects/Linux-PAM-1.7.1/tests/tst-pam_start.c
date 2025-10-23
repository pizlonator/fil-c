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
#include <unistd.h>

#include <security/pam_appl.h>


int
main (void)
{
  const char *service = "dummy";
  const char *user = "root";
  struct pam_conv conv = { NULL, NULL };
  pam_handle_t *pamh;
  int retval;

  /* 1: check with valid arguments */
  retval = pam_start (service, user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, &pamh) returned %d\n",
	       service, user, retval);
      return 1;
    }
  else if (pamh == NULL)
    {
      fprintf (stderr,
	       "pam_start (%s, %s, &conv, &pamh) returned NULL for pamh\n",
	       service, user);
      return 1;
    }

  pam_end (pamh, retval);

  /* 2: check with NULL for service */
  retval = pam_start (NULL, user, &conv, &pamh);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (NULL, %s, &conv, &pamh) returned %d\n",
	       user, retval);
      return 1;
    }

  /* 3: check with NULL for user */
  retval = pam_start (service, NULL, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, NULL, &conv, &pamh) returned %d\n",
	       service, retval);
      return 1;
    }

  pam_end (pamh, retval);


  /* 4: check with NULL for conv */
  retval = pam_start (service, user, NULL, &pamh);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, NULL, &pamh) returned %d\n",
	       service, user, retval);
      return 1;
    }

  /* 5: check with NULL for pamh */
  retval = pam_start (service, user, &conv, NULL);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, NULL) returned %d\n",
	       service, user, retval);
      return 1;
    }

  return 0;
}
