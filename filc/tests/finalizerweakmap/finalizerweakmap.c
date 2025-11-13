#include <stdfil.h>
#include <string.h>

static zgc_finq* finq;
static zweak_map* weak_map;

static __attribute__((noinline)) char* create(void)
{
    char* p = strdup("hello");
    char** q = zgc_finq_alloc(finq, sizeof(char*));
    *q = p;
    zweak_map_set(weak_map, p, q);
    return p;
}

static __attribute__((noinline)) void step1(void)
{
    char* p = create();
    zgc_request_and_wait();
    ZASSERT(zweak_map_get(weak_map, p));
    ZASSERT(zweak_map_size(weak_map) == 1);
    ZASSERT(!zgc_finq_poll(finq));
}

int main()
{
    finq = zgc_finq_new();
    weak_map = zweak_map_new();
    step1();
    zgc_request_and_wait();
    ZASSERT(!zweak_map_size(weak_map));
    char** q = zgc_finq_poll(finq);
    ZASSERT(q);
    ZASSERT(!strcmp(*q, "hello"));
    ZASSERT(!zgc_finq_poll(finq));
    zgc_request_and_wait();
    ZASSERT(!zgc_finq_poll(finq));
    return 0;
}

