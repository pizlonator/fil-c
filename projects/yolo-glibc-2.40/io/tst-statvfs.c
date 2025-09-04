#include <stdio.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <support/check.h>


/* This test cannot detect many errors.  But it will fail if the
   statvfs is completely hosed and it'll detect a missing export.  So
   it is better than nothing.  */
static int
do_test (int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i)
    {
      struct statvfs st;
      struct statfs stf;
      TEST_COMPARE (statvfs (argv[i], &st), 0);
      TEST_COMPARE (statfs (argv[i], &stf), 0);
      TEST_COMPARE (st.f_type, (unsigned int) stf.f_type);
      printf ("%s: free: %llu, mandatory: %s, tp=%x\n", argv[i],
              (unsigned long long int) st.f_bfree,
#ifdef ST_MANDLOCK
              (st.f_flag & ST_MANDLOCK) ? "yes" : "no",
#else
              "no",
#endif
              st.f_type);
    }
  return 0;
}

#define TEST_FUNCTION do_test (argc, argv)
#include "../test-skeleton.c"
