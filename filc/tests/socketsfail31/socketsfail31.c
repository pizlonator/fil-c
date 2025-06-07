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
    socklen_t slen = sizeof(*addr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(0);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    
    ZASSERT(!bind(server_fd, (struct sockaddr*)addr, slen));
    ZASSERT(!listen(server_fd, 5));
    
    free(opaque(addr));
    accept(server_fd, (struct sockaddr*)addr, &slen);  // Use-after-free
    return 0;
}