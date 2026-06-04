#define _GNU_SOURCE

#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

/* Extended open_how with extra bytes */
struct open_how_extended {
    unsigned long long flags;
    unsigned long long mode;
    unsigned long long resolve;
    unsigned long long extra;  /* Extra non-zero bytes to trigger E2BIG */
};

int main()
{
    /* Create a test file */
    const char* testfile = "filc/test-output/openat2e2big/testfile.txt";
    
    int fd = creat(testfile, 0644);
    ZASSERT(fd >= 0);
    ZASSERT(write(fd, "test\n", 5) == 5);
    close(fd);
    
    /* Test E2BIG with extended struct that has non-zero extra bytes */
    struct open_how_extended ext_how;
    memset(&ext_how, 0, sizeof(ext_how));
    ext_how.flags = O_RDONLY;
    ext_how.mode = 0;
    ext_how.resolve = 0;
    ext_how.extra = 1;  /* Non-zero extra byte should cause E2BIG */
    
    int result = syscall(SYS_openat2, AT_FDCWD, testfile, &ext_how, sizeof(ext_how));
    ZASSERT(result == -1);
    if (errno != E2BIG) {
        zprintf("Expected E2BIG (%d), got errno = %d\n", E2BIG, errno);
    }
    ZASSERT(errno == E2BIG);
    
    /* Now test with extended struct but zero extra bytes - should work */
    ext_how.extra = 0;
    result = syscall(SYS_openat2, AT_FDCWD, testfile, &ext_how, sizeof(ext_how));
    if (result < 0) {
        zprintf("Second openat2 failed with result=%d, errno=%d\n", result, errno);
    }
    ZASSERT(result >= 0);
    close(result);
    
    /* Clean up */
    unlink(testfile);
    
    zprintf("openat2e2big: success\n");
    return 0;
}
