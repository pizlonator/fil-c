#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    struct sockaddr_in* addr = malloc(sizeof(struct sockaddr_in));
    free(opaque(addr));
    connect(fd, (struct sockaddr*)addr, sizeof(struct sockaddr_in));  // Use-after-free
    return 0;
}