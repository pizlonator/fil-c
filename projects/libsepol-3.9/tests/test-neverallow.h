#ifndef TEST_NEVERALLOW_H__
#define TEST_NEVERALLOW_H__

#include <CUnit/Basic.h>

int neverallow_test_init(void);
int neverallow_test_cleanup(void);
int neverallow_add_tests(CU_pSuite suite);

#endif  /* TEST_NEVERALLOW_H__ */
