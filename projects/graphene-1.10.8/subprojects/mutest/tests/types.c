#include <mutest.h>

static void
check_scalars (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("booleans to have two states",
                 mutest_bool_value (true),
                 mutest_to_be, true,
                 mutest_not, mutest_to_be, false,
                 NULL);
  mutest_expect ("integer values to respond to multiple matchers",
                 mutest_int_value (42),
                 mutest_to_be, 42,
                 mutest_not, mutest_to_be, 47,
                 mutest_to_be_close_to, 42, 1,
                 mutest_to_be_in_range, 40, 50,
                 NULL);
  mutest_expect ("float values to require tolerance",
                 mutest_float_value (3.1416),
                 mutest_to_be_close_to, 3.1415, 0.0001,
                 mutest_not, mutest_to_be_close_to, 3.14159, 0.00001,
                 NULL);

  void *p = (void *) 0xdeadbeef;

  mutest_expect ("pointers to be like integer values",
                 mutest_pointer (p),
                 mutest_not, mutest_to_be, NULL,
                 mutest_to_be, (void *) 0xdeadbeef,
                 NULL);
}

static void
check_ordering (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("integers to support 'greater' and 'greater or equal' ordering",
                 mutest_int_value (42),
                 mutest_to_be_greater_than, 40,
                 mutest_not, mutest_to_be_greater_than, 42,
                 mutest_to_be_greater_than_or_equal, 42,
                 NULL);
  mutest_expect ("integers to support 'less' and 'less or equal' ordering",
                 mutest_int_value (47),
                 mutest_to_be_less_than, 50,
                 mutest_not, mutest_to_be_less_than, 47,
                 mutest_to_be_less_than_or_equal, 47,
                 NULL);
}

static void
check_ranges (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("integers to support ranges",
                 mutest_int_value (42),
                 mutest_to_be_in_range, 40, 45,
                 mutest_not, mutest_to_be_in_range, 30, 41,
                 mutest_not, mutest_to_be_in_range, 43, 50,
                 NULL);
  mutest_expect ("floats to support ranges",
                 mutest_float_value (3.14),
                 mutest_to_be_in_range, 3.0, 4.0,
                 NULL);

  mutest_expect ("contains() to work on integer ranges",
                 mutest_int_range (40, 45),
                 mutest_to_contain, 42,
                 mutest_to_contain, 40,
                 mutest_to_contain, 45,
                 mutest_not, mutest_to_contain, 30,
                 mutest_not, mutest_to_contain, 50,
                 NULL);
  mutest_expect ("contains() to work on float ranges",
                 mutest_float_range (0.0, 1.0),
                 mutest_to_contain, 0.0,
                 mutest_to_contain, 1.0,
                 mutest_to_contain, 0.5,
                 mutest_not, mutest_to_contain, 2.0,
                 NULL);
}

static void
check_string (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("strings to support exact matching",
                 mutest_string_value ("hello, world"),
                 mutest_to_be, "hello, world",
                 NULL);
  mutest_expect ("strings to support substring matching",
                 mutest_string_value ("hello, world"),
                 mutest_to_contain, "o, w",
                 NULL);
  mutest_expect ("strings to support prefix matching",
                 mutest_string_value ("hello, world"),
                 mutest_to_start_with_string, "hello",
                 NULL);
  mutest_expect ("strings to support suffix matching",
                 mutest_string_value ("hello, world"),
                 mutest_to_end_with_string, "world",
                 NULL);
}

static void
value_types (mutest_suite_t *suite MUTEST_UNUSED)
{
  mutest_it ("allows checking scalars", check_scalars);
  mutest_it ("allows ordering of numbers", check_ordering);
  mutest_it ("allows checking ranges", check_ranges);
  mutest_it ("allows checking strings", check_string);
}

MUTEST_MAIN (
  mutest_describe ("Values", value_types);
)
