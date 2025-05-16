#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <filc_test_support.h>
#include <stdbool.h>

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

static size_t num_nodes = 20000;
static size_t repeat = 10;
static const bool verbose = false;

struct foo;
typedef struct foo foo;

static zweak_map* other_map;
static zweak_map* string_map;

struct foo {
};

static void add_node(void)
{
    foo* node = (foo*)malloc(sizeof(foo));
    zweak_map_set(other_map, node, zweak_map_get(other_map, NULL));
    char* string;
    asprintf(&string, "node = %p", node);
    if (verbose)
        zprintf("setting string %p->%p\n", node, string);
    zweak_map_set(string_map, node, string);
    zweak_map_set(other_map, NULL, node);
}

static void build(void)
{
    size_t index;
    for (index = num_nodes; index--;)
        add_node();
}

static void remove_half(void)
{
    foo* key;
    size_t index;
    for (key = NULL, index = 0; zweak_map_get(other_map, key); index++) {
        foo* next = zweak_map_get(other_map, key);
        if ((index & 1))
            key = next;
        else
            zweak_map_set(other_map, key, zweak_map_get(other_map, next));
    }
}

static void add_half(void)
{
    size_t index;
    for (index = num_nodes / 2; index--;)
        add_node();
}

static void verify(unsigned expected_num_nodes)
{
    foo* node;
    unsigned count = 0;
    for (node = zweak_map_get(other_map, NULL); node; node = zweak_map_get(other_map, node)) {
        char* str;
        asprintf(&str, "node = %p", node);
        if (verbose)
            zprintf("gettin string for %p\n", node);
        ASSERT(!strcmp(zweak_map_get(string_map, node), str));
        count++;
    }
    if (count != expected_num_nodes) {
        zprintf("expected num nodes: %u\n", expected_num_nodes);
        zprintf("got num nodes: %u\n", count);
    }
    ASSERT(count == expected_num_nodes);
}

int main()
{
    other_map = zweak_map_new();
    string_map = zweak_map_new();
    zprintf("other_map = %P\n", other_map);
    zprintf("string_map = %P\n", string_map);
    
    if (!zgc_is_stw()) {
        num_nodes *= 2;
        repeat *= 2;
    }
    
    build();
    zgc_request_and_wait();
    verify(num_nodes);
    size_t index;
    for (index = 1; index <= repeat; ++index) {
        printf("Iteration %zu\n", index);
        remove_half();
        verify(num_nodes / 2);
        add_half();
        verify(num_nodes);
    }
    printf("Success!\n");
    return 0;
}

