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
    struct open_how how;
    how.flags = O_RDONLY;
    how.mode = 0;
    how.resolve = 0;
    
    syscall(SYS_openat2, AT_FDCWD, NULL, &how, sizeof(how));
    return 0;
}
