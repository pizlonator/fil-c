#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/syscall.h>

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

int main()
{
    unlink("filc/test-output/mmapfileb/mmapfile.dat");
    int fd = open("filc/test-output/mmapfileb/mmapfile.dat", O_CREAT | O_RDWR, 0644);
    ASSERT(fd > 2);

    ASSERT(!ftruncate(fd, 16384));
    
    void* memory = syscall(SYS_mmap, NULL, 16384, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    ASSERT(memory);
    ASSERT(memory != (void*)(intptr_t)-1);

    strcpy(memory, "hello, world!\n");

    munmap(memory, 16384);
    close(fd);

    fd = open("filc/test-output/mmapfileb/mmapfile.dat", O_RDONLY);
    ASSERT(fd > 2);
    char buf[100];
    ASSERT(read(fd, buf, 100) == 100);
    ASSERT(!strcmp(buf, "hello, world!\n"));

    return 0;
}

