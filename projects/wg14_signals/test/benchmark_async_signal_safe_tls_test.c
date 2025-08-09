#include "test_common.h"

#include "ticks_clock.h"

#include "wg14_signals/tss_async_signal_safe.h"

#include <stdatomic.h>

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)

static unsigned storage[2] = {5, 6};
static unsigned *storage_ptr = storage;
static int create(void **dest)
{
  *dest = storage_ptr++;
  return 0;
}
static int destroy(void *dest)
{
  (void) dest;
  return 0;
}

struct shared_t
{
  WG14_SIGNALS_PREFIX(tss_async_signal_safe) tls;
} shared;

int main(void)
{
  int ret = 0;
  struct WG14_SIGNALS_PREFIX(tss_async_signal_safe_attr)
  attr = {.create = create, .destroy = destroy};
  CHECK(-1 !=
        WG14_SIGNALS_PREFIX(tss_async_signal_safe_create)(&shared.tls, &attr));
  CHECK(-1 !=
        WG14_SIGNALS_PREFIX(tss_async_signal_safe_thread_init)(shared.tls));
  {
    volatile unsigned *val =
    (unsigned *) WG14_SIGNALS_PREFIX(tss_async_signal_safe_get)(shared.tls);
    if(val == WG14_SIGNALS_NULLPTR)
    {
      abort();
    }
  }
  puts("Preparing benchmark ...");
  {
    const ns_count begin = get_ns_count();
    ns_count end = begin;
    do
    {
    } while(end = get_ns_count(), end - begin < 1000000000);
  }
  const cpu_ticks_count ticks_per_sec = ticks_per_second();
  printf("There are %llu ticks per second.\n",
         (unsigned long long) ticks_per_sec);
  puts("Running benchmark ...");
  const ns_count begin = get_ns_count();
  ns_count end = begin;
  cpu_ticks_count ticks = 0, ops = 0;
  do
  {
    for(size_t n = 0; n < 65536; n++)
    {
      cpu_ticks_count s = get_ticks_count(memory_order_relaxed);
      volatile unsigned *val =
      (unsigned *) WG14_SIGNALS_PREFIX(tss_async_signal_safe_get)(shared.tls);
      (void) val;
      cpu_ticks_count e = get_ticks_count(memory_order_relaxed);
      ticks += e - s;
      ops++;
    }
  } while(end = get_ns_count(), end - begin < 3000000000);
  printf(
  "\nOn this platform (WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL "
  "= " STRINGIZE(WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL) "), tss_async_signal_safe_get() "
                                             "takes %f nanoseconds.\n\n",
  (double) ticks / ((double) ticks_per_sec / 1000000000.0) / (double) ops);

  CHECK(-1 != WG14_SIGNALS_PREFIX(tss_async_signal_safe_destroy)(shared.tls));
  printf("Exiting main with result %d ...\n", ret);
  return ret;
}
