#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdfil.h>
#include <string.h>
int main(void) {
    ZASSERT(!mlock(NULL, 0));
    ZASSERT(!mlock(main, 0));
    int res = mlock((void*)666, 0);
    int my_errno = errno;
    zprintf("res = %d, error = %s\n", res, strerror(my_errno));
    ZASSERT(res == -1);
    ZASSERT(my_errno == ENOMEM);
    ZASSERT(!mlock(zweak_new(NULL), 0));
    ZASSERT(!munlock(NULL, 0));
    ZASSERT(!munlock(main, 0));
    res = munlock((void*)666, 0);
    my_errno = errno;
    zprintf("res = %d, error = %s\n", res, strerror(my_errno));
    ZASSERT(res == -1);
    ZASSERT(my_errno == ENOMEM);
    ZASSERT(!munlock(zweak_new(NULL), 0));
    return 0;
}
