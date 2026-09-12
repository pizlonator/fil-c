/* This test checks that dlopen succeeds when a dependency is loaded
   via dlopen and the corresponding binary is marked with BTI
   while BTI is enforced.  */
#define TEST_BTI_DLOPEN_MODULE "tst-bti-mod-prot.so"
#define TEST_BTI_EXPECT_DLOPEN 1
#include "tst-bti-skeleton-dlopen.c"
