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

#include <pam_private.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

static int cleanup_was_called = 0;
static int cleanup3_was_called = 0;
static int cleanup3_retval = 0;
static int cleanup6_was_called = 0;
static int cleanup6_retval = 0;
static int cleanup7_was_called = 0;
static int cleanup7_retval = 0;
static int cleanup7b_was_called = 0;
static int cleanup7b_retval = 0;
static int cleanup8_was_called = 0;
static int cleanup8_retval = 0;

static void
tst_cleanup (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup_was_called = 1;
  fprintf (stderr, "tst_cleanup was called: data=\"%s\", error_status=%d\n",
	   (char *)data, error_status);
}

static void
tst_cleanup_3 (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup3_was_called = 1;

  if (strcmp (data, "test3") != 0)
    {
      fprintf (stderr, "tst_cleanup_3 called with wrong data, got \"%s\"\n",
	       (char *)data);
      cleanup3_retval = 1;
      return;
    }

  free (data);

  if (error_status & PAM_DATA_REPLACE)
    {
      fprintf (stderr, "tst_cleanup_3 called with PAM_DATA_REPLACE set\n");
      cleanup3_retval = 1;
      return;
    }

  if (error_status & PAM_DATA_SILENT)
    {
      fprintf (stderr, "tst_cleanup_3 called with PAM_DATA_SILENT set\n");
      cleanup3_retval = 1;
      return;
    }

  if (error_status != 0)
    {
      fprintf (stderr, "tst_cleanup_3 called with error_status set: %d\n",
	       error_status);
      cleanup3_retval = 1;
      return;
    }
}

static void
tst_cleanup_6 (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup6_was_called = 1;

  if (error_status & PAM_DATA_SILENT)
    {
      fprintf (stderr, "tst_cleanup_6 called with PAM_DATA_SILENT set\n");
      cleanup6_retval = 1;
      return;
    }

  if (error_status & PAM_DATA_REPLACE)
    {
      if (strcmp (data, "test6a") != 0)
	{
	  fprintf (stderr, "tst_cleanup_6 called with wrong data, got \"%s\"\n",
		   (char *)data);
	  cleanup6_retval = 1;
	  return;
	}

      if (error_status != PAM_DATA_REPLACE)
	{
	  fprintf (stderr, "tst_cleanup_6 called with error_status set: %d\n",
		   error_status);
	  cleanup6_retval = 1;
	  return;
	}
    }
  else
    {
      if (strcmp (data, "test6b") != 0)
	{
	  fprintf (stderr, "tst_cleanup_6 called with wrong data, got \"%s\"\n",
		   (char *)data);
	  cleanup6_retval = 1;
	  return;
	}

      if (error_status != 0)
	{
	  fprintf (stderr, "tst_cleanup_6 called with error_status set: %d\n",
		   error_status);
	  cleanup6_retval = 1;
	  return;
	}
    }

  free (data);
}

static void
tst_cleanup_7 (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup7_was_called = 1;

  if (error_status & PAM_DATA_SILENT)
    {
      fprintf (stderr, "tst_cleanup_7 called with PAM_DATA_SILENT set\n");
      cleanup7_retval = 1;
      return;
    }

  if (error_status & PAM_DATA_REPLACE)
    {
      if (strcmp (data, "test7a") != 0)
	{
	  fprintf (stderr, "tst_cleanup_7 called with wrong data, got \"%s\"\n",
		   (char *)data);
	  cleanup7_retval = 1;
	  return;
	}

      if (error_status != PAM_DATA_REPLACE)
	{
	  fprintf (stderr, "tst_cleanup_7 called with error_status set: %d\n",
		   error_status);
	  cleanup7_retval = 1;
	  return;
	}
    }
  else
    {
      fprintf (stderr, "tst_cleanup_7 called without PAM_DATA_REPLACE set: %d\n",
	       error_status);
      cleanup7_retval = 1;
      return;
    }

  free (data);
}

static void
tst_cleanup_7b (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup7b_was_called = 1;

  if (strcmp (data, "test7b") != 0)
    {
      fprintf (stderr, "tst_cleanup_7b called with wrong data, got \"%s\"\n",
	       (char *)data);
      cleanup7b_retval = 1;
      return;
    }

  free (data);

  if (error_status & PAM_DATA_REPLACE)
    {
      fprintf (stderr, "tst_cleanup_7b called with PAM_DATA_REPLACE set\n");
      cleanup7b_retval = 1;
      return;
    }

  if (error_status & PAM_DATA_SILENT)
    {
      fprintf (stderr, "tst_cleanup_7b called with PAM_DATA_SILENT set\n");
      cleanup7b_retval = 1;
      return;
    }

  if (error_status != 0)
    {
      fprintf (stderr, "tst_cleanup_7b called with error_status set: %d\n",
	       error_status);
      cleanup7b_retval = 1;
      return;
    }
}

static void
tst_cleanup_8 (pam_handle_t *pamh UNUSED, void *data, int error_status)
{
  cleanup8_was_called = 1;

  if (strcmp (data, "test8") != 0)
    {
      fprintf (stderr, "tst_cleanup_8 called with wrong data, got \"%s\"\n",
	       (char *)data);
      cleanup8_retval = 1;
      return;
    }

  free (data);

  if (error_status & PAM_DATA_REPLACE)
    {
      fprintf (stderr, "tst_cleanup_8 called with PAM_DATA_REPLACE set\n");
      cleanup8_retval = 1;
      return;
    }

  if (error_status & PAM_DATA_SILENT)
    {
      fprintf (stderr, "tst_cleanup_8 called with PAM_DATA_SILENT set\n");
      cleanup8_retval = 1;
      return;
    }

  if (error_status != 987)
    {
      fprintf (stderr, "tst_cleanup_8 called with wrong error_status set: %d\n",
	       error_status);
      cleanup8_retval = 1;
      return;
    }
}

