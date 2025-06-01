#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <filc_test_support.h>

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

static size_t num_nodes = 20000;
static size_t repeat = 7;

struct foo;
typedef struct foo foo;

typedef foo* (*foo_getter)(void);

struct foo {
    foo_getter other_getter;
    char* string;
};

static struct foo* getter(void)
{
    return (struct foo*)zcallee_closure_data();
}

static foo_getter root_getter;

static void add_node(void)
{
    foo* node = (foo*)malloc(sizeof(foo));
    node->other_getter = zclosure_new(getter, root_getter());
    asprintf(&node->string, "node = %p", node);
    root_getter = zclosure_new(getter, node);
}

static void build(void)
{
    size_t index;
    for (index = num_nodes; index--;)
        add_node();
}

static void remove_half(void)
{
    foo_getter* ptr;
    size_t index;
    for (ptr = &root_getter, index = 0; (*ptr)(); index++) {
        if ((index & 1))
            ptr = &(*ptr)()->other_getter;
        else
            *ptr = zclosure_new(getter, (*ptr)()->other_getter());
    }
}

static void add_half(void)
{
    size_t index;
    for (index = num_nodes / 2; index--;)
        add_node();
}

static void verify(void)
{
    foo* node;
    for (node = root_getter(); node; node = node->other_getter()) {
        char* str;
        asprintf(&str, "node = %p", node);
        ASSERT(!strcmp(node->string, str));
    }
}

int main()
{
    if (!zgc_is_stw()) {
        num_nodes *= 2;
        repeat *= 2;
    }

    root_getter = zclosure_new(getter, NULL);
    
    build();
    verify();
    size_t index;
    for (index = 1; index <= repeat; ++index) {
        printf("Iteration %zu\n", index);
        remove_half();
        verify();
        add_half();
        verify();
    }
    printf("Success!\n");
    return 0;
}

