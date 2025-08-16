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

static size_t num_nodes = 40000;
static size_t repeat = 10;

struct foo;
typedef struct foo foo;

struct foo {
    foo* other;
    char* string;
};

static foo* root;

static void add_node(void)
{
    foo* node = (foo*)malloc(sizeof(foo));
    foo tmp;
    tmp.other = root;
    asprintf(&tmp.string, "node = %p", node);
    zmemmove_builtin(node, &tmp, sizeof(foo));
    root = node;
}

static void build(void)
{
    size_t index;
    for (index = num_nodes; index--;)
        add_node();
}

static void remove_half(void)
{
    foo** ptr;
    size_t index;
    for (ptr = &root, index = 0; *ptr; index++) {
        if ((index & 1))
            ptr = &(*ptr)->other;
        else
            zmemmove_builtin(ptr, &(*ptr)->other, sizeof(foo*));
    }
}

static void add_half(void)
{
    size_t index;
    for (index = num_nodes / 2; index--;)
        add_node();
}

static size_t verify(void)
{
    foo* node;
    size_t count = 0;
    for (node = root; node;) {
        foo tmp;
        zmemmove_builtin(&tmp, node, sizeof(foo));
        char* str;
        asprintf(&str, "node = %p", node);
        ASSERT(!strcmp(tmp.string, str));
        node = tmp.other;
        count++;
    }
    return count;
}

int main()
{
    if (!zgc_is_stw()) {
        num_nodes *= 2;
        repeat *= 2;
    }
    
    build();
    ZASSERT(verify() == num_nodes);
    size_t index;
    for (index = 1; index <= repeat; ++index) {
        printf("Iteration %zu\n", index);
        remove_half();
        ZASSERT(verify() == num_nodes / 2);
        add_half();
        ZASSERT(verify() == num_nodes);
    }
    printf("Success!\n");
    return 0;
}

