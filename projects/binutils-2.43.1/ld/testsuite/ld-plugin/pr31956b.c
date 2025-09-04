struct CMUnitTest {
  void *test_func;
};

extern void _cmocka_run_group_tests(void *);

extern void argv_parse_cmd(void);
void __wrap_parse_line(void) {};

void foo (void) {
  argv_parse_cmd();
}

struct CMUnitTest main_tests = {
   foo
};

int
main (void)
{
  _cmocka_run_group_tests (&main_tests);
  return 0;
}
