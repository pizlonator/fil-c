/* This test checks that dlopen returns an error when a dependency
   is loaded via dlopen and the corresponding binary is not marked
   with BTI while BTI is enforced.  */
#define TEST_BTI_DLOPEN_MODULE "tst-bti-mod-unprot.so"
#define TEST_BTI_EXPECT_DLOPEN 0
#include "tst-bti-skeleton-dlopen.c"
