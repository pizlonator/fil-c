/* This test checks that dlopen returns an error when a dependency
   is loaded via dlopen that has an unmarked dependency while BTI
   is enforced.  */
#define TEST_BTI_DLOPEN_MODULE "tst-bti-mod.so"
#define TEST_BTI_EXPECT_DLOPEN 0
#include "tst-bti-skeleton-dlopen.c"
