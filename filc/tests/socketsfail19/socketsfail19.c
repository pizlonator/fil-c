#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    int* on = malloc(sizeof(int));
    *on = 666;
    socklen_t onlen = sizeof(int);
    free(opaque(on));
    getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, on, &onlen);
    return 0;
}