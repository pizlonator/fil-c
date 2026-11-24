#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>

union semun {
    unsigned short *array;
};

static void assert_oob_traps(int semid, int cmd)
{
    fflush(NULL);
    pid_t pid = fork();
    ZASSERT(pid >= 0);
    if (pid == 0) {
        /* Fil-C rounds allocation upper bounds to 16 bytes (FILC_MINALIGN),
           so allocate 16 bytes (8 shorts) against sem_nsems=16 (32 bytes). */
        unsigned short *tiny = (unsigned short *)calloc(8, sizeof(unsigned short));
        ZASSERT(tiny != NULL);
        union semun arg;
        arg.array = tiny;
        int rc = semctl(semid, 0, cmd, arg);
        printf("UNEXPECTED_SEMCTL_OOB_SURVIVED cmd=%d rc=%d errno=%d\n", cmd, rc, errno);
        fflush(stdout);
        _exit(0);
    }

    int status = 0;
    ZASSERT(waitpid(pid, &status, 0) == pid);
    ZASSERT(WIFSIGNALED(status));
}

int main(void)
{
    int semid = semget(IPC_PRIVATE, 16, IPC_CREAT | 0600);
    ZASSERT(semid >= 0);

    assert_oob_traps(semid, GETALL);
    assert_oob_traps(semid, SETALL);

    ZASSERT(semctl(semid, 0, IPC_RMID) == 0);
    printf("semctl oob trapped ok\n");
    return 0;
}
