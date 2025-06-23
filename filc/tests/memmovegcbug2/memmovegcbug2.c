#include <stdlib.h>
#include <stdfil.h>
#include <string.h>
#include <pthread.h>
#include <filc_test_support.h>

#define SIZE 10000000
#define REPEAT 5

struct foo {
    int x;
};

static struct foo** one;
static struct foo** two;

static void do_test(void)
{
    one = (struct foo**)malloc(sizeof(struct foo*) * SIZE);
    two = (struct foo**)malloc(sizeof(struct foo*) * SIZE);

    unsigned i;
    for (i = SIZE; i--;) {
        one[i] = malloc(sizeof(struct foo));
        one[i]->x = i * 666;
    }

    zprintf("one[0]=>x = %d\n", one[0]->x);

    memmove(two, one, sizeof(struct foo*) * SIZE);
    
    free(one);
    zgc_request_and_wait();

    for (i = SIZE; i--;) {
        if (two[i]->x != i * 666) {
            zprintf("two[%u]->x = %d\n", i, two[i]->x);
            ZASSERT(two[i]->x == i * 666);
        }
    }
}

int main()
{
    unsigned repeat = REPEAT;

    if (zgc_is_scribbling() || zgc_is_verifying())
        repeat /= 2;
    
    unsigned i;
    for (i = repeat; i--;)
        do_test();
    
    return 0;
}

