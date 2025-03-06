#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdfil.h>
#include <stdbool.h>
#include <pthread.h>
#include <filc_test_support.h>

static size_t num_nodes = 10000;
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
    node->other = root;
    asprintf(&node->string, "node = %p", node);
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
            *ptr = (*ptr)->other;
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
    for (node = root; node; node = node->other) {
        char* str;
        asprintf(&str, "node = %p", node);
        ZASSERT(!strcmp(node->string, str));
    }
}

static foo* _Atomic thread_node;
static bool thread_should_stop;

static void* thread_main(void* arg)
{
    while (!thread_should_stop) {
        foo* node = (foo*)malloc(sizeof(foo));
        node->other = NULL;
        asprintf(&node->string, "thread node = %p", node);
        zstore_store_fence();
        thread_node = node;
    }
    return NULL;
}

int main()
{
    if (!zgc_is_stw()) {
        num_nodes *= 4;
        repeat *= 2;
    }
    
    bool is_first_process = true;
    pthread_t ignored;
    pthread_create(&ignored, NULL, thread_main, NULL);
    build();
    verify();
    size_t index;
    for (index = 1; index <= repeat; ++index) {
        zprintf("Iteration %zu\n", index);
        remove_half();
        verify();
        add_half();

        zprintf("Forking!\n");
        int pid = fork();
        ZASSERT(pid >= 0);
        if (pid) {
            /* Make sure that the thread runs at all. */
            while (!thread_node)
                zfence();
            thread_should_stop = true;
            foo* current_thread_node = thread_node;
            zprintf("thread_node = %p, string = %s\n", current_thread_node, current_thread_node->string);
            char* expected_string;
            asprintf(&expected_string, "thread node = %p", current_thread_node);
            ZASSERT(!strcmp(expected_string, current_thread_node->string));
            int status;
            int wait_result = wait(&status);
            ZASSERT(wait_result == pid);
            ZASSERT(WIFEXITED(status));
            ZASSERT(!WEXITSTATUS(status));
            break;
        }
        is_first_process = false;
        thread_node = NULL;
        ZASSERT(!thread_should_stop);
        zprintf("Forked, now in pid = %d!\n", getpid());
        pthread_create(&ignored, NULL, thread_main, NULL);
        
        verify();
    }
    if (is_first_process)
        zprintf("Total victory!\n");
    else
        zprintf("Success!\n");
    return 0;
}

