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
#include <string.h>

#include <security/pam_appl.h>
#include "pam_inline.h"

struct mapping {
  int type;
  const char *string;
  int expected;
  const char *new_value;
};

struct mapping items[] = {
  {PAM_SERVICE, "PAM_SERVICE", PAM_SUCCESS, "logout"},
  {PAM_USER, "PAM_USER", PAM_SUCCESS, "noroot"},
  {PAM_TTY, "PAM_TTY", PAM_SUCCESS, "TTyX"},
  {PAM_RHOST, "PAM_RHOST", PAM_SUCCESS, "remote"},
  {PAM_AUTHTOK, "PAM_AUTHTOK", PAM_BAD_ITEM, "none"},
  {PAM_OLDAUTHTOK, "PAM_OLDAUTHTOK", PAM_BAD_ITEM, "none"},
  {PAM_RUSER, "PAM_RUSER", PAM_SUCCESS, "noroot"},
  {PAM_USER_PROMPT, "PAM_USER_PROMPT", PAM_SUCCESS, "your name: "},
  {PAM_FAIL_DELAY, "PAM_FAIL_DELAY", PAM_SUCCESS, "4000"},
  {PAM_AUTHTOK_TYPE, "PAM_AUTHTOK_TYPE", PAM_SUCCESS, "U**X"}
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

  /* 1: Call with NULL as pam handle */
  retval = pam_set_item (NULL, PAM_SERVICE, "dummy");
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_set_item (NULL, ...) returned PAM_SUCCESS\n");
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

  /* 2: check for bad item  */
  retval = pam_set_item (pamh, -1, "dummy");
  if (retval != PAM_BAD_ITEM)
    {
      fprintf (stderr,
	       "pam_set_item returned %d when expecting PAM_BAD_ITEM\n",
	       retval);
      return 1;
    }

  /* 3: try to set PAM_CONV to NULL  */
  retval = pam_set_item (pamh, PAM_CONV, NULL);
  if (retval != PAM_PERM_DENIED)
    {
      fprintf (stderr,
	       "pam_set_item (pamh, PAM_CONV, NULL) returned %d\n",
	       retval);
      return 1;
    }

  /* 4: try to replace all items */
  for (i = 0; i < PAM_ARRAY_SIZE(items); i++)
    {
      retval = pam_set_item (pamh, items[i].type, items[i].new_value);

      if (retval != items[i].expected)
	{
	  fprintf (stderr,
		   "pam_set_item failed to set value for %s. Returned %d\n",
		   items[i].string, retval);
	  return 1;
        }
      else if (items[i].expected == PAM_SUCCESS)
	{
	  const void *value;

	  retval = pam_get_item (pamh, items[i].type, &value);
	  if (retval != PAM_SUCCESS)
	    {
	      fprintf (stderr,
		       "pam_get_item was not able to fetch changed value: %d\n",
		       retval);
	      return 1;
	    }
	  if (strcmp (items[i].new_value, value) != 0)
	    {
	      fprintf (stderr,
		       "pam_get_item got wrong value:\n"
		       "expected: %s\n"
		       "got: %s\n", items[i].new_value, (const char *)value);
	      return 1;
	    }
	}
    }

  /* 5: try to set PAM_SERVICE to NULL  */
  retval = pam_set_item (pamh, PAM_SERVICE, NULL);
  if (retval != PAM_BAD_ITEM)
    {
      fprintf (stderr,
	       "pam_set_item (pamh, PAM_SERVICE, NULL) returned %d\n",
	       retval);
      return 1;
    }

  pam_end (pamh, 0);

  return 0;
}
