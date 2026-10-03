/* PR_GET_PDEATHSIG must store the current death signal through its int
   pointer (it is 0 unless our parent set one), and the speculation-control
   prctls must pass through to the kernel instead of being issued as
   PR_SET_NO_NEW_PRIVS. */

#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <sys/prctl.h>

int main(void)
{
    int sig = -1;
    ZASSERT(!prctl(PR_GET_PDEATHSIG, &sig, 0UL, 0UL, 0UL));
    printf("pdeathsig %d\n", sig);
    ZASSERT(sig == 0);

    long nnp_before = prctl(PR_GET_NO_NEW_PRIVS, 0UL, 0UL, 0UL, 0UL);

    errno = 0;
    long got = prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, 0UL, 0UL, 0UL);
    if (got >= 0) {
        printf("get speculation ctrl %ld\n", got);
    } else {
        ZASSERT(errno == EINVAL);
        printf("get speculation ctrl unsupported\n");
    }

    errno = 0;
    long setrc = prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, PR_SPEC_DISABLE, 0UL, 0UL);
    if (!setrc) {
        errno = 0;
        long got2 = prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, 0UL, 0UL, 0UL);
        ZASSERT(got2 >= 0);
        ZASSERT(got2 & PR_SPEC_DISABLE);
        printf("set speculation ctrl ok, now %ld\n", got2);
    } else {
        ZASSERT(errno == EINVAL);
        printf("set speculation ctrl unsupported\n");
    }

    long nnp_after = prctl(PR_GET_NO_NEW_PRIVS, 0UL, 0UL, 0UL, 0UL);
    ZASSERT(nnp_before == nnp_after);

    printf("prctl ok\n");
    return 0;
}
