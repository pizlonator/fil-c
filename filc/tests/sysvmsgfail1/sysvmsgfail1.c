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
#include <stdfil.h>
#include <inttypes.h>

static int msgid;

static void child(void)
{
    struct msgbuf* buf = malloc(offsetof(struct msgbuf, mtext));
    buf->mtype = 666;
    int result = msgsnd(msgid, buf, UINTPTR_MAX, 0);
    int my_errno = errno;
    zprintf("should not get here; result = %d, error = %s\n", result, strerror(my_errno));
}

int main()
{
    msgid = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    ZASSERT(msgid >= 0);

    int fork_result = fork();
    ZASSERT(fork_result >= 0);
    if (!fork_result)
        child();
    else {
        int status;
        int wait_result = wait(&status);
        ZASSERT(!msgctl(msgid, IPC_RMID, NULL));
        ZASSERT(wait_result == fork_result);
        ZASSERT(WIFEXITED(status));
        ZASSERT(WEXITSTATUS(status) == 42);
    }
    return 0;
}

