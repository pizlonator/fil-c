#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <filc_test_support.h>
#include <inttypes.h>

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

static size_t num_nodes = 5000;
static size_t repeat = 2;

struct foo;
typedef struct foo foo;

struct foo {
    uintptr_t other;
    uintptr_t string;
};

static uintptr_t root;

static zexact_ptrtable* ptrtable;

static void add_node(void)
{
    foo* node = (foo*)malloc(sizeof(foo));
    node->other = root;
    char* str;
    asprintf(&str, "node = %p", node);
    node->string = zexact_ptrtable_encode(ptrtable, str);
    root = zexact_ptrtable_encode(ptrtable, node);
}

static void build(void)
{
    size_t index;
    for (index = num_nodes; index--;)
        add_node();
}

static void remove_half(void)
{
    uintptr_t* ptr;
    size_t index;
    for (ptr = &root, index = 0; *ptr; index++) {
        if ((index & 1))
            ptr = &((foo*)zexact_ptrtable_decode(ptrtable, *ptr))->other;
        else {
            foo* node = zexact_ptrtable_decode(ptrtable, *ptr);
            *ptr = node->other;
            free(node);
        }
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
    for (node = zexact_ptrtable_decode(ptrtable, root); node; node = zexact_ptrtable_decode(ptrtable, node->other)) {
        char* str;
        asprintf(&str, "node = %p", node);
        ASSERT(!strcmp(zexact_ptrtable_decode(ptrtable, node->string), str));
    }
}

int main()
{
    if (!zgc_is_stw()) {
        num_nodes *= 2;
        repeat *= 2;
    }

    ptrtable = zexact_ptrtable_new();
    
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

