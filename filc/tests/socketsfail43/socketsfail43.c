#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    char* p = opaque(malloc(16));
    p -= (uintptr_t)p;  // Zero out address, keep capability
    p += UINTPTR_MAX - 10;  // Near max address
    
    connect(fd, (struct sockaddr*)p, sizeof(struct sockaddr_in));
    return 0;
}