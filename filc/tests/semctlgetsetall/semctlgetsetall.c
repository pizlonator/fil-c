#include <stdfil.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main(void)
{
    int semid = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
    ZASSERT(semid >= 0);

    unsigned short set_vals[3] = { 11, 22, 33 };
    unsigned short get_vals[3] = { 0, 0, 0 };
    union semun arg;

    arg.array = set_vals;
    ZASSERT(semctl(semid, 0, SETALL, arg) == 0);

    arg.array = get_vals;
    ZASSERT(semctl(semid, 0, GETALL, arg) == 0);

    ZASSERT(semctl(semid, 0, IPC_RMID) == 0);
    ZASSERT(get_vals[0] == 11);
    ZASSERT(get_vals[1] == 22);
    ZASSERT(get_vals[2] == 33);

    printf("semctl getall setall ok\n");
    return 0;
}
