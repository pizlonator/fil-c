#include <mutest.h>

static void
general_spec (void)
{
  bool a = true;

  mutest_expect ("a to be true",
                 mutest_bool_value (a),
                 mutest_to_be, true,
                 NULL);
  mutest_expect ("a to not be false",
                 mutest_bool_value (a),
                 mutest_not, mutest_to_be, false,
                 NULL);
}

static void
another_spec (void)
{
  const char *str = "hello, world";

  mutest_expect ("'str' to contain a greeting",
                 mutest_string_value (str),
                 mutest_to_contain, "hello",
                 NULL);
  mutest_expect ("'str' to contain who we are greeting",
                 mutest_string_value (str),
                 mutest_to_contain, "world",
                 NULL);
  mutest_expect ("'str' to contain all parts of a greeting",
                 mutest_string_value (str),
                 mutest_to_start_with_string, "hello",
                 mutest_to_contain, ",",
                 mutest_to_end_with_string, "world",
                 NULL);
}

static void
skip_spec (void)
{
  mutest_expect ("to be able to skip an expectation",
                 mutest_bool_value (true),
                 mutest_skip, "for any reason",
                 NULL);
}

static void
general_suite (void)
{
  mutest_it ("contains at least a spec with an expectation", general_spec);
  mutest_it ("can contain multiple specs", another_spec);
  mutest_it ("can contain expectations that can be skipped", skip_spec);
}

MUTEST_MAIN (
  mutest_describe ("A suite", general_suite);
)
