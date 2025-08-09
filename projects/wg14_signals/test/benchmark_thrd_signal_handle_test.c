#include "test_common.h"

#include "ticks_clock.h"

#include "wg14_signals/thrd_signal_handle.h"

#include <errno.h>
#include <string.h>

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)

#ifdef __FILC__
#define SIGNAL_TO_USE SIGUSR1
#else
#define SIGNAL_TO_USE SIGILL
#endif


static union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
sigill_recovery_func(const struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) *
                     rsi)
{
  return rsi->value;
}
static enum WG14_SIGNALS_PREFIX(thrd_signal_decision_t)
sigill_decider_func(struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) * rsi)
{
  (void) rsi;
  return WG14_SIGNALS_PREFIX(thrd_signal_decision_invoke_recovery);  // handled
}
static union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
sigill_func(union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) value)
{
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

  puts("Preparing benchmark ...");
  {
    const ns_count begin = get_ns_count();
    ns_count end = begin;
    do
    {
    } while(end = get_ns_count(), end - begin < 1000000000);
  }
  sigset_t guarded;
  sigemptyset(&guarded);
  sigaddset(&guarded, SIGNAL_TO_USE);
  union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
  value = {.int_value = 0};
  const cpu_ticks_count ticks_per_sec = ticks_per_second();
  printf("There are %llu ticks per second.\n",
         (unsigned long long) ticks_per_sec);

  puts("Benchmarking thread local handling ...");
  {
    const ns_count begin = get_ns_count();
    ns_count end = begin;
    cpu_ticks_count ticks = 0, ops = 0;
    do
    {
      for(size_t n = 0; n < 65536; n++)
      {
        cpu_ticks_count s = get_ticks_count(memory_order_relaxed);
        WG14_SIGNALS_PREFIX(thrd_signal_invoke)
        (&guarded, sigill_func, sigill_recovery_func, sigill_decider_func,
         value);
        cpu_ticks_count e = get_ticks_count(memory_order_relaxed);
        ticks += e - s;
        ops++;
      }
    } while(end = get_ns_count(), end - begin < 3000000000);
    printf(
    "\nOn this platform (WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL "
    "= " STRINGIZE(WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL) "), thrd_signal_invoke() "
                                               "takes %f nanoseconds.\n\n",
    (double) ticks / ((double) ticks_per_sec / 1000000000.0) / (double) ops);
  }

  puts("Benchmarking global handling ...");
  {
    void *sigill_decider = WG14_SIGNALS_PREFIX(signal_decider_create)(
    &guarded, false, sigill_decider_func, value);
    const ns_count begin = get_ns_count();
    ns_count end = begin;
    cpu_ticks_count ticks = 0, ops = 0;
    do
    {
      for(size_t n = 0; n < 65536; n++)
      {
        cpu_ticks_count s = get_ticks_count(memory_order_relaxed);
        CHECK(WG14_SIGNALS_PREFIX(thrd_signal_raise)(
        SIGNAL_TO_USE, WG14_SIGNALS_NULLPTR, WG14_SIGNALS_NULLPTR));
        cpu_ticks_count e = get_ticks_count(memory_order_relaxed);
        ticks += e - s;
        ops++;
      }
    } while(end = get_ns_count(), end - begin < 3000000000);
    printf(
    "\nOn this platform (WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL "
    "= " STRINGIZE(WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL) "), invoking a globally "
                                               "installed decider "
                                               "takes %f nanoseconds.\n\n",
    (double) ticks / ((double) ticks_per_sec / 1000000000.0) / (double) ops);
    WG14_SIGNALS_PREFIX(signal_decider_destroy(sigill_decider));
  }

  CHECK(WG14_SIGNALS_PREFIX(modern_signals_uninstall)(handlers) == 0);
  printf("Exiting main with result %d ...\n", ret);
  return ret;
}
