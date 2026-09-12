/* Test that when GCS is optional an LD_DEBUG warning is printed when
   a library that does not have GCS marking is loaded via dlopen.  */
#define TEST_GCS_EXPECT_ENABLED 0
#define TEST_GCS_EXPECT_DLOPEN 0
#include "tst-gcs-dlopen.c"
