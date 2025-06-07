#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    struct sockaddr_in* addr = malloc(sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    free(opaque(addr));
    bind(server_fd, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
    return 0;
}