/* Test enumerator iteration and querying.  Because
   ctf_arc_lookup_enumerator_next uses ctf_lookup_enumerator_next internally, we
   only need to test the former.  */

#include "config.h"
#include <ctf-api.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
print_constants (ctf_archive_t *ctf, const char *name)
{
  ctf_next_t *i = NULL;
  int err;
  ctf_dict_t *fp;
  ctf_id_t type;
  int64_t val;

  while ((type = ctf_arc_lookup_enumerator_next (ctf, name, &i,
						 &val, &fp, &err)) != CTF_ERR)
    {
      char *foo;

      printf ("%s in %s has value %li\n", name,
	      foo = ctf_type_aname (fp, type), (long int) val);
      free (foo);

      ctf_dict_close (fp);
    }
  if (err != ECTF_NEXT_END)
    {
      fprintf (stderr, "iteration failed: %s\n", ctf_errmsg (err));
      exit (1);
    }
}

int
main (int argc, char *argv[])
{
  ctf_archive_t *ctf;
  ctf_dict_t *fp;
  int err;
  ctf_id_t type;
  ctf_next_t *i = NULL;
  int64_t val;
  int counter = 0;

  if (argc != 2)
    {
      fprintf (stderr, "Syntax: %s PROGRAM\n", argv[0]);
      exit(1);
    }

  if ((ctf = ctf_open (argv[1], NULL, &err)) == NULL)
    goto open_err;

  /* Look for all instances of ENUMSAMPLE2_2, and add some new enums to all
     dicts found, to test dynamic enum iteration as well as static.

     Add two enums with a different name and constants to any that should
     already be there (one hidden), and one with the same constants, but hidden,
     to test ctf_lookup_enumerator_next()'s multiple-lookup functionality and
     ctf_lookup_enumerator() in the presence of hidden types.

     This also tests that you can add to enums under iteration without causing
     disaster.  */

  printf ("First iteration: addition of enums.\n");
  while ((type = ctf_arc_lookup_enumerator_next (ctf, "IENUMSAMPLE2_2", &i,
						 &val, &fp, &err)) != CTF_ERR)
    {
      char *foo;
      int dynadd2_value;
      int old_dynadd2_flag;

      /* Make sure that getting and setting a garbage flag, and setting one to a
	 garbage value, fails properly.  */
      if (ctf_dict_set_flag (fp, CTF_STRICT_NO_DUP_ENUMERATORS, 666) >= 0
	  || ctf_errno (fp) != ECTF_BADFLAG)
	fprintf (stderr, "Invalid flag value setting did not fail as it ought to\n");

      if (ctf_dict_set_flag (fp, 0, 1) >= 0 || ctf_errno (fp) != ECTF_BADFLAG)
	fprintf (stderr, "Invalid flag setting did not fail as it ought to\n");

      if (ctf_dict_get_flag (fp, 0) >= 0 || ctf_errno (fp) != ECTF_BADFLAG)
	fprintf (stderr, "Invalid flag getting did not fail as it ought to\n");

      /* Set it strict for now.  */
      if (ctf_dict_set_flag (fp, CTF_STRICT_NO_DUP_ENUMERATORS, 1) < 0)
	goto set_flag_err;

      printf ("IENUMSAMPLE2_2 in %s has value %li\n",
	      foo = ctf_type_aname (fp, type), (long int) val);
      free (foo);

      if ((type = ctf_add_enum (fp, CTF_ADD_ROOT, "ie3")) == CTF_ERR)
	goto enum_add_err;

      if (ctf_add_enumerator (fp, type, "DYNADD", counter += 10) < 0)
	goto enumerator_add_err;

      if (ctf_add_enumerator (fp, type, "DYNADD2", counter += 10) < 0)
	goto enumerator_add_err;
      dynadd2_value = counter;

      /* Make sure that overlapping enumerator addition fails as it should.  */

      if (ctf_add_enumerator (fp, type, "IENUMSAMPLE2_2", 666) >= 0
	  || ctf_errno (fp) != ECTF_DUPLICATE)
	fprintf (stderr, "Duplicate enumerator addition did not fail as it ought to\n");

      /* Make sure that it still fails if you set an enum value to the value it
	 already has.  */
      if (ctf_add_enumerator (fp, type, "DYNADD2", dynadd2_value) >= 0
	  || ctf_errno (fp) != ECTF_DUPLICATE)
	fprintf (stderr, "Duplicate enumerator addition did not fail as it ought to\n");

      /* Flip the strict flag and try again.  This time, it should succeed.  */

      if ((old_dynadd2_flag = ctf_dict_get_flag (fp, CTF_STRICT_NO_DUP_ENUMERATORS)) < 0)
	goto get_flag_err;

      if (ctf_dict_set_flag (fp, CTF_STRICT_NO_DUP_ENUMERATORS, 0) < 0)
	goto set_flag_err;

      if (ctf_add_enumerator (fp, type, "DYNADD2", dynadd2_value) < 0)
	goto enumerator_add_err;

      /* Flip it again and try *again*.  This time it should fail again.  */

      if (ctf_dict_set_flag (fp, CTF_STRICT_NO_DUP_ENUMERATORS, old_dynadd2_flag) < 0)
	goto set_flag_err;

      if (ctf_add_enumerator (fp, type, "DYNADD2", dynadd2_value) >= 0
	  || ctf_errno (fp) != ECTF_DUPLICATE)
	fprintf (stderr, "Duplicate enumerator addition did not fail as it ought to\n");

      if ((type = ctf_add_enum (fp, CTF_ADD_NONROOT, "ie4_hidden")) == CTF_ERR)
	goto enum_add_err;

      if (ctf_add_enumerator (fp, type, "DYNADD3", counter += 10) < 0)
	goto enumerator_add_err;
      if (ctf_add_enumerator (fp, type, "DYNADD4", counter += 10) < 0)
	goto enumerator_add_err;

      if ((type = ctf_add_enum (fp, CTF_ADD_NONROOT, "ie3_hidden")) == CTF_ERR)
	goto enum_add_err;

      if (ctf_add_enumerator (fp, type, "DYNADD", counter += 10) < 0)
	goto enumerator_add_err;
      if (ctf_add_enumerator (fp, type, "DYNADD2", counter += 10) < 0)
	goto enumerator_add_err;

      /* Look them up via ctf_lookup_enumerator.  DYNADD2 should fail because
	 it has duplicate enumerators.  */

      if (ctf_lookup_enumerator (fp, "DYNADD", &val) == CTF_ERR)
	goto enumerator_lookup_err;
      printf ("direct lookup: DYNADD value: %i\n", (int) val);

      if ((err = ctf_lookup_enumerator (fp, "DYNADD2", &val)) >= 0 ||
	  ctf_errno (fp) != ECTF_DUPLICATE)
	fprintf (stderr, "Duplicate enumerator lookup did not fail as it ought to: %i, %s\n",
		 err, ctf_errmsg (ctf_errno (fp)));

      if ((type = ctf_lookup_enumerator (fp, "DYNADD3", &val) != CTF_ERR) ||
	  ctf_errno (fp) != ECTF_NOENUMNAM)
	{
	  if (type != CTF_ERR)
	    {
	      char *foo;
	      printf ("direct lookup: hidden lookup did not return ECTF_NOENUMNAM but rather %li in %s\n",
		      (long int) val, foo = ctf_type_aname (fp, type));
	      free (foo);
	    }
	  else
	    printf ("direct lookup: hidden lookup did not return ECTF_NOENUMNAM but rather %s\n",
		    ctf_errmsg (ctf_errno (fp)));
	}

      ctf_dict_close (fp);
    }
  if (err != ECTF_NEXT_END)
    {
      fprintf (stderr, "iteration failed: %s\n", ctf_errmsg (err));
      return 1;
    }

  /* Look for (and print out) some enumeration constants.  */

  printf ("Second iteration: printing of enums.\n");

  print_constants (ctf, "ENUMSAMPLE_1");
  print_constants (ctf, "IENUMSAMPLE_1");
  print_constants (ctf, "ENUMSAMPLE_2");
  print_constants (ctf, "DYNADD");
  print_constants (ctf, "DYNADD3");

  ctf_close (ctf);

  printf ("All done.\n");

  return 0;

 open_err:
  fprintf (stderr, "%s: cannot open: %s\n", argv[0], ctf_errmsg (err));
  return 1;
 enum_add_err:
  fprintf (stderr, "Cannot add enum to dict \"%s\": %s\n",
	   ctf_cuname (fp) ? ctf_cuname (fp) : "(null: parent)", ctf_errmsg (ctf_errno (fp)));
  return 1;
 enumerator_add_err:
  fprintf (stderr, "Cannot add enumerator to dict \"%s\": %s\n",
	   ctf_cuname (fp) ? ctf_cuname (fp) : "(null: parent)", ctf_errmsg (ctf_errno (fp)));
  return 1;
 enumerator_lookup_err:
  fprintf (stderr, "Cannot look up enumerator in dict \"%s\": %s\n",
	   ctf_cuname (fp) ? ctf_cuname (fp) : "(null: parent)", ctf_errmsg (ctf_errno (fp)));
  return 1;
 get_flag_err:
  fprintf (stderr, "ctf_dict_get_flag failed: %s\n", ctf_errmsg (ctf_errno (fp)));
  return 1;
 set_flag_err:
  fprintf (stderr, "ctf_dict_set_flag failed: %s\n", ctf_errmsg (ctf_errno (fp)));
  return 1;
}
