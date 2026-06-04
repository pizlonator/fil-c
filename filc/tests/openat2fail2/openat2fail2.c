#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

struct open_how {
    unsigned long long flags;
    unsigned long long mode;
    unsigned long long resolve;
};

int main()
{
    /* Test with NULL how pointer - should trigger filc safety error */
    syscall(SYS_openat2, AT_FDCWD, "/some/path", NULL, sizeof(struct open_how));
    return 0;
}
