#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    void* special = zweak_map_new();
    socklen_t onlen = sizeof(int);
    getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, special, &onlen);
    return 0;
}