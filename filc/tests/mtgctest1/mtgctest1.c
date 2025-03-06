#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
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
static size_t repeat = 4;
static size_t num_threads = 10;

struct foo;
typedef struct foo foo;

struct foo {
    foo* other;
    char* string;
};

static void add_node(foo** root)
{
    foo* node = (foo*)malloc(sizeof(foo));
    node->other = *root;
    asprintf(&node->string, "node = %p", node);
    *root = node;
}

static void build(foo** root)
{
    size_t index;
    for (index = num_nodes; index--;)
        add_node(root);
}

static void remove_half(foo** root)
{
    foo** ptr;
    size_t index;
    for (ptr = root, index = 0; *ptr; index++) {
        if ((index & 1))
            ptr = &(*ptr)->other;
        else
            *ptr = (*ptr)->other;
    }
}

static void add_half(foo** root)
{
    size_t index;
    for (index = num_nodes / 2; index--;)
        add_node(root);
}

static void verify(foo** root)
{
    foo* node;
    for (node = *root; node; node = node->other) {
        char* str;
        asprintf(&str, "node = %p", node);
        ASSERT(!strcmp(node->string, str));
    }
}

static void* thread_main(void* arg)
{
    ASSERT(!arg);

    foo* root = NULL;
    
    build(&root);
    verify(&root);
    size_t index;
    for (index = 1; index <= repeat; ++index) {
        printf("Iteration %zu\n", index);
        remove_half(&root);
        verify(&root);
        add_half(&root);
        verify(&root);
    }
    
    return NULL;
}

int main()
{
    if (!zgc_is_stw()) {
        repeat *= 6;
        num_nodes *= 2;
    }
    
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    size_t index;
    for (index = num_threads; index--;)
        ASSERT(!pthread_create(threads + index, NULL, thread_main, NULL));
    for (index = num_threads; index--;)
        ASSERT(!pthread_join(threads[index], NULL));
    
    printf("Success!\n");
    return 0;
}

