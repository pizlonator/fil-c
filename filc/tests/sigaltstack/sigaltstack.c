/* Registers an alternate signal stack, reads it back, disables it, and -- the
   point of the test -- exercises SA_ONSTACK handler delivery. A handler that
   asked for SA_ONSTACK runs on the alternate stack where the runtime libc has
   ucontext (glibc); where it does not (Fil-C's musl), the deferred handler runs
   on the normal Fil-C stack, still memory-safe. Either way the handler runs and
   can do real work, and an ordinary (non-SA_ONSTACK) handler never runs on the
   alternate stack.

   "On the alternate stack" is detected the POSIX way: from within the handler,
   sigaltstack(NULL, &cur) reports SS_ONSTACK while the alternate stack is in
   use. (Checking the address of a local would not work here: Fil-C may place a
   large local array in the heap rather than on the machine stack.) */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdfil.h>

static char* g_alt_base;
static size_t g_alt_size;

static int currently_on_alt_stack(void)
{
    stack_t cur;
    memset(&cur, 0, sizeof(cur));
    if (sigaltstack(NULL, &cur) != 0)
        return 0;
    return (cur.ss_flags & SS_ONSTACK) != 0;
}

static volatile int g_onstack_ran;
static volatile int g_onstack_on_alt;
static volatile int g_onstack_usable;

static void onstack_handler(int sig)
{
    (void)sig;
    /* Do real work on the stack we were handed: a page-sized frame, written end
       to end. If the alternate stack's height checks were not wired up, running
       this would trap rather than proceed. */
    volatile char probe[4096];
    memset((void*)probe, 0x5a, sizeof(probe));
    g_onstack_usable = (probe[0] == 0x5a && probe[sizeof(probe) - 1] == 0x5a);
    g_onstack_on_alt = currently_on_alt_stack();
    g_onstack_ran = 1;
}

static volatile int g_normal_ran;
static volatile int g_normal_on_alt;

static void normal_handler(int sig)
{
    (void)sig;
    g_normal_on_alt = currently_on_alt_stack();
    g_normal_ran = 1;
}

/* Fil-C delivers signals at a safe point rather than in the async context, so
   spin over cheap syscalls (each a safe point) until the handler has run. */
static void drain_until(volatile int* flag)
{
    int spins;
    for (spins = 0; spins < 1000000 && !*flag; ++spins)
        (void)getpid();
    ZASSERT(*flag);
}

int main(void)
{
    /* Register / read back / disable. */
    g_alt_size = 1 << 17; /* 128 KiB: above Fil-C's alternate-stack minimum. */
    g_alt_base = (char*)malloc(g_alt_size);
    ZASSERT(g_alt_base);

    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = g_alt_base;
    ss.ss_size = g_alt_size;
    ss.ss_flags = 0;
    ZASSERT(sigaltstack(&ss, NULL) == 0);

    stack_t old;
    memset(&old, 0, sizeof(old));
    ZASSERT(sigaltstack(NULL, &old) == 0);
    ZASSERT(old.ss_size == g_alt_size);
    ZASSERT(!(old.ss_flags & SS_DISABLE));
    ZASSERT(old.ss_sp == g_alt_base);

    /* A handler that asked for SA_ONSTACK must run on the alternate stack. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = onstack_handler;
    sa.sa_flags = SA_ONSTACK;
    ZASSERT(sigemptyset(&sa.sa_mask) == 0);
    ZASSERT(sigaction(SIGUSR1, &sa, NULL) == 0);

    g_onstack_ran = g_onstack_on_alt = g_onstack_usable = 0;
    ZASSERT(raise(SIGUSR1) == 0);
    drain_until(&g_onstack_ran);
    ZASSERT(g_onstack_usable);
#ifdef __GLIBC__
    /* glibc's runtime libc has ucontext, so the SA_ONSTACK handler runs on the
       alternate stack. */
    ZASSERT(g_onstack_on_alt);
#else
    /* Fil-C's musl runtime libc has no ucontext (makecontext/swapcontext), so the
       deferred SA_ONSTACK handler runs on the normal Fil-C stack. It still runs and
       is memory-safe -- only the stack switch is unavailable. */
    ZASSERT(!g_onstack_on_alt);
#endif

    /* A handler without SA_ONSTACK must NOT run on the alternate stack, even
       though one is registered. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = normal_handler;
    sa.sa_flags = 0;
    ZASSERT(sigemptyset(&sa.sa_mask) == 0);
    ZASSERT(sigaction(SIGUSR2, &sa, NULL) == 0);

    g_normal_ran = g_normal_on_alt = 0;
    ZASSERT(raise(SIGUSR2) == 0);
    drain_until(&g_normal_ran);
    ZASSERT(!g_normal_on_alt);

    /* Disable the alternate stack. */
    stack_t dis;
    memset(&dis, 0, sizeof(dis));
    dis.ss_flags = SS_DISABLE;
    ZASSERT(sigaltstack(&dis, NULL) == 0);

    free(g_alt_base);
    /* Raw values for humans; the pass/fail decision is the ZASSERTs above (the
       on-alt expectation is libc-dependent, so it is not asserted from output). */
    printf("sigaltstack: onstack ran=1 usable=%d on_alt=%d, normal off alt=%d\n",
           g_onstack_usable, g_onstack_on_alt, !g_normal_on_alt);
    printf("sigaltstack: all checks passed\n");
    return 0;
}
