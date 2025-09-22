#include <stdlib.h>

#include <check.h>

#include "pipeline.h"

#define TEST_CASE(suite, test, name)                                          \
  do {                                                                        \
    TCase *t = tcase_create (#name);                                          \
    tcase_add_test (t, test_##test##_##name);                                 \
    suite_add_tcase (suite, t);                                               \
  } while (0)

#define TEST_CASE_WITH_FIXTURE(suite, test, name, setup, teardown)            \
  do {                                                                        \
    TCase *t = tcase_create (#name);                                          \
    tcase_add_checked_fixture (t, setup, teardown);                           \
    tcase_add_test (t, test_##test##_##name);                                 \
    suite_add_tcase (suite, t);                                               \
  } while (0)

#define MAIN(test)                                                            \
  int main (int argc _GL_UNUSED, char **argv _GL_UNUSED)                      \
  {                                                                           \
    int failed;                                                               \
    Suite *s = test##_suite ();                                               \
    SRunner *sr = srunner_create (s);                                         \
                                                                              \
    srunner_run_all (sr, CK_ENV);                                             \
    failed = srunner_ntests_failed (sr);                                      \
    srunner_free (sr);                                                        \
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;                       \
  }

extern char *temp_dir;

void temp_dir_setup (void);
void temp_dir_teardown (void);
