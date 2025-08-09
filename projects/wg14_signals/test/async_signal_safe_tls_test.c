#include "test_common.h"

#include "wg14_signals/current_thread_id.h"
#include "wg14_signals/tss_async_signal_safe.h"

static unsigned storage[2] = {5, 6};
static unsigned *storage_ptr = storage;
static int create(void **dest)
{
  *dest = storage_ptr++;
  printf("   creating value to %u in tid %lu\n", *(unsigned *) *dest,
         (unsigned long) WG14_SIGNALS_PREFIX(current_thread_id)());
  return 0;
}
static int destroy(void *dest)
{
  printf("   destroying value to %u in tid %lu\n", *(unsigned *) dest,
         (unsigned long) WG14_SIGNALS_PREFIX(current_thread_id)());
  return 0;
}

struct shared_t
{
  WG14_SIGNALS_PREFIX(tss_async_signal_safe) tls;
} shared;

static int thrfunc(void *x)
{
  (void) x;
  int ret = 0;
  printf("Initing TLS for worker thread ...\n");
  CHECK(-1 !=
        WG14_SIGNALS_PREFIX(tss_async_signal_safe_thread_init)(shared.tls));
  unsigned *val =
  (unsigned *) WG14_SIGNALS_PREFIX(tss_async_signal_safe_get)(shared.tls);
  if(val == WG14_SIGNALS_NULLPTR)
  {
    abort();
  }
  CHECK(*val == 6);
  return ret;
}

int main(void)
{
  int ret = 0;
  WG14_SIGNALS_PREFIX(thread_id_t)
  mytid = WG14_SIGNALS_PREFIX(current_thread_id)();
  printf("Main thread tid = %lu\n", (unsigned long) mytid);
  CHECK(0 != mytid);
  struct WG14_SIGNALS_PREFIX(tss_async_signal_safe_attr)
  attr = {.create = create, .destroy = destroy};
  printf("Creating TLS ...\n");
  CHECK(-1 !=
        WG14_SIGNALS_PREFIX(tss_async_signal_safe_create)(&shared.tls, &attr));
  printf("Initing TLS for main thread ...\n");
  CHECK(-1 !=
        WG14_SIGNALS_PREFIX(tss_async_signal_safe_thread_init)(shared.tls));
  thrd_t thread;
  thrd_create(&thread, thrfunc, &shared);
  unsigned *val =
  (unsigned *) WG14_SIGNALS_PREFIX(tss_async_signal_safe_get)(shared.tls);
  if(val == WG14_SIGNALS_NULLPTR)
  {
    abort();
  }
  CHECK(*val == 5);
  printf("Joining thread ...\n");
  int res = 0;
  thrd_join(thread, &res);
  CHECK(res == 0);

  printf("Destroying TLS ...\n");
  CHECK(-1 != WG14_SIGNALS_PREFIX(tss_async_signal_safe_destroy)(shared.tls));
  printf("Exiting main with result %d ...\n", ret);
  return ret;
}