int
main (void)
{
  const char *service = "dummy";
  const char *user = "root";
  struct pam_conv conv = { NULL, NULL };
  pam_handle_t *pamh;
  void *dataptr;
  int retval;

  /* 1: Call with NULL as pam handle */
  dataptr = strdup ("test1");
  retval = pam_set_data (NULL, "tst-pam_set_data-1", dataptr, tst_cleanup);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr, "pam_set_data (NULL, ...) returned PAM_SUCCESS\n");
      return 1;
    }
  free (dataptr);

  /* setup pam handle */
  retval = pam_start (service, user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, &pamh) returned %d\n",
               service, user, retval);
      return 1;
    }

  /* 2: check for call from application  */
  dataptr = strdup ("test2");
  retval = pam_set_data (pamh, "tst-pam_set_data-2", dataptr, tst_cleanup);
  if (retval != PAM_SYSTEM_ERR)
    {
      fprintf (stderr,
	       "pam_set_data returned %d when expecting PAM_SYSTEM_ERR\n",
	       retval);
      return 1;
    }
  free (dataptr);


  /* 3: check for call from module  */
  __PAM_TO_MODULE(pamh);
  dataptr = strdup ("test3");
  retval = pam_set_data (pamh, "tst-pam_set_data-3", dataptr,
			 tst_cleanup_3);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  /* 4: check for call with NULL as module_data_name */
  dataptr = strdup ("test4");
  retval = pam_set_data (pamh, NULL, dataptr, tst_cleanup);
  if (retval == PAM_SUCCESS)
    {
      fprintf (stderr,
	       "pam_set_data with NULL as module_data_name succeeded!\n");
      return 1;
    }
  free (dataptr);

  /* 5: check for call with NULL as cleanup function */
  dataptr = strdup ("test5");
  retval = pam_set_data (pamh, "tst-pam_set_data-5", dataptr, NULL);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "pam_set_data with NULL as cleanup function failed: %d\n",
	       retval);
      return 1;
    }
  free (dataptr);

  /* 6: Overwrite data and check cleanup flags */
  dataptr = strdup ("test6a");
  retval = pam_set_data (pamh, "tst-pam_set_data-6", dataptr,
			 tst_cleanup_6);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "test6: first pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  dataptr = strdup ("test6b");
  retval = pam_set_data (pamh, "tst-pam_set_data-6", dataptr,
			 tst_cleanup_6);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "test6: second pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  /* 7: Overwrite data and cleanup function, check cleanup flags */
  dataptr = strdup ("test7a");
  retval = pam_set_data (pamh, "tst-pam_set_data-7", dataptr,
			 tst_cleanup_7);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "test7: first pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  dataptr = strdup ("test7b");
  retval = pam_set_data (pamh, "tst-pam_set_data-7", dataptr,
			 tst_cleanup_7b);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "test7: second pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  __PAM_TO_APP(pamh);

  /* Close PAM handle and check return codes of cleanup functions */
  retval = pam_end (pamh, 0);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "pam_end reported an error: %d\n",
	       retval);
      return 1;
    }

  if (cleanup_was_called == 1)
    return 1;

  if (cleanup3_was_called == 0)
    {
      fprintf (stderr, "tst_cleanup_3 was never called!\n");
      return 1;
    }
  if (cleanup3_retval != 0)
    return 1;

  if (cleanup6_was_called == 0)
    {
      fprintf (stderr, "tst_cleanup_6 was never called!\n");
      return 1;
    }
  if (cleanup6_retval != 0)
    return 1;

  if (cleanup7_was_called == 0)
    {
      fprintf (stderr, "tst_cleanup_7 was never called!\n");
      return 1;
    }
  if (cleanup7_retval != 0)
    return 1;

  if (cleanup7b_was_called == 0)
    {
      fprintf (stderr, "tst_cleanup_7b was never called!\n");
      return 1;
    }
  if (cleanup7b_retval != 0)
    return 1;

  /* test if error code is delivered to cleanup function */
  /* setup pam handle */
  retval = pam_start (service, user, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr, "pam_start (%s, %s, &conv, &pamh) returned %d\n",
               service, user, retval);
      return 1;
    }

  /* 8: check if cleanup function is called with correct error code */
  __PAM_TO_MODULE(pamh);
  dataptr = strdup ("test8");
  retval = pam_set_data (pamh, "tst-pam_set_data-8", dataptr,
			 tst_cleanup_8);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "test8: pam_set_data failed: %d\n",
	       retval);
      return 1;
    }

  __PAM_TO_APP(pamh);

  retval = pam_end (pamh, 987);
  if (retval != PAM_SUCCESS)
    {
      fprintf (stderr,
	       "pam_end reported an error: %d\n",
	       retval);
      return 1;
    }

  if (cleanup8_was_called == 0)
    {
      fprintf (stderr, "tst_cleanup_3 was never called!\n");
      return 1;
    }

  if (cleanup8_retval != 0)
    return 1;

  return 0;
}
