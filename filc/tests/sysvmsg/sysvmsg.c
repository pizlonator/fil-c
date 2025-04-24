#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stddef.h>
#include <sys/types.h>

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

static int msgid;

static void child(void)
{
    struct msgbuf* buf = malloc(offsetof(struct msgbuf, mtext) + strlen("hello, world!\n") + 1);
    buf->mtype = 666;
    strcpy(buf->mtext, "hello, world!\n");

    ASSERT(!msgsnd(msgid, buf, strlen("hello, world!\n") + 1, 0));
}

static void parent(void)
{
    struct msgbuf* buf = malloc(offsetof(struct msgbuf, mtext) + strlen("hello, world!\n") + 1);
    ASSERT(msgrcv(msgid, buf, strlen("hello, world!\n") + 1, 666, 0)
           == strlen("hello, world!\n") + 1);
    printf("%s\n", buf->mtext);
    ASSERT(buf->mtype == 666);
    ASSERT(!strcmp(buf->mtext, "hello, world!\n"));
}
      
int main()
{
    msgid = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    ASSERT(msgid >= 0);

    int fork_result = fork();
    ASSERT(fork_result >= 0);
    if (!fork_result)
        child();
    else {
        parent();
        int status;
        int wait_result = wait(&status);
        ASSERT(!msgctl(msgid, IPC_RMID, NULL));
        ASSERT(wait_result == fork_result);
        ASSERT(WIFEXITED(status));
        ASSERT(!WEXITSTATUS(status));
    }
    return 0;
}

