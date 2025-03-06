#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdfil.h>
#include <stdbool.h>
#include <filc_test_support.h>

static size_t num_nodes = 20000;
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

int main()
{
    if (!zgc_is_stw()) {
        num_nodes *= 2;
        repeat *= 2;
    }
    
    bool is_first_process = true;
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
            int status;
            int wait_result = wait(&status);
            ZASSERT(wait_result == pid);
            ZASSERT(WIFEXITED(status));
            ZASSERT(!WEXITSTATUS(status));
            break;
        }
        is_first_process = false;
        zprintf("Forked, now in pid = %d!\n", getpid());
        
        verify();
    }
    if (is_first_process)
        zprintf("Total victory!\n");
    else
        zprintf("Success!\n");
    return 0;
}

