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
    *on = 1;
    free(opaque(on));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, on, sizeof(int));
    return 0;
}