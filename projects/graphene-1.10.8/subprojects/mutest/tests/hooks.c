#include <mutest.h>

static struct {
  int before_called;
  int before_each_counter;
  int after_each_counter;
  int after_called;
} fixture = {
  .before_called = -1,
  .before_each_counter = 0,
  .after_each_counter = 0,
  .after_called = -1,
};

static void
before_hook (void)
{
  fixture.before_called = 1;
  fixture.after_called = -1;
}

static void
before_each_hook (void)
{
  fixture.before_each_counter += 1;
}

static void
after_each_hook (void)
{
  fixture.after_each_counter += 1;
}

static void
after_hook (void)
{
  fixture.before_called = -1;
  fixture.after_called = 1;
}

static void
called_before_hook (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("before_called to have been called yet",
                 mutest_int_value (fixture.before_called),
                 mutest_to_be, 1,
                 NULL);
}

static void
called_after_hook (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("after_called to not have been called",
                 mutest_int_value (fixture.after_called),
                 mutest_to_be, -1,
                 NULL);
}

static void
calls_before_each_hook (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("before_each_counter to have been called at least once",
                 mutest_int_value (fixture.before_each_counter),
                 mutest_not, mutest_to_be, 0,
                 NULL);
}

static void
calls_after_each_hook (mutest_spec_t *spec MUTEST_UNUSED)
{
  mutest_expect ("before_each_counter to have been called at least once",
                 mutest_int_value (fixture.after_each_counter),
                 mutest_not, mutest_to_be, 0,
                 NULL);
}

static void
hooks_spec (mutest_suite_t *suite MUTEST_UNUSED)
{
  mutest_it ("calls before hook before the spec", called_before_hook);
  mutest_it ("didn't call after hook yet", called_after_hook);
}

static void
hooks_suite (mutest_suite_t *suite MUTEST_UNUSED)
{
  mutest_before_each (before_each_hook);
  mutest_after_each (after_each_hook);

  mutest_it ("calls before_each hook", calls_before_each_hook);
  mutest_it ("calls after_each hook", calls_after_each_hook);
}

MUTEST_MAIN (
  mutest_before (before_hook);
  mutest_after (after_hook);

  mutest_describe ("Suite Hooks", hooks_suite);
  mutest_describe ("Spec Hooks", hooks_spec);
)
