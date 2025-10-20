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

  Check the following line in group.conf:

  tst-pam_group1;*;tstpamgrp;Al0000-2400;tstpamgrpg


  pam_group should add group tstpamgrpg to user tstpamgrp, but not
  to tstpamgrp2.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <security/pam_appl.h>

#define GROUP_BLK 10
#define blk_size(len) (((len-1 + GROUP_BLK)/GROUP_BLK)*GROUP_BLK)

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

static int debug = 0;

static int
run_test (const char *user, gid_t groupid, int needit)
{
  pam_handle_t *pamh = NULL;
  int retval;
  int no_grps;

  retval = pam_start("tst-pam_group1", user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_group1: pam_start returned %d\n", retval);
      return 1;
    }

  retval = pam_set_item (pamh, PAM_TTY, "/dev/tty1");
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr,
		 "pam_group1: pam_set_item(PAM_TTY) returned %d\n",
		 retval);
      return 1;
    }

  retval = pam_authenticate (pamh, 0);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_group1: pam_authenticate returned %d\n", retval);
      return 1;
    }

  retval = pam_setcred (pamh, PAM_ESTABLISH_CRED);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_group1: pam_setcred returned %d\n", retval);
      return 1;
    }


  no_grps = getgroups(0, NULL); /* find the current number of groups */
  if (no_grps > 0)
    {
      int i, found;
      gid_t *grps = calloc (blk_size (no_grps), sizeof(gid_t));

      if (getgroups(no_grps, grps) < 0)
	{
	  if (debug)
	    fprintf (stderr, "pam_group1: getroups returned error: %m\n");
	  pam_end (pamh, PAM_SYSTEM_ERR);
	  return 1;
        }

      found = 0;
      for (i = 0; i < no_grps; ++i)
	{
#if 0
	  if (debug)
	    fprintf (stderr, "gid[%d]=%d\n", i, grps[i]);
#endif
	  if (grps[i] == groupid)
	    found = 1;
	}
      if ((needit && found) || (!needit && !found))
	{
	  /* everything is ok */
	}
      else
	{
	  pam_end (pamh, PAM_SYSTEM_ERR);
	  if (debug)
	    fprintf (stderr,
		     "pam_group1: unexpected result for %s: needit=%d, found=%d\n",
		     user, needit, found);
	  return 1;
	}
    }

  retval = pam_end (pamh,retval);
  if (retval != PAM_SUCCESS)
    {
      if (debug)
	fprintf (stderr, "pam_group1: pam_end returned %d\n", retval);
      return 1;
    }
  return 0;
}

int
main(int argc, char *argv[])
{
  struct group *grp;
  gid_t grpid;

  if (argc > 1 && strcmp (argv[1], "-d") == 0)
    debug = 1;

  grp = getgrnam ("tstpamgrpg");
  if (grp == NULL)
    return 1;
  grpid = grp->gr_gid;

  if (run_test ("root", grpid, 0) != 0 ||
      run_test ("tstpamgrp2", grpid, 0) != 0 ||
      run_test ("tstpamgrp", grpid, 1) != 0)
    return 1;

  return 0;
}
