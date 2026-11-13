#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FILC_HAS_ZCALL_ON_FRESH_STACK
#error "no zcall_on_fresh_stack"
#endif

#define CHUNK (512ul * 1024)
#define BASE (0x3c1000000000ul)
#define LENGTH (2 * CHUNK)

#define CHECK(x) do { \
    if (!(x)) { \
        printf("FAIL %s:%d: %s (errno = %d)\n", __FILE__, __LINE__, #x, errno); \
        exit(1); \
    } \
} while (0)

/* What a process keeps of a mapping that it reopens: a pointer in the heap. */
struct holder {
    char* mapping;
    int answer;
    char* text;
    int fresh;
};

static char* reserve_with_retries(int tries)
{
    char* p = NULL;
    int try;
    for (try = 0; try < tries && !p; try++) {
        p = zgc_reserve_at(BASE, LENGTH);
        if (!p)
            zgc_wait(zgc_request_fresh());
    }
    return p;
}

/* In a function of its own, so that its frame, which kept the pointer that it passed, is gone. */
static __attribute__((noinline)) void unmap_holder(struct holder* holder)
{
    CHECK(!munmap(holder->mapping, LENGTH));
    holder->mapping = NULL;
}

/* The mapping goes away and comes back at the same address, as when a process reopens its file. */
static int remap(struct holder* holder)
{
    unmap_holder(holder);
    char* again = reserve_with_retries(5);
    if (!again)
        return 0;
    CHECK(!again[0] && !again[CHUNK]);
    CHECK(!munmap(again, LENGTH));
    return 1;
}

/* The callback of zcall_on_fresh_stack. Everything that it uses comes through its argument. */
static void* reopen_main(void* arg)
{
    struct holder* holder = (struct holder*)arg;
    int ok = remap(holder);
    /* What the argument reaches is alive. */
    _exit(ok && holder->answer == 42 && !strcmp(holder->text, "alive") ? 0 : 10);
    return NULL;
}

/* A caller with a frame that has a pointer to the mapping, and uses it after the call. With a plain
   call, that keeps the unmapped range alive. */
static __attribute__((noinline)) void reopen_with_live_frame(struct holder* holder, int fresh)
{
    char* volatile held = reserve_with_retries(8);
    CHECK(held);
    held[0] = 1;
    holder->mapping = (char*)held;

    if (fresh)
        zcall_on_fresh_stack(reopen_main, holder);
    /* A plain call: the frame still has the pointer, and uses it below. */
    int ok = remap(holder);
    CHECK(held != NULL);
    _exit(ok ? 20 : 0);
}

static void run(int fresh, int expected_status)
{
    struct holder* holder = malloc(sizeof(*holder));
    CHECK(holder);
    holder->answer = 42;
    holder->text = strdup("alive");
    int pid = fork();
    CHECK(pid >= 0);
    if (!pid) {
        reopen_with_live_frame(holder, fresh);
        _exit(30);
    }
    int status;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == expected_status);
}

int main(void)
{
    /* Without it the reopen cannot get the address back, while the frame holds the pointer. */
    run(0, 0);

    /* With it the frame is gone, and the reopen gets the address back. */
    run(1, 0);

    /* A callback that returns ends the process with status 0. */
    int pid = fork();
    CHECK(pid >= 0);
    if (!pid) {
        zcall_on_fresh_stack((void* (*)(void*))getpid, NULL);
    }
    int status;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);

    printf("freshstack ok\n");
    return 0;
}
