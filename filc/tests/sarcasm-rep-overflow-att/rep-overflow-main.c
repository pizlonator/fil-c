#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void rep_movsq_ov(void *dst, void *src, size_t n);
void rep_movsl_ov(void *dst, void *src, size_t n);
void rep_movsw_ov(void *dst, void *src, size_t n);
void rep_stosq_ov(void *dst, size_t n, long v);

// Each case must panic. Cases run in forked children using stack buffers
// only (no allocator state matters post-fork); the parent verifies every
// child failed to return.
static void run_case(const char *which) {
    // Small valid buffers: the huge count must trap in the lowering's
    // count-overflow guard, never reach the hardware copy.
    long src[4] = {1, 2, 3, 4};
    long dst[4] = {0, 0, 0, 0};
    if (!strcmp(which, "movsq"))
        rep_movsq_ov(dst, src, (size_t)1 << 61); /* qwords: N wraps to 0 */
    else if (!strcmp(which, "movsl"))
        rep_movsl_ov(dst, src, (size_t)1 << 62); /* dwords: N wraps to 0 */
    else if (!strcmp(which, "movsw"))
        rep_movsw_ov(dst, src, (size_t)1 << 63); /* words: N wraps to 0 */
    else if (!strcmp(which, "stosq"))
        rep_stosq_ov(dst, (size_t)1 << 61, 0);
    else {
        printf("unknown case %s\n", which);
        _exit(2);
    }
    printf("NOT REACHED %s\n", which);
    _exit(0); /* Bug: the overflowing copy did not trap. */
}

int main(int argc, char **argv) {
    if (argc > 1) {
        /* Debugging escape hatch: run one case in-process (it should panic). */
        run_case(argv[1]);
        return 0;
    }
    const char *cases[] = {"movsq", "movsl", "movsw", "stosq"};
    int failed = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            run_case(cases[i]);
            _exit(0);
        }
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 42) {
            printf("case %s panicked as expected\n", cases[i]);
        } else {
            /* exit 0 means the copy returned (unsound); a signal means the
               hardware copy ran OOB instead of trapping cleanly. Either way
               the overflow guard failed to fire. */
            printf("case %s did NOT panic cleanly (BUG)\n", cases[i]);
            failed = 1;
        }
    }
    if (failed) {
        printf("rep overflow FAILED\n");
        return 1;
    }
    printf("rep overflow ok\n");
    return 0;
}
