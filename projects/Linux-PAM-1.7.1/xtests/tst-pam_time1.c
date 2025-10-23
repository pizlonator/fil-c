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

  Check the following lines in time.conf:
  *;*;you|me;!Al0000-2400
  *;*;x|y;!Al0000-2400

  User 'x' should not be able to login.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include "pam_inline.h"


struct test_t {
  const char *user;
  int retval;
};

static struct test_t tests[] = {
  {"xy", 0},
  {"yx", 0},
  {"you",6},
  {"me", 6},
  {"x",  6},
  {"y",  6},
};

static struct pam_conv conv = {
  NULL, NULL
};

int
main(int argc, char *argv[])
{
  pam_handle_t *pamh = NULL;
  int retval;
  int debug = 0;
  unsigned int i;

  if (argc > 1 && strcmp (argv[1], "-d") == 0)
    debug = 1;

  for (i = 0; i < PAM_ARRAY_SIZE(tests); i++)
    {
      retval = pam_start("tst-pam_time1", tests[i].user, &conv, &pamh);
      if (retval != PAM_SUCCESS)
	{
	  if (debug)
	    fprintf (stderr, "pam_time1: pam_start returned %d\n", retval);
	  return 1;
	}

      retval = pam_acct_mgmt (pamh, 0);
      if (retval != tests[i].retval)
	{
	  if (debug)
	    fprintf (stderr,
		     "pam_time1: pam_acct_mgmt(%s) returned wrong value, %d, expected %d\n",
		     tests[i].user, retval, tests[i].retval);
	  return 1;
	}

      retval = pam_end (pamh,retval);
      if (retval != PAM_SUCCESS)
	{
	  if (debug)
	    fprintf (stderr, "pam_time1: pam_end returned %d\n", retval);
	  return 1;
	}
    }
  return 0;
}
