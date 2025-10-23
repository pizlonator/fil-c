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
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>

const char *envvals[] = {"VAL1=1", "VAL2=2", "VAL3=3"};

int
main (void)
{
  const char *service = "dummy";
  const char *user = "root";
  struct pam_conv conv = { NULL, NULL };
  pam_handle_t *pamh;
  int retval;
  char **ptr;
  char *temp;
  int var, i;

  /* 1: Call with NULL as pam handle */
  ptr = pam_getenvlist (NULL);
  if (ptr != NULL)
    {
      fprintf (stderr, "pam_getenvlist (NULL) does not return NULL\n");
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

  /* 2: Call with pam handle, but no environment set */
  ptr = pam_getenvlist (pamh);
  if (ptr == NULL || *ptr != NULL)
    {
      fprintf (stderr,
	       "pam_getenvlist (pamh) does not return pointer to NULL\n");
      temp = ptr ? *ptr : NULL;
      var = 0;
      while (temp)
	{
	  printf ("%s\n", temp);
	  var++;
	  temp = *(ptr + var);
	}
      return 1;
    }
  free (ptr);

  /* set environment variable */
  for (i = 0; i < 3; i++)
    {
      retval = pam_putenv (pamh, envvals[i]);
      if (retval != PAM_SUCCESS)
	{
	  fprintf (stderr, "pam_putenv (pamh, \"%s\") returned %d\n",
		   envvals[i], retval);
	  return 1;
	}
    }

  /* 3: Call with pam handle and environment set */
  ptr = pam_getenvlist (pamh);
  if (ptr == NULL)
    {
      fprintf (stderr, "pam_getenvlist (pamh) returned NULL\n");
      return 1;
    }
  else
    {
      temp = *ptr;
      var = 0;
      while (temp)
	{
	  if (strcmp (temp, envvals[var]) != 0)
	    {
	      fprintf (stderr,
		       "pam_getenvlist returns wrong value:\n"
		       "expected: %s\n"
		       "got: %s\n", envvals[var], temp);
	      return 1;
	    }
	  free (temp);
	  var++;
	  temp = *(ptr + var);
	}
      free (ptr);
    }

  pam_end (pamh, retval);

  return 0;
}
