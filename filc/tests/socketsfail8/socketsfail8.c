#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    char* p = opaque(malloc(16));
    p -= (uintptr_t)p;  // Zero out address, keep capability
    p += UINTPTR_MAX - 10;  // Near max address
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, p, sizeof(int));
    return 0;
}