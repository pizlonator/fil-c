#include "test_common.h"

#include "wg14_signals/thrd_signal_handle.h"

#include <errno.h>
#include <string.h>

#ifdef __FILC__
#define SIGNAL_TO_USE SIGUSR1
#else
#define SIGNAL_TO_USE SIGILL
#endif

struct shared_t
{
  int count_decider, count_recovery;
};

static union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
sigill_recovery_func(const struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) *
                     rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->count_recovery++;
  return rsi->value;
}
static enum WG14_SIGNALS_PREFIX(thrd_signal_decision_t)
sigill_decider_func(struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) * rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->count_decider++;
  return WG14_SIGNALS_PREFIX(thrd_signal_decision_invoke_recovery);  // handled
}
static union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
sigill_func(union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) value)
{
  WG14_SIGNALS_PREFIX(thrd_signal_raise)
  (SIGNAL_TO_USE, WG14_SIGNALS_NULLPTR, WG14_SIGNALS_NULLPTR);
  return value;
}

int main(void)
{
  int ret = 0;
  void *handlers =
  WG14_SIGNALS_PREFIX(modern_signals_install)(WG14_SIGNALS_NULLPTR, 0);
  if(handlers == WG14_SIGNALS_NULLPTR)
  {
    fprintf(stderr, "FATAL: modern_signals_install() failed with %s\n",
            strerror(errno));
    return 1;
  }

  puts("Test thread local handling ...");
  {
    struct shared_t shared = {.count_decider = 0, .count_recovery = 0};
    union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
    value = {.ptr_value = &shared};
    sigset_t guarded;
    sigemptyset(&guarded);
    sigaddset(&guarded, SIGNAL_TO_USE);
    WG14_SIGNALS_PREFIX(thrd_signal_invoke)
    (&guarded, sigill_func, sigill_recovery_func, sigill_decider_func, value);
    CHECK(shared.count_decider == 1);
    CHECK(shared.count_recovery == 1);
  }

  puts("Test global handling ...");
  {
    struct shared_t shared = {.count_decider = 0, .count_recovery = 0};
    union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
    value = {.ptr_value = &shared};
    sigset_t guarded;
    sigemptyset(&guarded);
    sigaddset(&guarded, SIGNAL_TO_USE);
    void *sigill_decider = WG14_SIGNALS_PREFIX(signal_decider_create)(
    &guarded, false, sigill_decider_func, value);
    CHECK(WG14_SIGNALS_PREFIX(thrd_signal_raise)(
    SIGNAL_TO_USE, WG14_SIGNALS_NULLPTR, WG14_SIGNALS_NULLPTR));
    CHECK(shared.count_decider == 1);
    CHECK(shared.count_recovery == 0);
    WG14_SIGNALS_PREFIX(signal_decider_destroy(sigill_decider));
  }

  CHECK(WG14_SIGNALS_PREFIX(modern_signals_uninstall)(handlers) == 0);
  printf("Exiting main with result %d ...\n", ret);
  return ret;
}
