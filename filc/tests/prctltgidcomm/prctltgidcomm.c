#include <pthread.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>

static void *worker_fn(void *arg)
{
    (void)arg;
    ZASSERT(prctl(PR_SET_NAME, "filc_worker_thr", 0UL, 0UL, 0UL) == 0);
    return NULL;
}

static void read_proc_comm(char *buf, size_t size)
{
    FILE *fp = fopen("/proc/self/comm", "r");
    if (!fp) {
        buf[0] = '\0';
        return;
    }
    ZASSERT(fgets(buf, (int)size, fp) != NULL);
    fclose(fp);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
}

int main(void)
{
    const char *want = "filc_tgid_ok";
    ZASSERT(prctl(PR_SET_NAME, want, 0UL, 0UL, 0UL) == 0);

    char got_prctl[16] = { 0 };
    ZASSERT(prctl(PR_GET_NAME, got_prctl, 0UL, 0UL, 0UL) == 0);
    ZASSERT(strcmp(got_prctl, want) == 0);

    char got_comm[32] = { 0 };
    read_proc_comm(got_comm, sizeof(got_comm));
    if (got_comm[0] != '\0')
        ZASSERT(strcmp(got_comm, want) == 0);

    /* Secondary thread PR_SET_NAME must not overwrite TGID leader comm. */
    pthread_t thr;
    ZASSERT(pthread_create(&thr, NULL, worker_fn, NULL) == 0);
    ZASSERT(pthread_join(thr, NULL) == 0);

    char got_comm_after[32] = { 0 };
    read_proc_comm(got_comm_after, sizeof(got_comm_after));
    if (got_comm_after[0] != '\0')
        ZASSERT(strcmp(got_comm_after, want) == 0);

    printf("prctl tgid comm ok\n");
    return 0;
}
