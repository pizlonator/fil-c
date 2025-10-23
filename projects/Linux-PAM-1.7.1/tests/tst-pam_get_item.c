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
#include "pam_inline.h"

struct mapping {
  int type;
  const char *string;
  int expected;
};

struct mapping items[] = {
  {PAM_SERVICE, "PAM_SERVICE", PAM_SUCCESS},
  {PAM_USER, "PAM_USER", 0},
  {PAM_TTY, "PAM_TTY", 0},
  {PAM_RHOST, "PAM_RHOST", 0},
  {PAM_CONV, "PAM_CONV", 0},
  {PAM_AUTHTOK, "PAM_AUTHTOK", PAM_BAD_ITEM},
  {PAM_OLDAUTHTOK, "PAM_OLDAUTHTOK", PAM_BAD_ITEM},
  {PAM_RUSER, "PAM_RUSER", 0},
  {PAM_USER_PROMPT, "PAM_USER_PROMPT", 0},
  {PAM_FAIL_DELAY, "PAM_FAIL_DELAY", 0},
  {PAM_AUTHTOK_TYPE, "PAM_AUTHTOK_TYPE", 0}
};

int
main (void)
{
  const char *service = "dummy";
  const char *user = "root";
  struct pam_conv conv = { NULL, NULL };
  pam_handle_t *pamh;
  int retval;
  unsigned int i;
  const void *value;

  /* 1: Call with NULL as pam handle */
  retval = pam_get_item (NULL, PAM_SERVICE, &value);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_get_item (NULL, 0) returned PAM_SUCCESS\n");
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

  /* 2: check for valid item types. Expected return value is
     PAM_SUCCESS, except it has to fail. */
  for (i = 0; i < PAM_ARRAY_SIZE(items); i++)
    {
      retval = pam_get_item (pamh, items[i].type, &value);

      if (retval != items[i].expected)
	{
	  fprintf (stderr,
		   "pam_get_item failed to get value for %s. Returned %d\n",
		   items[i].string, retval);
	  return 1;
        }
    }

  /* 3: check for bad item  */
  retval = pam_get_item (pamh, -1, &value);
  if (retval != PAM_BAD_ITEM)
    {
      fprintf (stderr,
	       "pam_get_item returned %d when expecting PAM_BAD_ITEM\n",
	       retval);
      return 1;
    }

  /* 4: check for valid item types, but NULL as value address. */
  for (i = 0; i < PAM_ARRAY_SIZE(items); i++)
    {
      retval = pam_get_item (pamh, items[i].type, NULL);

      if (retval != PAM_PERM_DENIED)
	{
	  fprintf (stderr,
		   "pam_get_item returned %d to get value for %s\n",
		   retval, items[i].string);
	  return 1;
        }
    }

  pam_end (pamh, 0);

  return 0;
}
