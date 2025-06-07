#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    int on = 666;
    socklen_t onlen = 1000;  // Request 1000 bytes from a 4-byte int
    getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, &onlen);
    return 0;
}