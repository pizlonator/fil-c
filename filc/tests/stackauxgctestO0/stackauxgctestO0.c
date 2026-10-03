/* Regression test for a -O0 GC rooting bug. Explicit stack auxes (the hidden
   capability slots for pointer-holding locals) all shared one lowers slot, so
   the collector only saw the stack aux of whichever local was initialized
   last. Objects pointed to by the other locals were not roots, so the GC
   collected them while they were still live.

   This test is compiled at -O0 (see optFlags in the manifest), where allocas
   do not have lifetime markers, so every local is an always-live explicit
   local. Each of objs, more, and weaks must be its own GC root set. */

#include <stdfil.h>
#include <stdlib.h>
#include <stdio.h>

#define N 10

struct big {
    long tag;
    char pad[16000];
};

int main(void)
{
    struct big* objs[N];
    struct big* more[N];
    zweak* weaks[N];
    unsigned t;

    for (t = N; t--;) {
        objs[t] = malloc(sizeof(struct big));
        objs[t]->tag = (long)t;
        more[t] = malloc(sizeof(struct big));
        more[t]->tag = 1000 + (long)t;
        weaks[t] = zweak_new(objs[t]);
    }

    /* Allocate a lot of garbage so the collector has work to do while all of
       objs, more, and weaks are live. */
    for (t = 1000; t--;) {
        struct big* volatile garbage = malloc(sizeof(struct big));
        garbage->tag = -1;
    }

    zgc_request_and_wait();

    for (t = N; t--;) {
        ZASSERT(zweak_get(weaks[t]) == objs[t]);
        ZASSERT(objs[t]->tag == (long)t);
        ZASSERT(more[t]->tag == 1000 + (long)t);
    }

    printf("Success!\n");
    return 0;
}
