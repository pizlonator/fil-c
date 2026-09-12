/* Test that when BTI is not enforced an LD_DEBUG warning is printed
   when a library that does not have BTI marking is loaded via dlopen.  */
#define TEST_BTI_DLOPEN_MODULE "tst-bti-mod-unprot.so"
#define TEST_BTI_EXPECT_DLOPEN 1
#include "tst-bti-skeleton-dlopen.c"
