/* Test that when GCS is optional an LD_DEBUG warning is printed when
   one of the shared library dependencies does not have GCS marking.  */
#define TEST_GCS_EXPECT_ENABLED 0
#include "tst-gcs-shared.c"
